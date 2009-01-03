/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 The Geeqie Team
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

typedef enum {
	MK_NONE,
	MK_KEYWORDS,
	MK_COMMENT
} MetadataKey;

static const gchar *group_keys[] = {KEYWORD_KEY, COMMENT_KEY, NULL}; /* tags that will be written to all files in a group */

static gboolean metadata_write_queue_idle_cb(gpointer data);
static gint metadata_legacy_write(FileData *fd);
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
		}

	if (metadata_write_idle_id != -1) 
		{
		g_source_remove(metadata_write_idle_id);
		metadata_write_idle_id = -1;
		}
	
	if (options->metadata.confirm_timeout > 0)
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
	file_data_send_notification(fd, NOTIFY_TYPE_REREAD);

	file_data_unref(fd);
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

	if (success) metadata_legacy_delete(fd, fd->change->dest);
	return success;
}

static gboolean metadata_check_key(const gchar *keys[], const gchar *key)
{
	const gchar **k = keys;
	
	while (k)
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
	file_data_send_notification(fd, NOTIFY_TYPE_INTERNAL);

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

static gint metadata_file_write(gchar *path, GHashTable *modified_xmp)
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

static gint metadata_legacy_write(FileData *fd)
{
	gint success = FALSE;

	g_assert(fd->change && fd->change->dest);
	gchar *metadata_pathl;

	DEBUG_1("Saving comment: %s", fd->change->dest);

	metadata_pathl = path_from_utf8(fd->change->dest);

	success = metadata_file_write(metadata_pathl, fd->modified_xmp);

	g_free(metadata_pathl);

	return success;
}

static gint metadata_file_read(gchar *path, GList **keywords, gchar **comment)
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
		
		switch(key)
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

	*keywords = g_list_reverse(list);
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

static gint metadata_legacy_read(FileData *fd, GList **keywords, gchar **comment)
{
	gchar *metadata_path;
	gchar *metadata_pathl;
	gint success = FALSE;
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


static gint metadata_xmp_read(FileData *fd, GList **keywords, gchar **comment)
{
	ExifData *exif;

	exif = exif_read_fd(fd);
	if (!exif) return FALSE;

	if (comment)
		{
		gchar *text;
		ExifItem *item = exif_get_item(exif, COMMENT_KEY);

		text = exif_item_get_string(item, 0);
		*comment = utf8_validate_or_convert(text);
		g_free(text);
		}

	if (keywords)
		{
		ExifItem *item;
		guint i;
		
		*keywords = NULL;
		item = exif_get_item(exif, KEYWORD_KEY);
		for (i = 0; i < exif_item_get_elements(item); i++)
			{
			gchar *kw = exif_item_get_string(item, i);
			gchar *utf8_kw;

			if (!kw) break;

			utf8_kw = utf8_validate_or_convert(kw);
			*keywords = g_list_append(*keywords, (gpointer) utf8_kw);
			g_free(kw);
			}

		/* FIXME:
		 * Exiv2 handles Iptc keywords as multiple entries with the
		 * same key, thus exif_get_item returns only the first keyword
		 * and the only way to get all keywords is to iterate through
		 * the item list.
		 */
		 /* Read IPTC keywords only if there are no XMP keywords
		  * IPTC does not have standard charset, thus the encoding may differ
		  * from XMP and keyword merging is not reliable.
		  */
		 if (!*keywords)
			{
			for (item = exif_get_first_item(exif);
			     item;
			     item = exif_get_next_item(exif))
				{
				guint tag;
			
				tag = exif_item_get_tag_id(item);
				if (tag == 0x0019)
					{
					gchar *tag_name = exif_item_get_tag_name(item);
        
					if (strcmp(tag_name, "Iptc.Application2.Keywords") == 0)
						{
						gchar *kw;
						gchar *utf8_kw;
        
						kw = exif_item_get_data_as_text(item);
						if (!kw) continue;
        
						utf8_kw = utf8_validate_or_convert(kw);
						*keywords = g_list_append(*keywords, (gpointer) utf8_kw);
						g_free(kw);
						}
					g_free(tag_name);
					}
				}
			}
		}

	exif_free_fd(fd, exif);

	return (comment && *comment) || (keywords && *keywords);
}

gint metadata_read(FileData *fd, GList **keywords, gchar **comment)
{
	GList *keywords_xmp = NULL;
	GList *keywords_legacy = NULL;
	gchar *comment_xmp = NULL;
	gchar *comment_legacy = NULL;
	gint result_xmp, result_legacy;

	if (!fd) return FALSE;

	result_xmp = metadata_xmp_read(fd, &keywords_xmp, &comment_xmp);
	result_legacy = metadata_legacy_read(fd, &keywords_legacy, &comment_legacy);

	if (!result_xmp && !result_legacy)
		{
		return FALSE;
		}

	if (keywords)
		{
		if (result_xmp && result_legacy)
			*keywords = g_list_concat(keywords_xmp, keywords_legacy);
		else
			*keywords = result_xmp ? keywords_xmp : keywords_legacy;

		*keywords = remove_duplicate_strings_from_list(*keywords);
		}
	else
		{
		if (result_xmp) string_list_free(keywords_xmp);
		if (result_legacy) string_list_free(keywords_legacy);
		}


	if (comment)
		{
		if (result_xmp && result_legacy && comment_xmp && comment_legacy && *comment_xmp && *comment_legacy)
			*comment = g_strdup_printf("%s\n%s", comment_xmp, comment_legacy);
		else
			*comment = result_xmp ? comment_xmp : comment_legacy;
		}

	if (result_xmp && (!comment || *comment != comment_xmp)) g_free(comment_xmp);
	if (result_legacy && (!comment || *comment != comment_legacy)) g_free(comment_legacy);
	
	// return FALSE in the following cases:
	//  - only looking for a comment and didn't find one
	//  - only looking for keywords and didn't find any
	//  - looking for either a comment or keywords, but found nothing
	if ((!keywords && comment   && !*comment)  ||
	    (!comment  && keywords  && !*keywords) ||
	    ( comment  && !*comment &&   keywords && !*keywords))
		return FALSE;

	return TRUE;
}

void metadata_set(FileData *fd, GList *new_keywords, gchar *new_comment, gboolean append)
{
	gchar *comment = NULL;
	GList *keywords = NULL;
	GList *keywords_list = NULL;

	metadata_read(fd, &keywords, &comment);
	
	if (new_comment)
		{
		if (append && comment && *comment)
			{
			gchar *tmp = comment;
				
			comment = g_strconcat(tmp, new_comment, NULL);
			g_free(tmp);
			}
		else
			{
			g_free(comment);
			comment = g_strdup(new_comment);
			}
		}
	
	if (new_keywords)
		{
		if (append && keywords && g_list_length(keywords) > 0)
			{
			GList *work;

			work = new_keywords;
			while (work)
				{
				gchar *key;
				GList *p;

				key = work->data;
				work = work->next;

				p = keywords;
				while (p && key)
					{
					gchar *needle = p->data;
					p = p->next;

					if (strcmp(needle, key) == 0) key = NULL;
					}

				if (key) keywords = g_list_append(keywords, g_strdup(key));
				}
			keywords_list = keywords;
			}
		else
			{
			keywords_list = new_keywords;
			}
		}
	
	metadata_write_string(fd, COMMENT_KEY, comment);
	metadata_write_list(fd, KEYWORD_KEY, keywords);

	string_list_free(keywords);
	g_free(comment);
}

gboolean find_string_in_list(GList *list, const gchar *string)
{
	while (list)
		{
		gchar *haystack = list->data;

		if (haystack && string && strcmp(haystack, string) == 0) return TRUE;

		list = list->next;
		}

	return FALSE;
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
			if (find_string_in_list(list, keyword) == FALSE)
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
	GList *keywords;
	gboolean found = FALSE;
	if (metadata_read(fd, &keywords, NULL))
		{
		GList *work = keywords;

		while (work)
			{
			gchar *kw = work->data;
			work = work->next;
			
			if (strcmp(kw, data) == 0)
				{
				found = TRUE;
				break;
				}
			}
		string_list_free(keywords);
		}
	return found;
}

gboolean meta_data_set_keyword_mark(FileData *fd, gint n, gboolean value, gpointer data)
{
	GList *keywords = NULL;
	gboolean found = FALSE;
	gboolean changed = FALSE;
	GList *work;
	metadata_read(fd, &keywords, NULL);

	work = keywords;

	while (work)
		{
		gchar *kw = work->data;
		
		if (strcmp(kw, data) == 0)
			{
			found = TRUE;
			if (!value) 
				{
				changed = TRUE;
				keywords = g_list_delete_link(keywords, work);
				g_free(kw);
				}
			break;
			}
		work = work->next;
		}
	if (value && !found) 
		{
		changed = TRUE;
		keywords = g_list_append(keywords, g_strdup(data));
		}
	
	if (changed) metadata_write_list(fd, KEYWORD_KEY, keywords);

	string_list_free(keywords);
	return TRUE;
}


/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
