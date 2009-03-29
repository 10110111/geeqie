/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: John Ellis, Laurent Monin
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "metadata.h"

#include "cache.h"
#include "exif.h"
#include "filedata.h"
#include "misc.h"
#include "secure_save.h"
#include "ui_fileops.h"
#include "ui_misc.h"
#include "utilops.h"
#include "filefilter.h"
#include "layout.h"
#include "rcfile.h"

typedef enum {
	MK_NONE,
	MK_KEYWORDS,
	MK_COMMENT
} MetadataKey;

static const gchar *group_keys[] = {KEYWORD_KEY, COMMENT_KEY, NULL}; /* tags that will be written to all files in a group */

static gboolean metadata_write_queue_idle_cb(gpointer data);
static gboolean metadata_legacy_write(FileData *fd);
static void metadata_legacy_delete(FileData *fd, const gchar *except);



/*
 *-------------------------------------------------------------------
 * write queue
 *-------------------------------------------------------------------
 */

static GList *metadata_write_queue = NULL;
static gint metadata_write_idle_id = -1;

static void metadata_write_queue_add(FileData *fd)
{
	if (!g_list_find(metadata_write_queue, fd))
		{
		metadata_write_queue = g_list_prepend(metadata_write_queue, fd);
		file_data_ref(fd);
		
		layout_status_update_write_all();
		}

	if (metadata_write_idle_id != -1) 
		{
		g_source_remove(metadata_write_idle_id);
		metadata_write_idle_id = -1;
		}
	
	if (options->metadata.confirm_after_timeout)
		{
		metadata_write_idle_id = g_timeout_add(options->metadata.confirm_timeout * 1000, metadata_write_queue_idle_cb, NULL);
		}
}


gboolean metadata_write_queue_remove(FileData *fd)
{
	g_hash_table_destroy(fd->modified_xmp);
	fd->modified_xmp = NULL;

	metadata_write_queue = g_list_remove(metadata_write_queue, fd);
	
	file_data_increment_version(fd);
	file_data_send_notification(fd, NOTIFY_REREAD);

	file_data_unref(fd);

	layout_status_update_write_all();
	return TRUE;
}

gboolean metadata_write_queue_remove_list(GList *list)
{
	GList *work;
	gboolean ret = TRUE;
	
	work = list;
	while (work)
		{
		FileData *fd = work->data;
		work = work->next;
		ret = ret && metadata_write_queue_remove(fd);
		}
	return ret;
}


gboolean metadata_write_queue_confirm(FileUtilDoneFunc done_func, gpointer done_data)
{
	GList *work;
	GList *to_approve = NULL;
	
	work = metadata_write_queue;
	while (work)
		{
		FileData *fd = work->data;
		work = work->next;
		
		if (fd->change) continue; /* another operation in progress, skip this file for now */
		
		to_approve = g_list_prepend(to_approve, file_data_ref(fd));
		}

	file_util_write_metadata(NULL, to_approve, NULL, done_func, done_data);
	
	filelist_free(to_approve);
	
	return (metadata_write_queue != NULL);
}

static gboolean metadata_write_queue_idle_cb(gpointer data)
{
	metadata_write_queue_confirm(NULL, NULL);
	metadata_write_idle_id = -1;
	return FALSE;
}

gboolean metadata_write_perform(FileData *fd)
{
	gboolean success;
	ExifData *exif;
	
	g_assert(fd->change);
	
	if (fd->change->dest && 
	    strcmp(extension_from_path(fd->change->dest), GQ_CACHE_EXT_METADATA) == 0)
		{
		success = metadata_legacy_write(fd);
		if (success) metadata_legacy_delete(fd, fd->change->dest);
		return success;
		}

	/* write via exiv2 */
	/*  we can either use cached metadata which have fd->modified_xmp already applied 
	                             or read metadata from file and apply fd->modified_xmp
	    metadata are read also if the file was modified meanwhile */
	exif = exif_read_fd(fd); 
	if (!exif) return FALSE;

	success = (fd->change->dest) ? exif_write_sidecar(exif, fd->change->dest) : exif_write(exif); /* write modified metadata */
	exif_free_fd(fd, exif);

	if (fd->change->dest)
		/* this will create a FileData for the sidecar and link it to the main file 
		   (we can't wait until the sidecar is discovered by directory scanning because
		    exif_read_fd is called before that and it would read the main file only and 
		    store the metadata in the cache)
		    FIXME: this does not catch new sidecars created by independent external programs
		*/
		file_data_unref(file_data_new_simple(fd->change->dest)); 
		
	if (success) metadata_legacy_delete(fd, fd->change->dest);
	return success;
}

gint metadata_queue_length(void)
{
	return g_list_length(metadata_write_queue);
}

static gboolean metadata_check_key(const gchar *keys[], const gchar *key)
{
	const gchar **k = keys;
	
	while (*k)
		{
		if (strcmp(key, *k) == 0) return TRUE;
		k++;
		}
	return FALSE;
}

gboolean metadata_write_list(FileData *fd, const gchar *key, const GList *values)
{
	if (!fd->modified_xmp)
		{
		fd->modified_xmp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)string_list_free);
		}
	g_hash_table_insert(fd->modified_xmp, g_strdup(key), string_list_copy((GList *)values));
	if (fd->exif)
		{
		exif_update_metadata(fd->exif, key, values);
		}
	metadata_write_queue_add(fd);
	file_data_increment_version(fd);
	file_data_send_notification(fd, NOTIFY_METADATA);

	if (options->metadata.sync_grouped_files && metadata_check_key(group_keys, key))
		{
		GList *work = fd->sidecar_files;
		
		while (work)
			{
			FileData *sfd = work->data;
			work = work->next;
			
			if (filter_file_class(sfd->extension, FORMAT_CLASS_META)) continue; 

			metadata_write_list(sfd, key, values);
			}
		}


	return TRUE;
}
	
gboolean metadata_write_string(FileData *fd, const gchar *key, const char *value)
{
	GList *list = g_list_append(NULL, g_strdup(value));
	gboolean ret = metadata_write_list(fd, key, list);
	string_list_free(list);
	return ret;
}


/*
 *-------------------------------------------------------------------
 * keyword / comment read/write
 *-------------------------------------------------------------------
 */

static gboolean metadata_file_write(gchar *path, GHashTable *modified_xmp)
{
	SecureSaveInfo *ssi;
	GList *keywords = g_hash_table_lookup(modified_xmp, KEYWORD_KEY);
	GList *comment_l = g_hash_table_lookup(modified_xmp, COMMENT_KEY);
	gchar *comment = comment_l ? comment_l->data : NULL;

	ssi = secure_open(path);
	if (!ssi) return FALSE;

	secure_fprintf(ssi, "#%s comment (%s)\n\n", GQ_APPNAME, VERSION);

	secure_fprintf(ssi, "[keywords]\n");
	while (keywords && secsave_errno == SS_ERR_NONE)
		{
		const gchar *word = keywords->data;
		keywords = keywords->next;

		secure_fprintf(ssi, "%s\n", word);
		}
	secure_fputc(ssi, '\n');

	secure_fprintf(ssi, "[comment]\n");
	secure_fprintf(ssi, "%s\n", (comment) ? comment : "");

	secure_fprintf(ssi, "#end\n");

	return (secure_close(ssi) == 0);
}

static gboolean metadata_legacy_write(FileData *fd)
{
	gboolean success = FALSE;
	gchar *metadata_pathl;

	g_assert(fd->change && fd->change->dest);

	DEBUG_1("Saving comment: %s", fd->change->dest);

	metadata_pathl = path_from_utf8(fd->change->dest);

	success = metadata_file_write(metadata_pathl, fd->modified_xmp);

	g_free(metadata_pathl);

	return success;
}

static gboolean metadata_file_read(gchar *path, GList **keywords, gchar **comment)
{
	FILE *f;
	gchar s_buf[1024];
	MetadataKey key = MK_NONE;
	GList *list = NULL;
	GString *comment_build = NULL;

	f = fopen(path, "r");
	if (!f) return FALSE;

	while (fgets(s_buf, sizeof(s_buf), f))
		{
		gchar *ptr = s_buf;

		if (*ptr == '#') continue;
		if (*ptr == '[' && key != MK_COMMENT)
			{
			gchar *keystr = ++ptr;
			
			key = MK_NONE;
			while (*ptr != ']' && *ptr != '\n' && *ptr != '\0') ptr++;
			
			if (*ptr == ']')
				{
				*ptr = '\0';
				if (g_ascii_strcasecmp(keystr, "keywords") == 0)
					key = MK_KEYWORDS;
				else if (g_ascii_strcasecmp(keystr, "comment") == 0)
					key = MK_COMMENT;
				}
			continue;
			}
		
		switch (key)
			{
			case MK_NONE:
				break;
			case MK_KEYWORDS:
				{
				while (*ptr != '\n' && *ptr != '\0') ptr++;
				*ptr = '\0';
				if (strlen(s_buf) > 0)
					{
					gchar *kw = utf8_validate_or_convert(s_buf);

					list = g_list_prepend(list, kw);
					}
				}
				break;
			case MK_COMMENT:
				if (!comment_build) comment_build = g_string_new("");
				g_string_append(comment_build, s_buf);
				break;
			}
		}
	
	fclose(f);

	if (keywords) 
		{
		*keywords = g_list_reverse(list);
		}
	else
		{
		string_list_free(list);
		}
		
	if (comment_build)
		{
		if (comment)
			{
			gint len;
			gchar *ptr = comment_build->str;

			/* strip leading and trailing newlines */
			while (*ptr == '\n') ptr++;
			len = strlen(ptr);
			while (len > 0 && ptr[len - 1] == '\n') len--;
			if (ptr[len] == '\n') len++; /* keep the last one */
			if (len > 0)
				{
				gchar *text = g_strndup(ptr, len);

				*comment = utf8_validate_or_convert(text);
				g_free(text);
				}
			}
		g_string_free(comment_build, TRUE);
		}

	return TRUE;
}

static void metadata_legacy_delete(FileData *fd, const gchar *except)
{
	gchar *metadata_path;
	gchar *metadata_pathl;
	if (!fd) return;

	metadata_path = cache_find_location(CACHE_TYPE_METADATA, fd->path);
	if (metadata_path && (!except || strcmp(metadata_path, except) != 0)) 
		{
		metadata_pathl = path_from_utf8(metadata_path);
		unlink(metadata_pathl);
		g_free(metadata_pathl);
		g_free(metadata_path);
		}
	metadata_path = cache_find_location(CACHE_TYPE_XMP_METADATA, fd->path);
	if (metadata_path && (!except || strcmp(metadata_path, except) != 0)) 
		{
		metadata_pathl = path_from_utf8(metadata_path);
		unlink(metadata_pathl);
		g_free(metadata_pathl);
		g_free(metadata_path);
		}
}

static gboolean metadata_legacy_read(FileData *fd, GList **keywords, gchar **comment)
{
	gchar *metadata_path;
	gchar *metadata_pathl;
	gboolean success = FALSE;
	
	if (!fd) return FALSE;

	metadata_path = cache_find_location(CACHE_TYPE_METADATA, fd->path);
	if (!metadata_path) return FALSE;

	metadata_pathl = path_from_utf8(metadata_path);

	success = metadata_file_read(metadata_pathl, keywords, comment);

	g_free(metadata_pathl);
	g_free(metadata_path);

	return success;
}

static GList *remove_duplicate_strings_from_list(GList *list)
{
	GList *work = list;
	GHashTable *hashtable = g_hash_table_new(g_str_hash, g_str_equal);
	GList *newlist = NULL;

	while (work)
		{
		gchar *key = work->data;

		if (g_hash_table_lookup(hashtable, key) == NULL)
			{
			g_hash_table_insert(hashtable, (gpointer) key, GINT_TO_POINTER(1));
			newlist = g_list_prepend(newlist, key);
			}
		work = work->next;
		}

	g_hash_table_destroy(hashtable);
	g_list_free(list);

	return g_list_reverse(newlist);
}

GList *metadata_read_list(FileData *fd, const gchar *key, MetadataFormat format)
{
	ExifData *exif;
	GList *list = NULL;
	if (!fd) return NULL;

	/* unwritten data overide everything */
	if (fd->modified_xmp && format == METADATA_PLAIN)
		{
	        list = g_hash_table_lookup(fd->modified_xmp, key);
		if (list) return string_list_copy(list);
		}

	/* 
	    Legacy metadata file is the primary source if it exists.
	    Merging the lists does not make much sense, because the existence of
	    legacy metadata file indicates that the other metadata sources are not
	    writable and thus it would not be possible to delete the keywords
	    that comes from the image file.
	*/
	if (strcmp(key, KEYWORD_KEY) == 0)
		{
	        if (metadata_legacy_read(fd, &list, NULL)) return list;
	        }

	if (strcmp(key, COMMENT_KEY) == 0)
		{
		gchar *comment = NULL;
	        if (metadata_legacy_read(fd, NULL, &comment)) return g_list_append(NULL, comment);
	        }
	
	if (strncmp(key, "file.", 5) == 0)
		{
	        return g_list_append(NULL, metadata_file_info(fd, key, format));
		}
	
	exif = exif_read_fd(fd); /* this is cached, thus inexpensive */
	if (!exif) return NULL;
	list = exif_get_metadata(exif, key, format);
	exif_free_fd(fd, exif);
	return list;
}

gchar *metadata_read_string(FileData *fd, const gchar *key, MetadataFormat format)
{
	GList *string_list = metadata_read_list(fd, key, format);
	if (string_list)
		{
		gchar *str = string_list->data;
		string_list->data = NULL;
		string_list_free(string_list);
		return str;
		}
	return NULL;
}

guint64 metadata_read_int(FileData *fd, const gchar *key, guint64 fallback)
{
	guint64 ret;
	gchar *endptr;
	gchar *string = metadata_read_string(fd, key, METADATA_PLAIN);
	if (!string) return fallback;
	
	ret = g_ascii_strtoull(string, &endptr, 10);
	if (string == endptr) ret = fallback;
	g_free(string);
	return ret;
}
	
gboolean metadata_append_string(FileData *fd, const gchar *key, const char *value)
{
	gchar *str = metadata_read_string(fd, key, METADATA_PLAIN);
	
	if (!str) 
		{
		return metadata_write_string(fd, key, value);
		}
	else
		{
		gchar *new_string = g_strconcat(str, value, NULL);
		gboolean ret = metadata_write_string(fd, key, new_string);
		g_free(str);
		g_free(new_string);
		return ret;
		}
}

gboolean metadata_append_list(FileData *fd, const gchar *key, const GList *values)
{
	GList *list = metadata_read_list(fd, key, METADATA_PLAIN);
	
	if (!list) 
		{
		return metadata_write_list(fd, key, values);
		}
	else
		{
		gboolean ret;
		list = g_list_concat(list, string_list_copy(values));
		list = remove_duplicate_strings_from_list(list);
		
		ret = metadata_write_list(fd, key, list);
		string_list_free(list);
		return ret;
		}
}

gchar *find_string_in_list_utf8nocase(GList *list, const gchar *string)
{
	gchar *string_casefold = g_utf8_casefold(string, -1);

	while (list)
		{
		gchar *haystack = list->data;
		
		if (haystack)
			{
			gboolean equal;
			gchar *haystack_casefold = g_utf8_casefold(haystack, -1);

			equal = (strcmp(haystack_casefold, string_casefold) == 0);
			g_free(haystack_casefold);

			if (equal)
				{
				g_free(string_casefold);
				return haystack;
				}
			}
	
		list = list->next;
		}
	
	g_free(string_casefold);
	return NULL;
}


#define KEYWORDS_SEPARATOR(c) ((c) == ',' || (c) == ';' || (c) == '\n' || (c) == '\r' || (c) == '\b')

GList *string_to_keywords_list(const gchar *text)
{
	GList *list = NULL;
	const gchar *ptr = text;

	while (*ptr != '\0')
		{
		const gchar *begin;
		gint l = 0;

		while (KEYWORDS_SEPARATOR(*ptr)) ptr++;
		begin = ptr;
		while (*ptr != '\0' && !KEYWORDS_SEPARATOR(*ptr))
			{
			ptr++;
			l++;
			}

		/* trim starting and ending whitespaces */
		while (l > 0 && g_ascii_isspace(*begin)) begin++, l--;
		while (l > 0 && g_ascii_isspace(begin[l-1])) l--;

		if (l > 0)
			{
			gchar *keyword = g_strndup(begin, l);

			/* only add if not already in the list */
			if (!find_string_in_list_utf8nocase(list, keyword))
				list = g_list_append(list, keyword);
			else
				g_free(keyword);
			}
		}

	return list;
}

/*
 * keywords to marks
 */
 

gboolean meta_data_get_keyword_mark(FileData *fd, gint n, gpointer data)
{
	/* FIXME: do not use global keyword_tree */
	GList *path = data;
	GList *keywords;
	gboolean found = FALSE;
	keywords = metadata_read_list(fd, KEYWORD_KEY, METADATA_PLAIN);
	if (keywords)
		{
		GtkTreeIter iter;
		if (keyword_tree_get_iter(GTK_TREE_MODEL(keyword_tree), &iter, path) &&
		    keyword_tree_is_set(GTK_TREE_MODEL(keyword_tree), &iter, keywords))
			found = TRUE;

		}
	return found;
}

gboolean meta_data_set_keyword_mark(FileData *fd, gint n, gboolean value, gpointer data)
{
	GList *path = data;
	GList *keywords = NULL;
	GtkTreeIter iter;
	
	if (!keyword_tree_get_iter(GTK_TREE_MODEL(keyword_tree), &iter, path)) return FALSE;

	keywords = metadata_read_list(fd, KEYWORD_KEY, METADATA_PLAIN);

	if (!!keyword_tree_is_set(GTK_TREE_MODEL(keyword_tree), &iter, keywords) != !!value)
		{
		if (value) 
			{
			keyword_tree_set(GTK_TREE_MODEL(keyword_tree), &iter, &keywords);
			}
		else
			{
			keyword_tree_reset(GTK_TREE_MODEL(keyword_tree), &iter, &keywords);
			}
		metadata_write_list(fd, KEYWORD_KEY, keywords);
		}

	string_list_free(keywords);
	return TRUE;
}



void meta_data_connect_mark_with_keyword(GtkTreeModel *keyword_tree, GtkTreeIter *kw_iter, gint mark)
{

	FileDataGetMarkFunc get_mark_func;
	FileDataSetMarkFunc set_mark_func;
	gpointer mark_func_data;

	gint i;

	for (i = 0; i < FILEDATA_MARKS_SIZE; i++)
		{
		file_data_get_registered_mark_func(i, &get_mark_func, &set_mark_func, &mark_func_data);
		if (get_mark_func == meta_data_get_keyword_mark) 
			{
			GtkTreeIter old_kw_iter;
			GList *old_path = mark_func_data;
			
			if (keyword_tree_get_iter(keyword_tree, &old_kw_iter, old_path) && 
			    (i == mark || /* release any previous connection of given mark */
			     keyword_compare(keyword_tree, &old_kw_iter, kw_iter) == 0)) /* or given keyword */
				{
				file_data_register_mark_func(i, NULL, NULL, NULL, NULL);
				gtk_tree_store_set(GTK_TREE_STORE(keyword_tree), &old_kw_iter, KEYWORD_COLUMN_MARK, "", -1);
				}
			}
		}


	if (mark >= 0 && mark < FILEDATA_MARKS_SIZE)
		{
		GList *path;
		gchar *mark_str;
		path = keyword_tree_get_path(keyword_tree, kw_iter);
		file_data_register_mark_func(mark, meta_data_get_keyword_mark, meta_data_set_keyword_mark, path, (GDestroyNotify)string_list_free);
		
		mark_str = g_strdup_printf("%d", mark + 1);
		gtk_tree_store_set(GTK_TREE_STORE(keyword_tree), kw_iter, KEYWORD_COLUMN_MARK, mark_str, -1);
		g_free(mark_str);
		}
}


/*
 *-------------------------------------------------------------------
 * keyword tree
 *-------------------------------------------------------------------
 */



GtkTreeStore *keyword_tree;

gchar *keyword_get_name(GtkTreeModel *keyword_tree, GtkTreeIter *iter)
{
	gchar *name;
	gtk_tree_model_get(keyword_tree, iter, KEYWORD_COLUMN_NAME, &name, -1);
	return name;
}

gchar *keyword_get_casefold(GtkTreeModel *keyword_tree, GtkTreeIter *iter)
{
	gchar *casefold;
	gtk_tree_model_get(keyword_tree, iter, KEYWORD_COLUMN_CASEFOLD, &casefold, -1);
	return casefold;
}

gboolean keyword_get_is_keyword(GtkTreeModel *keyword_tree, GtkTreeIter *iter)
{
	gboolean is_keyword;
	gtk_tree_model_get(keyword_tree, iter, KEYWORD_COLUMN_IS_KEYWORD, &is_keyword, -1);
	return is_keyword;
}

void keyword_set(GtkTreeStore *keyword_tree, GtkTreeIter *iter, const gchar *name, gboolean is_keyword)
{
	gchar *casefold = g_utf8_casefold(name, -1);
	gtk_tree_store_set(keyword_tree, iter, KEYWORD_COLUMN_MARK, "",
						KEYWORD_COLUMN_NAME, name,
						KEYWORD_COLUMN_CASEFOLD, casefold,
						KEYWORD_COLUMN_IS_KEYWORD, is_keyword, -1);
	g_free(casefold);
}

gboolean keyword_compare(GtkTreeModel *keyword_tree, GtkTreeIter *a, GtkTreeIter *b)
{
	GtkTreePath *pa = gtk_tree_model_get_path(keyword_tree, a);
	GtkTreePath *pb = gtk_tree_model_get_path(keyword_tree, b);
	gint ret = gtk_tree_path_compare(pa, pb);
	gtk_tree_path_free(pa);
	gtk_tree_path_free(pb);
	return ret;
}

gboolean keyword_same_parent(GtkTreeModel *keyword_tree, GtkTreeIter *a, GtkTreeIter *b)
{
	GtkTreeIter parent_a;
	GtkTreeIter parent_b;
	
	gboolean valid_pa = gtk_tree_model_iter_parent(keyword_tree, &parent_a, a);
	gboolean valid_pb = gtk_tree_model_iter_parent(keyword_tree, &parent_b, b);

	if (valid_pa && valid_pb)
		{
		return keyword_compare(keyword_tree, &parent_a, &parent_b) == 0;
		}
	else
		{
		return (!valid_pa && !valid_pb); /* both are toplevel */
		}
}

gboolean keyword_exists(GtkTreeModel *keyword_tree, GtkTreeIter *parent_ptr, GtkTreeIter *sibling, const gchar *name, gboolean exclude_sibling, GtkTreeIter *result)
{
	GtkTreeIter parent;
	GtkTreeIter iter;
	gboolean toplevel = FALSE;
	gboolean ret;
	gchar *casefold;
	
	if (parent_ptr)
		{
		parent = *parent_ptr;
		}
	else if (sibling)
		{
		toplevel = !gtk_tree_model_iter_parent(keyword_tree, &parent, sibling);
		}
	else
		{
		toplevel = TRUE;
		}
	
	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(keyword_tree), &iter, toplevel ? NULL : &parent)) return FALSE;
	
	casefold = g_utf8_casefold(name, -1);
	ret = FALSE;
	
	while (TRUE)
		{
		if (!(exclude_sibling && sibling && keyword_compare(keyword_tree, &iter, sibling) == 0))
			{
			gchar *iter_casefold = keyword_get_casefold(keyword_tree, &iter);
			ret = strcmp(casefold, iter_casefold) == 0;
			g_free(iter_casefold);
			}
		if (ret) 
			{
			if (result) *result = iter;
			break;
			}
		if (!gtk_tree_model_iter_next(keyword_tree, &iter)) break;
		}
	g_free(casefold);
	return ret;
}


void keyword_copy(GtkTreeStore *keyword_tree, GtkTreeIter *to, GtkTreeIter *from)
{

	gchar *mark, *name, *casefold;
	gboolean is_keyword;

	/* do not copy KEYWORD_COLUMN_HIDE_IN, it fully shows the new subtree */
	gtk_tree_model_get(GTK_TREE_MODEL(keyword_tree), from, KEYWORD_COLUMN_MARK, &mark,
						KEYWORD_COLUMN_NAME, &name,
						KEYWORD_COLUMN_CASEFOLD, &casefold,
						KEYWORD_COLUMN_IS_KEYWORD, &is_keyword, -1);

	gtk_tree_store_set(keyword_tree, to, KEYWORD_COLUMN_MARK, mark,
						KEYWORD_COLUMN_NAME, name,
						KEYWORD_COLUMN_CASEFOLD, casefold,
						KEYWORD_COLUMN_IS_KEYWORD, is_keyword, -1);
	g_free(mark);
	g_free(name);
	g_free(casefold);
}

void keyword_copy_recursive(GtkTreeStore *keyword_tree, GtkTreeIter *to, GtkTreeIter *from)
{
	GtkTreeIter from_child;
	
	keyword_copy(keyword_tree, to, from);
	
	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(keyword_tree), &from_child, from)) return;
	
	while (TRUE)
		{
		GtkTreeIter to_child;
		gtk_tree_store_append(keyword_tree, &to_child, to);
		keyword_copy_recursive(keyword_tree, &to_child, &from_child);
		if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(keyword_tree), &from_child)) return;
		}
}

void keyword_move_recursive(GtkTreeStore *keyword_tree, GtkTreeIter *to, GtkTreeIter *from)
{
	keyword_copy_recursive(keyword_tree, to, from);
	keyword_delete(keyword_tree, from);
}

GList *keyword_tree_get_path(GtkTreeModel *keyword_tree, GtkTreeIter *iter_ptr)
{
	GList *path = NULL;
	GtkTreeIter iter = *iter_ptr;
	
	while (TRUE)
		{
		GtkTreeIter parent;
		path = g_list_prepend(path, keyword_get_name(keyword_tree, &iter));
		if (!gtk_tree_model_iter_parent(keyword_tree, &parent, &iter)) break;
		iter = parent;
		}
	return path;
}

gboolean keyword_tree_get_iter(GtkTreeModel *keyword_tree, GtkTreeIter *iter_ptr, GList *path)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter_first(keyword_tree, &iter)) return FALSE;
	
	while (TRUE)
		{
		GtkTreeIter children;
		while (TRUE)
			{
			gchar *name = keyword_get_name(keyword_tree, &iter);
			if (strcmp(name, path->data) == 0) break;
			g_free(name);
			if (!gtk_tree_model_iter_next(keyword_tree, &iter)) return FALSE;
			}
		path = path->next;
		if (!path) 
			{
			*iter_ptr = iter;
			return TRUE;
			}
			
	    	if (!gtk_tree_model_iter_children(keyword_tree, &children, &iter)) return FALSE;
	    	iter = children;
		}
}


static gboolean keyword_tree_is_set_casefold(GtkTreeModel *keyword_tree, GtkTreeIter iter, GList *casefold_list)
{
	if (!casefold_list) return FALSE;

	if (!keyword_get_is_keyword(keyword_tree, &iter))
		{
		/* for the purpose of expanding and hiding, a helper is set if it has any children set */
		GtkTreeIter child;
		if (!gtk_tree_model_iter_children(keyword_tree, &child, &iter)) 
			return FALSE; /* this should happen only on empty helpers */

		while (TRUE)
			{
			if (keyword_tree_is_set_casefold(keyword_tree, child, casefold_list)) return TRUE;
			if (!gtk_tree_model_iter_next(keyword_tree, &child)) return FALSE;
			}
		}
	
	while (TRUE)
		{
		GtkTreeIter parent;

		if (keyword_get_is_keyword(keyword_tree, &iter))
			{
			GList *work = casefold_list;
			gboolean found = FALSE;
			gchar *iter_casefold = keyword_get_casefold(keyword_tree, &iter);
			while (work)
				{
				const gchar *casefold = work->data;
				work = work->next;

				if (strcmp(iter_casefold, casefold) == 0)
					{
					found = TRUE;
					break;
					}
				}
			g_free(iter_casefold);
			if (!found) return FALSE;
			}
		
		if (!gtk_tree_model_iter_parent(keyword_tree, &parent, &iter)) return TRUE;
		iter = parent;
		}
}

gboolean keyword_tree_is_set(GtkTreeModel *keyword_tree, GtkTreeIter *iter, GList *kw_list)
{
	gboolean ret;
	GList *casefold_list = NULL;
	GList *work;

	work = kw_list;
	while (work)
		{
		const gchar *kw = work->data;
		work = work->next;

		casefold_list = g_list_prepend(casefold_list, g_utf8_casefold(kw, -1));
		}
	
	ret = keyword_tree_is_set_casefold(keyword_tree, *iter, casefold_list);
	
	string_list_free(casefold_list);
	return ret;
}

void keyword_tree_set(GtkTreeModel *keyword_tree, GtkTreeIter *iter_ptr, GList **kw_list)
{
	GtkTreeIter iter = *iter_ptr;
	while (TRUE)
		{
		GtkTreeIter parent;

		if (keyword_get_is_keyword(keyword_tree, &iter))
			{
			gchar *name = keyword_get_name(keyword_tree, &iter);
			if (!find_string_in_list_utf8nocase(*kw_list, name))
				{
				*kw_list = g_list_append(*kw_list, name);
				}
			else
				{
				g_free(name);
				}
			}

		if (!gtk_tree_model_iter_parent(keyword_tree, &parent, &iter)) return;
		iter = parent;
		}
}

static void keyword_tree_reset1(GtkTreeModel *keyword_tree, GtkTreeIter *iter, GList **kw_list)
{
	gchar *found;
	gchar *name;
	if (!keyword_get_is_keyword(keyword_tree, iter)) return;

	name = keyword_get_name(keyword_tree, iter);
	found = find_string_in_list_utf8nocase(*kw_list, name);

	if (found)
		{
		*kw_list = g_list_remove(*kw_list, found);
		g_free(found);
		}
	g_free(name);
}

static void keyword_tree_reset_recursive(GtkTreeModel *keyword_tree, GtkTreeIter *iter, GList **kw_list)
{
	GtkTreeIter child;
	keyword_tree_reset1(keyword_tree, iter, kw_list);
	
	if (!gtk_tree_model_iter_children(keyword_tree, &child, iter)) return;

	while (TRUE)
		{
		keyword_tree_reset_recursive(keyword_tree, &child, kw_list);
		if (!gtk_tree_model_iter_next(keyword_tree, &child)) return;
		}
}

static gboolean keyword_tree_check_empty_children(GtkTreeModel *keyword_tree, GtkTreeIter *parent, GList *kw_list)
{
	GtkTreeIter iter;
	
	if (!gtk_tree_model_iter_children(keyword_tree, &iter, parent)) 
		return TRUE; /* this should happen only on empty helpers */

	while (TRUE)
		{
		if (keyword_tree_is_set(keyword_tree, &iter, kw_list)) return FALSE;
		if (!gtk_tree_model_iter_next(keyword_tree, &iter)) return TRUE;
		}
}

void keyword_tree_reset(GtkTreeModel *keyword_tree, GtkTreeIter *iter_ptr, GList **kw_list)
{
	GtkTreeIter iter = *iter_ptr;
	GtkTreeIter parent;
	keyword_tree_reset_recursive(keyword_tree, &iter, kw_list);

	if (!gtk_tree_model_iter_parent(keyword_tree, &parent, &iter)) return;
	iter = parent;
	
	while (keyword_tree_check_empty_children(keyword_tree, &iter, *kw_list))
		{
		GtkTreeIter parent;
		keyword_tree_reset1(keyword_tree, &iter, kw_list);
		if (!gtk_tree_model_iter_parent(keyword_tree, &parent, &iter)) return;
		iter = parent;
		}
}

void keyword_delete(GtkTreeStore *keyword_tree, GtkTreeIter *iter_ptr)
{
	GList *list;
	GtkTreeIter child;
	while (gtk_tree_model_iter_children(GTK_TREE_MODEL(keyword_tree), &child, iter_ptr))
		{
		keyword_delete(keyword_tree, &child);
		}
	
	meta_data_connect_mark_with_keyword(GTK_TREE_MODEL(keyword_tree), iter_ptr, -1);

	gtk_tree_model_get(GTK_TREE_MODEL(keyword_tree), iter_ptr, KEYWORD_COLUMN_HIDE_IN, &list, -1);
	g_list_free(list);
	
	gtk_tree_store_remove(keyword_tree, iter_ptr);
}


void keyword_hide_in(GtkTreeStore *keyword_tree, GtkTreeIter *iter, gpointer id)
{
	GList *list;
	gtk_tree_model_get(GTK_TREE_MODEL(keyword_tree), iter, KEYWORD_COLUMN_HIDE_IN, &list, -1);
	if (!g_list_find(list, id))
		{
		list = g_list_prepend(list, id);
		gtk_tree_store_set(keyword_tree, iter, KEYWORD_COLUMN_HIDE_IN, list, -1);
		}
}

void keyword_show_in(GtkTreeStore *keyword_tree, GtkTreeIter *iter, gpointer id)
{
	GList *list;
	gtk_tree_model_get(GTK_TREE_MODEL(keyword_tree), iter, KEYWORD_COLUMN_HIDE_IN, &list, -1);
	list = g_list_remove(list, id);
	gtk_tree_store_set(keyword_tree, iter, KEYWORD_COLUMN_HIDE_IN, list, -1);
}

gboolean keyword_is_hidden_in(GtkTreeModel *keyword_tree, GtkTreeIter *iter, gpointer id)
{
	GList *list;
	gtk_tree_model_get(keyword_tree, iter, KEYWORD_COLUMN_HIDE_IN, &list, -1);
	return !!g_list_find(list, id);
}

static gboolean keyword_show_all_in_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	keyword_show_in(GTK_TREE_STORE(model), iter, data);
	return FALSE;
}

void keyword_show_all_in(GtkTreeStore *keyword_tree, gpointer id)
{
	gtk_tree_model_foreach(GTK_TREE_MODEL(keyword_tree), keyword_show_all_in_cb, id);
}

static void keyword_hide_unset_in_recursive(GtkTreeStore *keyword_tree, GtkTreeIter *iter_ptr, gpointer id, GList *keywords)
{
	GtkTreeIter iter = *iter_ptr;
	while (TRUE)
		{
		if (!keyword_tree_is_set(GTK_TREE_MODEL(keyword_tree), &iter, keywords))
			{
			keyword_hide_in(keyword_tree, &iter, id);
			/* no need to check children of hidden node */
			}
		else
			{
			GtkTreeIter child;
			if (gtk_tree_model_iter_children(GTK_TREE_MODEL(keyword_tree), &child, &iter)) 
				{
				keyword_hide_unset_in_recursive(keyword_tree, &child, id, keywords);
				}
			}
		if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(keyword_tree), &iter)) return;
		}
}

void keyword_hide_unset_in(GtkTreeStore *keyword_tree, gpointer id, GList *keywords)
{
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(keyword_tree), &iter)) return;
	keyword_hide_unset_in_recursive(keyword_tree, &iter, id, keywords);
}

static gboolean keyword_show_set_in_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter_ptr, gpointer data)
{
	GtkTreeIter iter = *iter_ptr;
	GList *keywords = data;
	gpointer id = keywords->data;
	keywords = keywords->next; /* hack */
	if (keyword_tree_is_set(model, &iter, keywords))
		{
		while (TRUE)
			{
			GtkTreeIter parent;
			keyword_show_in(GTK_TREE_STORE(model), &iter, id);
			if (!gtk_tree_model_iter_parent(GTK_TREE_MODEL(keyword_tree), &parent, &iter)) break;
			iter = parent;
			}
		}
	return FALSE;
}

void keyword_show_set_in(GtkTreeStore *keyword_tree, gpointer id, GList *keywords)
{
	/* hack: pass id to keyword_hide_unset_in_cb in the list */
	keywords = g_list_prepend(keywords, id);
	gtk_tree_model_foreach(GTK_TREE_MODEL(keyword_tree), keyword_show_set_in_cb, keywords);
	keywords = g_list_delete_link(keywords, keywords);
}


void keyword_tree_new(void)
{
	if (keyword_tree) return;
	
	keyword_tree = gtk_tree_store_new(KEYWORD_COLUMN_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);
}


void keyword_tree_new_default(void)
{
	if (keyword_tree) return;
	
	keyword_tree_new();

	GtkTreeIter i1, i2, i3;

	gtk_tree_store_append(keyword_tree, &i1, NULL);
	keyword_set(keyword_tree, &i1, "animal", TRUE);

		gtk_tree_store_append(keyword_tree, &i2, &i1);
		keyword_set(keyword_tree, &i2, "mammal", TRUE);

			gtk_tree_store_append(keyword_tree, &i3, &i2);
			keyword_set(keyword_tree, &i3, "dog", TRUE);

			gtk_tree_store_append(keyword_tree, &i3, &i2);
			keyword_set(keyword_tree, &i3, "cat", TRUE);

		gtk_tree_store_append(keyword_tree, &i2, &i1);
		keyword_set(keyword_tree, &i2, "insect", TRUE);

			gtk_tree_store_append(keyword_tree, &i3, &i2);
			keyword_set(keyword_tree, &i3, "fly", TRUE);

			gtk_tree_store_append(keyword_tree, &i3, &i2);
			keyword_set(keyword_tree, &i3, "dragonfly", TRUE);

	gtk_tree_store_append(keyword_tree, &i1, NULL);
	keyword_set(keyword_tree, &i1, "daytime", FALSE);

		gtk_tree_store_append(keyword_tree, &i2, &i1);
		keyword_set(keyword_tree, &i2, "morning", TRUE);

		gtk_tree_store_append(keyword_tree, &i2, &i1);
		keyword_set(keyword_tree, &i2, "noon", TRUE);

}


static void keyword_tree_node_write_config(GtkTreeModel *keyword_tree, GtkTreeIter *iter_ptr, GString *outstr, gint indent)
{
	GtkTreeIter iter = *iter_ptr;
	while (TRUE)
		{
		GtkTreeIter children;
		gchar *name;

		WRITE_NL(); WRITE_STRING("<keyword ");
		name = keyword_get_name(keyword_tree, &iter);
		write_char_option(outstr, indent, "name", name);
		g_free(name);
		write_bool_option(outstr, indent, "kw", keyword_get_is_keyword(keyword_tree, &iter));
		if (gtk_tree_model_iter_children(keyword_tree, &children, &iter)) 
			{
			WRITE_STRING(">");
			indent++;
			keyword_tree_node_write_config(keyword_tree, &children, outstr, indent);
			indent--;
			WRITE_NL(); WRITE_STRING("</keyword>");
			}
		else 
			{
			WRITE_STRING("/>");
			}
		if (!gtk_tree_model_iter_next(keyword_tree, &iter)) return;
		}
}

void keyword_tree_write_config(GString *outstr, gint indent)
{
	GtkTreeIter iter;
	WRITE_NL(); WRITE_STRING("<keyword_tree>");
	indent++;
	
	if (keyword_tree && gtk_tree_model_get_iter_first(GTK_TREE_MODEL(keyword_tree), &iter))
		{
		keyword_tree_node_write_config(GTK_TREE_MODEL(keyword_tree), &iter, outstr, indent);
		}
	indent--;
	WRITE_NL(); WRITE_STRING("</keyword_tree>");
}

GtkTreeIter *keyword_add_from_config(GtkTreeStore *keyword_tree, GtkTreeIter *parent, const gchar **attribute_names, const gchar **attribute_values)
{
	gchar *name = NULL;
	gboolean is_kw = TRUE;

	while (*attribute_names)
		{
		const gchar *option = *attribute_names++;
		const gchar *value = *attribute_values++;

		if (READ_CHAR_FULL("name", name)) continue;
		if (READ_BOOL_FULL("kw", is_kw)) continue;

		log_printf("unknown attribute %s = %s\n", option, value);
		}
	if (name && name[0]) 
		{
		GtkTreeIter iter;
		/* re-use existing keyword if any */
		if (!keyword_exists(GTK_TREE_MODEL(keyword_tree), parent, NULL, name, FALSE, &iter))
			{
			gtk_tree_store_append(keyword_tree, &iter, parent);
			}
		keyword_set(keyword_tree, &iter, name, is_kw);
		g_free(name);
		return gtk_tree_iter_copy(&iter);
		}
	g_free(name);
	return NULL;
}

/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
