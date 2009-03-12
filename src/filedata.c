/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "filedata.h"

#include "filefilter.h"
#include "cache.h"
#include "thumb_standard.h"
#include "ui_fileops.h"
#include "metadata.h"
#include "trash.h"


static GHashTable *file_data_pool = NULL;
static GHashTable *file_data_planned_change_hash = NULL;

static gint sidecar_file_priority(const gchar *path);


/*
 *-----------------------------------------------------------------------------
 * text conversion utils
 *-----------------------------------------------------------------------------
 */

gchar *text_from_size(gint64 size)
{
	gchar *a, *b;
	gchar *s, *d;
	gint l, n, i;

	/* what I would like to use is printf("%'d", size)
	 * BUT: not supported on every libc :(
	 */
	if (size > G_MAXUINT)
		{
		/* the %lld conversion is not valid in all libcs, so use a simple work-around */
		a = g_strdup_printf("%d%09d", (guint)(size / 1000000000), (guint)(size % 1000000000));
		}
	else
		{
		a = g_strdup_printf("%d", (guint)size);
		}
	l = strlen(a);
	n = (l - 1)/ 3;
	if (n < 1) return a;

	b = g_new(gchar, l + n + 1);

	s = a;
	d = b;
	i = l - n * 3;
	while (*s != '\0')
		{
		if (i < 1)
			{
			i = 3;
			*d = ',';
			d++;
			}

		*d = *s;
		s++;
		d++;
		i--;
		}
	*d = '\0';

	g_free(a);
	return b;
}

gchar *text_from_size_abrev(gint64 size)
{
	if (size < (gint64)1024)
		{
		return g_strdup_printf(_("%d bytes"), (gint)size);
		}
	if (size < (gint64)1048576)
		{
		return g_strdup_printf(_("%.1f K"), (gdouble)size / 1024.0);
		}
	if (size < (gint64)1073741824)
		{
		return g_strdup_printf(_("%.1f MB"), (gdouble)size / 1048576.0);
		}

	/* to avoid overflowing the gdouble, do division in two steps */
	size /= 1048576;
	return g_strdup_printf(_("%.1f GB"), (gdouble)size / 1024.0);
}

/* note: returned string is valid until next call to text_from_time() */
const gchar *text_from_time(time_t t)
{
	static gchar *ret = NULL;
	gchar buf[128];
	gint buflen;
	struct tm *btime;
	GError *error = NULL;

	btime = localtime(&t);

	/* the %x warning about 2 digit years is not an error */
	buflen = strftime(buf, sizeof(buf), "%x %H:%M", btime);
	if (buflen < 1) return "";

	g_free(ret);
	ret = g_locale_to_utf8(buf, buflen, NULL, NULL, &error);
	if (error)
		{
		log_printf("Error converting locale strftime to UTF-8: %s\n", error->message);
		g_error_free(error);
		return "";
		}

	return ret;
}

/*
 *-----------------------------------------------------------------------------
 * file info struct
 *-----------------------------------------------------------------------------
 */

FileData *file_data_merge_sidecar_files(FileData *target, FileData *source);
static void file_data_check_sidecars(FileData *fd);
FileData *file_data_disconnect_sidecar_file(FileData *target, FileData *sfd);


void file_data_increment_version(FileData *fd)
{
	fd->version++;
	fd->valid_marks = 0;
	if (fd->parent) 
		{
		fd->parent->version++;
		fd->parent->valid_marks = 0;
		}
}

static void file_data_set_collate_keys(FileData *fd)
{
	gchar *caseless_name;

	caseless_name = g_utf8_casefold(fd->name, -1);

	g_free(fd->collate_key_name);
	g_free(fd->collate_key_name_nocase);

#if GLIB_CHECK_VERSION(2, 8, 0)
	fd->collate_key_name = g_utf8_collate_key_for_filename(fd->name, -1);
	fd->collate_key_name_nocase = g_utf8_collate_key_for_filename(caseless_name, -1);
#else
	fd->collate_key_name = g_utf8_collate_key(fd->name, -1);
	fd->collate_key_name_nocase = g_utf8_collate_key(caseless_name, -1);
#endif
	g_free(caseless_name);
}

static void file_data_set_path(FileData *fd, const gchar *path)
{
	g_assert(path /* && *path*/); /* view_dir_tree uses FileData with zero length path */
	g_assert(file_data_pool);

	g_free(fd->path);

	if (fd->original_path)
		{
		g_hash_table_remove(file_data_pool, fd->original_path);
		g_free(fd->original_path);
		}

	g_assert(!g_hash_table_lookup(file_data_pool, path));

	fd->original_path = g_strdup(path);
	g_hash_table_insert(file_data_pool, fd->original_path, fd);

	if (strcmp(path, G_DIR_SEPARATOR_S) == 0)
		{
		fd->path = g_strdup(path);
		fd->name = fd->path;
		fd->extension = fd->name + 1;
		file_data_set_collate_keys(fd);
		return;
		}

	fd->path = g_strdup(path);
	fd->name = filename_from_path(fd->path);

	if (strcmp(fd->name, "..") == 0)
		{
		gchar *dir = remove_level_from_path(path);
		g_free(fd->path);
		fd->path = remove_level_from_path(dir);
		g_free(dir);
		fd->name = "..";
		fd->extension = fd->name + 2;
		file_data_set_collate_keys(fd);
		return;
		}
	else if (strcmp(fd->name, ".") == 0)
		{
		g_free(fd->path);
		fd->path = remove_level_from_path(path);
		fd->name = ".";
		fd->extension = fd->name + 1;
		file_data_set_collate_keys(fd);
		return;
		}

	fd->extension = extension_from_path(fd->path);
	if (fd->extension == NULL)
		{
		fd->extension = fd->name + strlen(fd->name);
		}

	file_data_set_collate_keys(fd);
}

static gboolean file_data_check_changed_files_recursive(FileData *fd, struct stat *st)
{
	gboolean ret = FALSE;
	GList *work;
	
	if (fd->size != st->st_size ||
	    fd->date != st->st_mtime)
		{
		fd->size = st->st_size;
		fd->date = st->st_mtime;
		fd->mode = st->st_mode;
		if (fd->thumb_pixbuf) g_object_unref(fd->thumb_pixbuf);
		fd->thumb_pixbuf = NULL;
		file_data_increment_version(fd);
		file_data_send_notification(fd, NOTIFY_TYPE_REREAD);
		ret = TRUE;
		}

	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		struct stat st;
		work = work->next;

		if (!stat_utf8(sfd->path, &st))
			{
			fd->size = 0;
			fd->date = 0;
			file_data_disconnect_sidecar_file(fd, sfd);
			ret = TRUE;
			continue;
			}

		ret |= file_data_check_changed_files_recursive(sfd, &st);
		}
	return ret;
}


gboolean file_data_check_changed_files(FileData *fd)
{
	gboolean ret = FALSE;
	struct stat st;
	
	if (fd->parent) fd = fd->parent;

	if (!stat_utf8(fd->path, &st))
		{
		GList *work;
		FileData *sfd = NULL;

		/* parent is missing, we have to rebuild whole group */
		ret = TRUE;
		fd->size = 0;
		fd->date = 0;
		
		work = fd->sidecar_files;
		while (work)
			{
			sfd = work->data;
			work = work->next;
		
			file_data_disconnect_sidecar_file(fd, sfd);
			}
		if (sfd) file_data_check_sidecars(sfd); /* this will group the sidecars back together */
		file_data_send_notification(fd, NOTIFY_TYPE_REREAD);
		}
	else
		{
		ret |= file_data_check_changed_files_recursive(fd, &st);
		}

	return ret;
}

static FileData *file_data_new(const gchar *path_utf8, struct stat *st, gboolean check_sidecars)
{
	FileData *fd;

	DEBUG_2("file_data_new: '%s' %d", path_utf8, check_sidecars);

	if (!file_data_pool)
		file_data_pool = g_hash_table_new(g_str_hash, g_str_equal);

	fd = g_hash_table_lookup(file_data_pool, path_utf8);
	if (fd)
		{
		file_data_ref(fd);
		}
		
	if (!fd && file_data_planned_change_hash)
		{
		fd = g_hash_table_lookup(file_data_planned_change_hash, path_utf8);
		if (fd)
			{
			DEBUG_1("planned change: using %s -> %s", path_utf8, fd->path);
			file_data_ref(fd);
			file_data_apply_ci(fd);
			}
		}
		
	if (fd)
		{
		gboolean changed;
		
		if (fd->parent)
			changed = file_data_check_changed_files(fd);
		else
			changed = file_data_check_changed_files_recursive(fd, st);
		if (changed && check_sidecars && sidecar_file_priority(fd->extension))
			file_data_check_sidecars(fd);
		DEBUG_2("file_data_pool hit: '%s' %s", fd->path, changed ? "(changed)" : "");
		
		return fd;
		}

	fd = g_new0(FileData, 1);
	
	fd->size = st->st_size;
	fd->date = st->st_mtime;
	fd->mode = st->st_mode;
	fd->ref = 1;
	fd->magick = 0x12345678;

	file_data_set_path(fd, path_utf8); /* set path, name, collate_key_*, original_path */

	if (check_sidecars)
		file_data_check_sidecars(fd);

	return fd;
}

static void file_data_check_sidecars(FileData *fd)
{
	gint base_len;
	GString *fname;
	FileData *parent_fd = NULL;
	GList *work;

	if (fd->disable_grouping || !sidecar_file_priority(fd->extension))
		return;

	base_len = fd->extension - fd->path;
	fname = g_string_new_len(fd->path, base_len);
	work = sidecar_ext_get_list();

	while (work)
		{
		/* check for possible sidecar files;
		   the sidecar files created here are referenced only via fd->sidecar_files or fd->parent,
		   they have fd->ref set to 0 and file_data unref must chack and free them all together
		   (using fd->ref would cause loops and leaks)
		*/

		FileData *new_fd;
		gchar *ext = work->data;

		work = work->next;

		if (g_ascii_strcasecmp(ext, fd->extension) == 0)
			{
			new_fd = fd; /* processing the original file */
			}
		else
			{
			struct stat nst;
			g_string_truncate(fname, base_len);

			if (!stat_utf8_case_insensitive_ext(fname, ext, &nst))
				continue;

			new_fd = file_data_new(fname->str, &nst, FALSE);
			
			if (new_fd->disable_grouping)
				{
				file_data_unref(new_fd);
				continue;
				}
			
			new_fd->ref--; /* do not use ref here */
			}

		if (!parent_fd)
			parent_fd = new_fd; /* parent is the one with the highest prio, found first */
		else
			file_data_merge_sidecar_files(parent_fd, new_fd);
		}
	g_string_free(fname, TRUE);
}


static FileData *file_data_new_local(const gchar *path, struct stat *st, gboolean check_sidecars)
{
	gchar *path_utf8 = path_to_utf8(path);
	FileData *ret = file_data_new(path_utf8, st, check_sidecars);

	g_free(path_utf8);
	return ret;
}

FileData *file_data_new_simple(const gchar *path_utf8)
{
	struct stat st;

	if (!stat_utf8(path_utf8, &st))
		{
		st.st_size = 0;
		st.st_mtime = 0;
		}

	return file_data_new(path_utf8, &st, TRUE);
}

FileData *file_data_add_sidecar_file(FileData *target, FileData *sfd)
{
	sfd->parent = target;
	if (!g_list_find(target->sidecar_files, sfd))
		target->sidecar_files = g_list_prepend(target->sidecar_files, sfd);
	file_data_increment_version(sfd); /* increments both sfd and target */
	return target;
}


FileData *file_data_merge_sidecar_files(FileData *target, FileData *source)
{
	GList *work;
	
	file_data_add_sidecar_file(target, source);

	work = source->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		file_data_add_sidecar_file(target, sfd);
		work = work->next;
		}

	g_list_free(source->sidecar_files);
	source->sidecar_files = NULL;

	target->sidecar_files = filelist_sort(target->sidecar_files, SORT_NAME, TRUE);
	
	return target;
}

#ifdef DEBUG_FILEDATA
FileData *file_data_ref_debug(const gchar *file, gint line, FileData *fd)
#else
FileData *file_data_ref(FileData *fd)
#endif
{
	if (fd == NULL) return NULL;
#ifdef DEBUG_FILEDATA
	if (fd->magick != 0x12345678)
		DEBUG_0("fd magick mismatch at %s:%d", file, line);
#endif
	g_assert(fd->magick == 0x12345678);
	fd->ref++;

#ifdef DEBUG_FILEDATA
	DEBUG_2("file_data_ref (%d): '%s' @ %s:%d", fd->ref, fd->path, file, line);
#else
	DEBUG_2("file_data_ref (%d): '%s'", fd->ref, fd->path);
#endif
	return fd;
}

static void file_data_free(FileData *fd)
{
	g_assert(fd->magick == 0x12345678);
	g_assert(fd->ref == 0);

	g_hash_table_remove(file_data_pool, fd->original_path);

	g_free(fd->path);
	g_free(fd->original_path);
	g_free(fd->collate_key_name);
	g_free(fd->collate_key_name_nocase);
	if (fd->thumb_pixbuf) g_object_unref(fd->thumb_pixbuf);
	g_free(fd->histmap);
	
	g_assert(fd->sidecar_files == NULL); /* sidecar files must be freed before calling this */

	file_data_change_info_free(NULL, fd);
	g_free(fd);
}

#ifdef DEBUG_FILEDATA
void file_data_unref_debug(const gchar *file, gint line, FileData *fd)
#else
void file_data_unref(FileData *fd)
#endif
{
	if (fd == NULL) return;
#ifdef DEBUG_FILEDATA
	if (fd->magick != 0x12345678)
		DEBUG_0("fd magick mismatch @ %s:%d", file, line);
#endif
	g_assert(fd->magick == 0x12345678);
	
	fd->ref--;
#ifdef DEBUG_FILEDATA
	DEBUG_2("file_data_unref (%d): '%s' @ %s:%d", fd->ref, fd->path, file, line);
#else
	DEBUG_2("file_data_unref (%d): '%s'", fd->ref, fd->path);
#endif
	if (fd->ref == 0)
		{
		GList *work;
		FileData *parent = fd->parent ? fd->parent : fd;
		
		if (parent->ref > 0) return;

		work = parent->sidecar_files;
		while (work)
			{
			FileData *sfd = work->data;
			if (sfd->ref > 0) return;
			work = work->next;
			}

		/* none of parent/children is referenced, we can free everything */

		DEBUG_2("file_data_unref: deleting '%s', parent '%s'", fd->path, fd->parent ? parent->path : "-");

		work = parent->sidecar_files;
		while (work)
			{
			FileData *sfd = work->data;
			file_data_free(sfd);
			work = work->next;
			}

		g_list_free(parent->sidecar_files);
		parent->sidecar_files = NULL;

		file_data_free(parent);
		}
}

FileData *file_data_disconnect_sidecar_file(FileData *target, FileData *sfd)
{
	sfd->parent = target;
	g_assert(g_list_find(target->sidecar_files, sfd));
	
	file_data_increment_version(sfd); /* increments both sfd and target */

	target->sidecar_files = g_list_remove(target->sidecar_files, sfd);
	sfd->parent = NULL;

	if (sfd->ref == 0)
		{
		file_data_free(sfd);
		return NULL;
		}

	return sfd;
}

/* disables / enables grouping for particular file, sends UPDATE notification */
void file_data_disable_grouping(FileData *fd, gboolean disable)
{
	if (!fd->disable_grouping == !disable) return;
	fd->disable_grouping = !!disable;
	
	if (disable)
		{
		if (fd->parent)
			{
			FileData *parent = file_data_ref(fd->parent);
			file_data_disconnect_sidecar_file(parent, fd);
			file_data_send_notification(fd, NOTIFY_TYPE_INTERNAL);
			file_data_send_notification(parent, NOTIFY_TYPE_INTERNAL);
			file_data_unref(parent);
			}
		else if (fd->sidecar_files)
			{
			GList *sidecar_files = filelist_copy(fd->sidecar_files);
			GList *work = sidecar_files;
			while (work)
				{
				FileData *sfd = work->data;
				work = work->next;
				file_data_disconnect_sidecar_file(fd, sfd);
				file_data_send_notification(sfd, NOTIFY_TYPE_INTERNAL);
				}
			file_data_send_notification(fd, NOTIFY_TYPE_INTERNAL);
			file_data_check_sidecars((FileData *)sidecar_files->data); /* this will group the sidecars back together */
			filelist_free(sidecar_files);
			}
		}
	else
		{
		file_data_check_sidecars(fd);
		file_data_send_notification(fd, NOTIFY_TYPE_INTERNAL);
		}
}

/* compare name without extension */
gint file_data_compare_name_without_ext(FileData *fd1, FileData *fd2)
{
	size_t len1 = fd1->extension - fd1->name;
	size_t len2 = fd2->extension - fd2->name;

	if (len1 < len2) return -1;
	if (len1 > len2) return 1;

	return strncmp(fd1->name, fd2->name, len1); /* FIXME: utf8 */
}

void file_data_change_info_free(FileDataChangeInfo *fdci, FileData *fd)
{
	if (!fdci && fd) fdci = fd->change;

	if (!fdci) return;

	g_free(fdci->source);
	g_free(fdci->dest);

	g_free(fdci);

	if (fd) fd->change = NULL;
}

static gboolean file_data_can_write_directly(FileData *fd)
{
	return filter_name_is_writable(fd->extension);
}

static gboolean file_data_can_write_sidecar(FileData *fd)
{
	return filter_name_allow_sidecar(fd->extension) && !filter_name_is_writable(fd->extension);
}

gchar *file_data_get_sidecar_path(FileData *fd, gboolean existing_only)
{
	gchar *sidecar_path = NULL;
	GList *work;
	
	if (!file_data_can_write_sidecar(fd)) return NULL;
	
	work = fd->parent ? fd->parent->sidecar_files : fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		work = work->next;
		if (g_ascii_strcasecmp(sfd->extension, ".xmp") == 0)
			{
			sidecar_path = g_strdup(sfd->path);
			break;
			}
		}
	
	if (!existing_only && !sidecar_path)
		{
		gchar *base = remove_extension_from_path(fd->path);
		sidecar_path = g_strconcat(base, ".xmp", NULL);
		g_free(base);
		}

	return sidecar_path;
}


/*
 *-----------------------------------------------------------------------------
 * sidecar file info struct
 *-----------------------------------------------------------------------------
 */



static gint sidecar_file_priority(const gchar *path)
{
	const gchar *extension = extension_from_path(path);
	gint i = 1;
	GList *work;

	if (extension == NULL)
		return 0;

	work = sidecar_ext_get_list();

	while (work) {
		gchar *ext = work->data;
		
		work = work->next;
		if (g_ascii_strcasecmp(extension, ext) == 0) return i;
		i++;
	}
	return 0;
}


/*
 *-----------------------------------------------------------------------------
 * load file list
 *-----------------------------------------------------------------------------
 */

static SortType filelist_sort_method = SORT_NONE;
static gboolean filelist_sort_ascend = TRUE;


gint filelist_sort_compare_filedata(FileData *fa, FileData *fb)
{
	if (!filelist_sort_ascend)
		{
		FileData *tmp = fa;
		fa = fb;
		fb = tmp;
		}

	switch (filelist_sort_method)
		{
		case SORT_NAME:
			break;
		case SORT_SIZE:
			if (fa->size < fb->size) return -1;
			if (fa->size > fb->size) return 1;
			/* fall back to name */
			break;
		case SORT_TIME:
			if (fa->date < fb->date) return -1;
			if (fa->date > fb->date) return 1;
			/* fall back to name */
			break;
#ifdef HAVE_STRVERSCMP
		case SORT_NUMBER:
			return strverscmp(fa->name, fb->name);
			break;
#endif
		default:
			break;
		}

	if (options->file_sort.case_sensitive)
		return strcmp(fa->collate_key_name, fb->collate_key_name);
	else
		return strcmp(fa->collate_key_name_nocase, fb->collate_key_name_nocase);
}

gint filelist_sort_compare_filedata_full(FileData *fa, FileData *fb, SortType method, gboolean ascend)
{
	filelist_sort_method = method;
	filelist_sort_ascend = ascend;
	return filelist_sort_compare_filedata(fa, fb);
}

static gint filelist_sort_file_cb(gpointer a, gpointer b)
{
	return filelist_sort_compare_filedata(a, b);
}

GList *filelist_sort_full(GList *list, SortType method, gboolean ascend, GCompareFunc cb)
{
	filelist_sort_method = method;
	filelist_sort_ascend = ascend;
	return g_list_sort(list, cb);
}

GList *filelist_insert_sort_full(GList *list, gpointer data, SortType method, gboolean ascend, GCompareFunc cb)
{
	filelist_sort_method = method;
	filelist_sort_ascend = ascend;
	return g_list_insert_sorted(list, data, cb);
}

GList *filelist_sort(GList *list, SortType method, gboolean ascend)
{
	return filelist_sort_full(list, method, ascend, (GCompareFunc) filelist_sort_file_cb);
}

GList *filelist_insert_sort(GList *list, FileData *fd, SortType method, gboolean ascend)
{
	return filelist_insert_sort_full(list, fd, method, ascend, (GCompareFunc) filelist_sort_file_cb);
}


static GList *filelist_filter_out_sidecars(GList *flist)
{
	GList *work = flist;
	GList *flist_filtered = NULL;

	while (work)
		{
		FileData *fd = work->data;
	
		work = work->next;
		if (fd->parent) /* remove fd's that are children */
			file_data_unref(fd);
		else
			flist_filtered = g_list_prepend(flist_filtered, fd);
		}
	g_list_free(flist);

	return flist_filtered;
}

static gboolean filelist_read_real(FileData *dir_fd, GList **files, GList **dirs, gboolean follow_symlinks)
{
	DIR *dp;
	struct dirent *dir;
	gchar *pathl;
	GList *dlist = NULL;
	GList *flist = NULL;
	gint (*stat_func)(const gchar *path, struct stat *buf);

	g_assert(files || dirs);

	if (files) *files = NULL;
	if (dirs) *dirs = NULL;

	pathl = path_from_utf8(dir_fd->path);
	if (!pathl) return FALSE;

	dp = opendir(pathl);
	if (dp == NULL)
		{
		g_free(pathl);
		return FALSE;
		}

	if (follow_symlinks)
		stat_func = stat;
	else
		stat_func = lstat;

	while ((dir = readdir(dp)) != NULL)
		{
		struct stat ent_sbuf;
		const gchar *name = dir->d_name;
		gchar *filepath;

		if (!options->file_filter.show_hidden_files && ishidden(name))
			continue;

		filepath = g_build_filename(pathl, name, NULL);
		if (stat_func(filepath, &ent_sbuf) >= 0)
			{
			if (S_ISDIR(ent_sbuf.st_mode))
				{
				/* we ignore the .thumbnails dir for cleanliness */
				if (dirs &&
				    !(name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) &&
				    strcmp(name, GQ_CACHE_LOCAL_THUMB) != 0 &&
				    strcmp(name, GQ_CACHE_LOCAL_METADATA) != 0 &&
				    strcmp(name, THUMB_FOLDER_LOCAL) != 0)
					{
					dlist = g_list_prepend(dlist, file_data_new_local(filepath, &ent_sbuf, FALSE));
					}
				}
			else
				{
				if (files && filter_name_exists(name))
					{
					flist = g_list_prepend(flist, file_data_new_local(filepath, &ent_sbuf, TRUE));
					}
				}
			}
		g_free(filepath);
		}

	closedir(dp);
	
	g_free(pathl);

	if (dirs) *dirs = dlist;
	if (files) *files = filelist_filter_out_sidecars(flist);

	return TRUE;
}

gboolean filelist_read(FileData *dir_fd, GList **files, GList **dirs)
{
	return filelist_read_real(dir_fd, files, dirs, TRUE);
}

gboolean filelist_read_lstat(FileData *dir_fd, GList **files, GList **dirs)
{
	return filelist_read_real(dir_fd, files, dirs, FALSE);
}

void filelist_free(GList *list)
{
	GList *work;

	work = list;
	while (work)
		{
		file_data_unref((FileData *)work->data);
		work = work->next;
		}

	g_list_free(list);
}


GList *filelist_copy(GList *list)
{
	GList *new_list = NULL;
	GList *work;

	work = list;
	while (work)
		{
		FileData *fd;

		fd = work->data;
		work = work->next;

		new_list = g_list_prepend(new_list, file_data_ref(fd));
		}

	return g_list_reverse(new_list);
}

GList *filelist_from_path_list(GList *list)
{
	GList *new_list = NULL;
	GList *work;

	work = list;
	while (work)
		{
		gchar *path;

		path = work->data;
		work = work->next;

		new_list = g_list_prepend(new_list, file_data_new_simple(path));
		}

	return g_list_reverse(new_list);
}

GList *filelist_to_path_list(GList *list)
{
	GList *new_list = NULL;
	GList *work;

	work = list;
	while (work)
		{
		FileData *fd;

		fd = work->data;
		work = work->next;

		new_list = g_list_prepend(new_list, g_strdup(fd->path));
		}

	return g_list_reverse(new_list);
}

GList *filelist_filter(GList *list, gboolean is_dir_list)
{
	GList *work;

	if (!is_dir_list && options->file_filter.disable && options->file_filter.show_hidden_files) return list;

	work = list;
	while (work)
		{
		FileData *fd = (FileData *)(work->data);
		const gchar *name = fd->name;

		if ((!options->file_filter.show_hidden_files && ishidden(name)) ||
		    (!is_dir_list && !filter_name_exists(name)) ||
		    (is_dir_list && name[0] == '.' && (strcmp(name, GQ_CACHE_LOCAL_THUMB) == 0 ||
						       strcmp(name, GQ_CACHE_LOCAL_METADATA) == 0)) )
			{
			GList *link = work;
			
			list = g_list_remove_link(list, link);
			file_data_unref(fd);
			g_list_free(link);
			}
	
		work = work->next;
		}

	return list;
}

/*
 *-----------------------------------------------------------------------------
 * filelist recursive
 *-----------------------------------------------------------------------------
 */

static gint filelist_sort_path_cb(gconstpointer a, gconstpointer b)
{
	return CASE_SORT(((FileData *)a)->path, ((FileData *)b)->path);
}

GList *filelist_sort_path(GList *list)
{
	return g_list_sort(list, filelist_sort_path_cb);
}

static void filelist_recursive_append(GList **list, GList *dirs)
{
	GList *work;

	work = dirs;
	while (work)
		{
		FileData *fd = (FileData *)(work->data);
		GList *f;
		GList *d;

		if (filelist_read(fd, &f, &d))
			{
			f = filelist_filter(f, FALSE);
			f = filelist_sort_path(f);
			*list = g_list_concat(*list, f);

			d = filelist_filter(d, TRUE);
			d = filelist_sort_path(d);
			filelist_recursive_append(list, d);
			filelist_free(d);
			}

		work = work->next;
		}
}

GList *filelist_recursive(FileData *dir_fd)
{
	GList *list;
	GList *d;

	if (!filelist_read(dir_fd, &list, &d)) return NULL;
	list = filelist_filter(list, FALSE);
	list = filelist_sort_path(list);

	d = filelist_filter(d, TRUE);
	d = filelist_sort_path(d);
	filelist_recursive_append(&list, d);
	filelist_free(d);

	return list;
}


/*
 * marks and orientation
 */

static FileDataGetMarkFunc file_data_get_mark_func[FILEDATA_MARKS_SIZE];
static FileDataSetMarkFunc file_data_set_mark_func[FILEDATA_MARKS_SIZE];
static gpointer file_data_mark_func_data[FILEDATA_MARKS_SIZE];

gboolean file_data_get_mark(FileData *fd, gint n)
{
	gboolean valid = (fd->valid_marks & (1 << n));
	
	if (file_data_get_mark_func[n] && !valid) 
		{
		guint old = fd->marks;
		gboolean value = (file_data_get_mark_func[n])(fd, n, file_data_mark_func_data[n]);
		
		if (!value != !(fd->marks & (1 << n))) 
			{
			fd->marks = fd->marks ^ (1 << n);
			}
		
		fd->valid_marks |= (1 << n);
		if (old && !fd->marks) /* keep files with non-zero marks in memory */
			{
			file_data_unref(fd);
			}
		else if (!old && fd->marks)
			{
			file_data_ref(fd);
			}
		}

	return !!(fd->marks & (1 << n));
}

guint file_data_get_marks(FileData *fd)
{
	gint i;
	for (i = 0; i < FILEDATA_MARKS_SIZE; i++) file_data_get_mark(fd, i);
	return fd->marks;
}

void file_data_set_mark(FileData *fd, gint n, gboolean value)
{
	guint old;
	if (!value == !file_data_get_mark(fd, n)) return;
	
	if (file_data_set_mark_func[n]) 
		{
		(file_data_set_mark_func[n])(fd, n, value, file_data_mark_func_data[n]);
		}
	
	old = fd->marks;

	fd->marks = fd->marks ^ (1 << n);
	
	if (old && !fd->marks) /* keep files with non-zero marks in memory */
		{
		file_data_unref(fd);
		}
	else if (!old && fd->marks)
		{
		file_data_ref(fd);
		}
	
	file_data_increment_version(fd);
	file_data_send_notification(fd, NOTIFY_TYPE_INTERNAL);
}

gboolean file_data_filter_marks(FileData *fd, guint filter)
{
	gint i;
	for (i = 0; i < FILEDATA_MARKS_SIZE; i++) if (filter & (1 << i)) file_data_get_mark(fd, i);
	return ((fd->marks & filter) == filter);
}

GList *file_data_filter_marks_list(GList *list, guint filter)
{
	GList *work;

	work = list;
	while (work)
		{
		FileData *fd = work->data;
		GList *link = work;
		work = work->next;

		if (!file_data_filter_marks(fd, filter))
			{
			list = g_list_remove_link(list, link);
			file_data_unref(fd);
			g_list_free(link);
			}
		}

	return list;
}

static void file_data_notify_mark_func(gpointer key, gpointer value, gpointer user_data)
{
	FileData *fd = value;
	file_data_increment_version(fd);
	file_data_send_notification(fd, NOTIFY_TYPE_INTERNAL);
}

gboolean file_data_register_mark_func(gint n, FileDataGetMarkFunc get_mark_func, FileDataSetMarkFunc set_mark_func, gpointer data)
{
	if (n < 0 || n >= FILEDATA_MARKS_SIZE) return FALSE;
		
	file_data_get_mark_func[n] = get_mark_func;
        file_data_set_mark_func[n] = set_mark_func;
        file_data_mark_func_data[n] = data;
        
        if (get_mark_func)
    		{
    		/* this effectively changes all known files */
    		g_hash_table_foreach(file_data_pool, file_data_notify_mark_func, NULL);
    		}
        
        return TRUE;
}

void file_data_get_registered_mark_func(gint n, FileDataGetMarkFunc *get_mark_func, FileDataSetMarkFunc *set_mark_func, gpointer *data)
{
	if (get_mark_func) *get_mark_func = file_data_get_mark_func[n];
	if (set_mark_func) *set_mark_func = file_data_set_mark_func[n];
	if (data) *data = file_data_mark_func_data[n];
}

gint file_data_get_user_orientation(FileData *fd)
{
	return fd->user_orientation;
}

void file_data_set_user_orientation(FileData *fd, gint value)
{
	if (fd->user_orientation == value) return;

	fd->user_orientation = value;
	file_data_increment_version(fd);
	file_data_send_notification(fd, NOTIFY_TYPE_INTERNAL);
}


/*
 * file_data    - operates on the given fd
 * file_data_sc - operates on the given fd + sidecars - all fds linked via fd->sidecar_files or fd->parent
 */


/* return list of sidecar file extensions in a string */
gchar *file_data_sc_list_to_string(FileData *fd)
{
	GList *work;
	GString *result = g_string_new("");

	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;

		result = g_string_append(result, "+ ");
		result = g_string_append(result, sfd->extension);
		work = work->next;
		if (work) result = g_string_append_c(result, ' ');
		}

	return g_string_free(result, FALSE);
}



/*
 * add FileDataChangeInfo (see typedefs.h) for the given operation
 * uses file_data_add_change_info
 *
 * fails if the fd->change already exists - change operations can't run in parallel
 * fd->change_info works as a lock
 *
 * dest can be NULL - in this case the current name is used for now, it will
 * be changed later
 */

/*
   FileDataChangeInfo types:
   COPY
   MOVE   - path is changed, name may be changed too
   RENAME - path remains unchanged, name is changed
            extension should remain (FIXME should we allow editing extension? it will make problems wth grouping)
	    sidecar names are changed too, extensions are not changed
   DELETE
   UPDATE - file size, date or grouping has been changed
*/

gboolean file_data_add_ci(FileData *fd, FileDataChangeType type, const gchar *src, const gchar *dest)
{
	FileDataChangeInfo *fdci;

	if (fd->change) return FALSE;

	fdci = g_new0(FileDataChangeInfo, 1);

	fdci->type = type;

	if (src)
		fdci->source = g_strdup(src);
	else
		fdci->source = g_strdup(fd->path);

	if (dest)
		fdci->dest = g_strdup(dest);

	fd->change = fdci;
	
	return TRUE;
}

static void file_data_planned_change_remove(FileData *fd)
{
	if (file_data_planned_change_hash &&
	    (fd->change->type == FILEDATA_CHANGE_MOVE || fd->change->type == FILEDATA_CHANGE_RENAME))
		{
		if (g_hash_table_lookup(file_data_planned_change_hash, fd->change->dest) == fd)
			{
			DEBUG_1("planned change: removing %s -> %s", fd->change->dest, fd->path);
			g_hash_table_remove(file_data_planned_change_hash, fd->change->dest);
			file_data_unref(fd);
			if (g_hash_table_size(file_data_planned_change_hash) == 0)
				{
				g_hash_table_destroy(file_data_planned_change_hash);
				file_data_planned_change_hash = NULL;
				DEBUG_1("planned change: empty");
				}
			}
		}
}


void file_data_free_ci(FileData *fd)
{
	FileDataChangeInfo *fdci = fd->change;

	if (!fdci) return;

	file_data_planned_change_remove(fd);

	g_free(fdci->source);
	g_free(fdci->dest);

	g_free(fdci);

	fd->change = NULL;
}


static gboolean file_data_sc_add_ci(FileData *fd, FileDataChangeType type)
{
	GList *work;

	if (fd->parent) fd = fd->parent;
	
	if (fd->change) return FALSE;
	
	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		
		if (sfd->change) return FALSE;
		work = work->next;
		}

	file_data_add_ci(fd, type, NULL, NULL);
	
	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		
		file_data_add_ci(sfd, type, NULL, NULL);
		work = work->next;
		}
		
	return TRUE;
}

static gboolean file_data_sc_check_ci(FileData *fd, FileDataChangeType type)
{
	GList *work;
	
	if (fd->parent) fd = fd->parent;
	
	if (!fd->change || fd->change->type != type) return FALSE;
	
	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;

		if (!sfd->change || sfd->change->type != type) return FALSE;
		work = work->next;
		}

	return TRUE;
}


gboolean file_data_sc_add_ci_copy(FileData *fd, const gchar *dest_path)
{
	if (!file_data_sc_add_ci(fd, FILEDATA_CHANGE_COPY)) return FALSE;
	file_data_sc_update_ci_copy(fd, dest_path);
	return TRUE;
}

gboolean file_data_sc_add_ci_move(FileData *fd, const gchar *dest_path)
{
	if (!file_data_sc_add_ci(fd, FILEDATA_CHANGE_MOVE)) return FALSE;
	file_data_sc_update_ci_move(fd, dest_path);
	return TRUE;
}

gboolean file_data_sc_add_ci_rename(FileData *fd, const gchar *dest_path)
{
	if (!file_data_sc_add_ci(fd, FILEDATA_CHANGE_RENAME)) return FALSE;
	file_data_sc_update_ci_rename(fd, dest_path);
	return TRUE;
}

gboolean file_data_sc_add_ci_delete(FileData *fd)
{
	return file_data_sc_add_ci(fd, FILEDATA_CHANGE_DELETE);
}

gboolean file_data_sc_add_ci_unspecified(FileData *fd, const gchar *dest_path)
{
	if (!file_data_sc_add_ci(fd, FILEDATA_CHANGE_UNSPECIFIED)) return FALSE;
	file_data_sc_update_ci_unspecified(fd, dest_path);
	return TRUE;
}

gboolean file_data_add_ci_write_metadata(FileData *fd)
{
	return file_data_add_ci(fd, FILEDATA_CHANGE_WRITE_METADATA, NULL, NULL);
}

void file_data_sc_free_ci(FileData *fd)
{
	GList *work;

	if (fd->parent) fd = fd->parent;
	
	file_data_free_ci(fd);
	
	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
	
		file_data_free_ci(sfd);
		work = work->next;
		}
}

gboolean file_data_sc_add_ci_delete_list(GList *fd_list)
{
	GList *work;
	gboolean ret = TRUE;

	work = fd_list;
	while (work)
		{
		FileData *fd = work->data;
	
		if (!file_data_sc_add_ci_delete(fd)) ret = FALSE;
		work = work->next;
		}

	return ret;
}

static void file_data_sc_revert_ci_list(GList *fd_list)
{
	GList *work;
	
	work = fd_list;
	while (work)
		{
		FileData *fd = work->data;
		
		file_data_sc_free_ci(fd);
		work = work->prev;
		}
}

static gboolean file_data_sc_add_ci_list_call_func(GList *fd_list, const gchar *dest, gboolean (*func)(FileData *, const gchar *))
{
	GList *work;
	
	work = fd_list;
	while (work)
		{
		FileData *fd = work->data;
		
		if (!func(fd, dest))
			{
			file_data_sc_revert_ci_list(work->prev);
			return FALSE;
			}
		work = work->next;
		}
	
	return TRUE;
}

gboolean file_data_sc_add_ci_copy_list(GList *fd_list, const gchar *dest)
{
	return file_data_sc_add_ci_list_call_func(fd_list, dest, file_data_sc_add_ci_copy);
}

gboolean file_data_sc_add_ci_move_list(GList *fd_list, const gchar *dest)
{
	return file_data_sc_add_ci_list_call_func(fd_list, dest, file_data_sc_add_ci_move);
}

gboolean file_data_sc_add_ci_rename_list(GList *fd_list, const gchar *dest)
{
	return file_data_sc_add_ci_list_call_func(fd_list, dest, file_data_sc_add_ci_rename);
}

gboolean file_data_sc_add_ci_unspecified_list(GList *fd_list, const gchar *dest)
{
	return file_data_sc_add_ci_list_call_func(fd_list, dest, file_data_sc_add_ci_unspecified);
}

gboolean file_data_add_ci_write_metadata_list(GList *fd_list)
{
	GList *work;
	gboolean ret = TRUE;

	work = fd_list;
	while (work)
		{
		FileData *fd = work->data;
	
		if (!file_data_add_ci_write_metadata(fd)) ret = FALSE;
		work = work->next;
		}

	return ret;
}

void file_data_free_ci_list(GList *fd_list)
{
	GList *work;
	
	work = fd_list;
	while (work)
		{
		FileData *fd = work->data;
		
		file_data_free_ci(fd);
		work = work->next;
		}
}

void file_data_sc_free_ci_list(GList *fd_list)
{
	GList *work;
	
	work = fd_list;
	while (work)
		{
		FileData *fd = work->data;
		
		file_data_sc_free_ci(fd);
		work = work->next;
		}
}

/*
 * update existing fd->change, it will be used from dialog callbacks for interactive editing
 * fails if fd->change does not exist or the change type does not match
 */

static void file_data_update_planned_change_hash(FileData *fd, const gchar *old_path, gchar *new_path)
{
	FileDataChangeType type = fd->change->type;
	
	if (type == FILEDATA_CHANGE_MOVE || type == FILEDATA_CHANGE_RENAME)
		{
		FileData *ofd;
		
		if (!file_data_planned_change_hash)
			file_data_planned_change_hash = g_hash_table_new(g_str_hash, g_str_equal);
		
		if (old_path && g_hash_table_lookup(file_data_planned_change_hash, old_path) == fd)
			{
			DEBUG_1("planned change: removing %s -> %s", old_path, fd->path);
			g_hash_table_remove(file_data_planned_change_hash, old_path);
			file_data_unref(fd);
			}

		ofd = g_hash_table_lookup(file_data_planned_change_hash, new_path);
		if (ofd != fd)
			{
			if (ofd)
				{
				DEBUG_1("planned change: replacing %s -> %s", new_path, ofd->path);
				g_hash_table_remove(file_data_planned_change_hash, new_path);
				file_data_unref(ofd);
				}
			
			DEBUG_1("planned change: inserting %s -> %s", new_path, fd->path);
			file_data_ref(fd);
			g_hash_table_insert(file_data_planned_change_hash, new_path, fd);
			}
		}
}

static void file_data_update_ci_dest(FileData *fd, const gchar *dest_path)
{
	gchar *old_path = fd->change->dest;

	fd->change->dest = g_strdup(dest_path);
	file_data_update_planned_change_hash(fd, old_path, fd->change->dest);
	g_free(old_path);
}

static void file_data_update_ci_dest_preserve_ext(FileData *fd, const gchar *dest_path)
{
	const gchar *extension = extension_from_path(fd->change->source);
	gchar *base = remove_extension_from_path(dest_path);
	gchar *old_path = fd->change->dest;
	
	fd->change->dest = g_strconcat(base, extension, NULL);
	file_data_update_planned_change_hash(fd, old_path, fd->change->dest);
	
	g_free(old_path);
	g_free(base);
}

static void file_data_sc_update_ci(FileData *fd, const gchar *dest_path)
{
	GList *work;
	gchar *dest_path_full = NULL;
	
	if (fd->parent) fd = fd->parent;
	
	if (!dest_path)
		{
		dest_path = fd->path;
		}
	else if (!strchr(dest_path, G_DIR_SEPARATOR)) /* we got only filename, not a full path */
		{
		gchar *dir = remove_level_from_path(fd->path);
		
		dest_path_full = g_build_filename(dir, dest_path, NULL);
		g_free(dir);
		dest_path = dest_path_full;
		}
	else if (fd->change->type != FILEDATA_CHANGE_RENAME && isdir(dest_path)) /* rename should not move files between directories */
		{
		dest_path_full = g_build_filename(dest_path, fd->name, NULL);
		dest_path = dest_path_full;
		}
		
	file_data_update_ci_dest(fd, dest_path);
	
	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		
		file_data_update_ci_dest_preserve_ext(sfd, dest_path);
		work = work->next;
		}
	
	g_free(dest_path_full);
}

static gboolean file_data_sc_check_update_ci(FileData *fd, const gchar *dest_path, FileDataChangeType type)
{
	if (!file_data_sc_check_ci(fd, type)) return FALSE;
	file_data_sc_update_ci(fd, dest_path);
	return TRUE;
}

gboolean file_data_sc_update_ci_copy(FileData *fd, const gchar *dest_path)
{
	return file_data_sc_check_update_ci(fd, dest_path, FILEDATA_CHANGE_COPY);
}
	
gboolean file_data_sc_update_ci_move(FileData *fd, const gchar *dest_path)
{
	return file_data_sc_check_update_ci(fd, dest_path, FILEDATA_CHANGE_MOVE);
}

gboolean file_data_sc_update_ci_rename(FileData *fd, const gchar *dest_path)
{
	return file_data_sc_check_update_ci(fd, dest_path, FILEDATA_CHANGE_RENAME);
}

gboolean file_data_sc_update_ci_unspecified(FileData *fd, const gchar *dest_path)
{
	return file_data_sc_check_update_ci(fd, dest_path, FILEDATA_CHANGE_UNSPECIFIED);
}

static gboolean file_data_sc_update_ci_list_call_func(GList *fd_list,
						      const gchar *dest,
						      gboolean (*func)(FileData *, const gchar *))
{
	GList *work;
	gboolean ret = TRUE;
	
	work = fd_list;
	while (work)
		{
		FileData *fd = work->data;
		
		if (!func(fd, dest)) ret = FALSE;
		work = work->next;
		}
	
	return ret;
}

gboolean file_data_sc_update_ci_move_list(GList *fd_list, const gchar *dest)
{
	return file_data_sc_update_ci_list_call_func(fd_list, dest, file_data_sc_update_ci_move);
}

gboolean file_data_sc_update_ci_copy_list(GList *fd_list, const gchar *dest)
{
	return file_data_sc_update_ci_list_call_func(fd_list, dest, file_data_sc_update_ci_copy);
}

gboolean file_data_sc_update_ci_unspecified_list(GList *fd_list, const gchar *dest)
{
	return file_data_sc_update_ci_list_call_func(fd_list, dest, file_data_sc_update_ci_unspecified);
}


/*
 * verify source and dest paths - dest image exists, etc.
 * it should detect all possible problems with the planned operation
 */

gint file_data_verify_ci(FileData *fd)
{
	gint ret = CHANGE_OK;
	gchar *dir;
	
	if (!fd->change)
		{
		DEBUG_1("Change checked: no change info: %s", fd->path);
		return ret;
		}

	if (!isname(fd->path))
		{
		/* this probably should not happen */
		ret |= CHANGE_NO_SRC;
		DEBUG_1("Change checked: file does not exist: %s", fd->path);
		return ret;
		}
		
	dir = remove_level_from_path(fd->path);
	
	if (fd->change->type != FILEDATA_CHANGE_DELETE &&
	    fd->change->type != FILEDATA_CHANGE_WRITE_METADATA &&
	    !access_file(fd->path, R_OK))
		{
		ret |= CHANGE_NO_READ_PERM;
		DEBUG_1("Change checked: no read permission: %s", fd->path);
		}
	else if ((fd->change->type == FILEDATA_CHANGE_DELETE || fd->change->type == FILEDATA_CHANGE_MOVE) &&
	    	 !access_file(dir, W_OK))
		{
		ret |= CHANGE_NO_WRITE_PERM_DIR;
		DEBUG_1("Change checked: source dir is readonly: %s", fd->path);
		}
	else if (fd->change->type != FILEDATA_CHANGE_COPY &&
		 fd->change->type != FILEDATA_CHANGE_UNSPECIFIED &&
		 fd->change->type != FILEDATA_CHANGE_WRITE_METADATA &&
		 !access_file(fd->path, W_OK))
		{
		ret |= CHANGE_WARN_NO_WRITE_PERM;
		DEBUG_1("Change checked: no write permission: %s", fd->path);
		}
	/* WRITE_METADATA is special because it can be configured to silently write to ~/.geeqie/...
	   - that means that there are no hard errors and warnings can be disabled
	   - the destination is determined during the check
	*/
	else if (fd->change->type == FILEDATA_CHANGE_WRITE_METADATA)
		{
		/* determine destination file */
		gboolean have_dest = FALSE;
		gchar *dest_dir = NULL;
		
		if (options->metadata.save_in_image_file)
			{
			if (file_data_can_write_directly(fd)) 
				{
				/* we can write the file directly */
				if (access_file(fd->path, W_OK))
					{
					have_dest = TRUE;
					}
				else
					{
					if (options->metadata.warn_on_write_problems)
						{
						ret |= CHANGE_WARN_NO_WRITE_PERM;
						DEBUG_1("Change checked: file is not writable: %s", fd->path);
						}
					}
				}
			else if (file_data_can_write_sidecar(fd)) 
				{
				/* we can write sidecar */
				gchar *sidecar = file_data_get_sidecar_path(fd, FALSE);
				if (access_file(sidecar, W_OK) || (!isname(sidecar) && access_file(dir, W_OK)))
					{
					file_data_update_ci_dest(fd, sidecar);
					have_dest = TRUE;
					}
				else
					{
					if (options->metadata.warn_on_write_problems)
						{
						ret |= CHANGE_WARN_NO_WRITE_PERM;
						DEBUG_1("Change checked: file is not writable: %s", sidecar);
						}
					}
				g_free(sidecar);
				}
			}
		
		if (!have_dest)
			{
			/* write private metadata file under ~/.geeqie */

			/* If an existing metadata file exists, we will try writing to
			 * it's location regardless of the user's preference.
			 */
			gchar *metadata_path = cache_find_location(CACHE_TYPE_XMP_METADATA, fd->path);
			if (!metadata_path) metadata_path = cache_find_location(CACHE_TYPE_METADATA, fd->path);
			
			if (metadata_path && !access_file(metadata_path, W_OK))
				{
				g_free(metadata_path);
				metadata_path = NULL;
				}

			if (!metadata_path)
				{
				mode_t mode = 0755;

				dest_dir = cache_get_location(CACHE_TYPE_METADATA, fd->path, FALSE, &mode);
				if (recursive_mkdir_if_not_exists(dest_dir, mode))
					{
					gchar *filename = g_strconcat(fd->name, options->metadata.save_legacy_format ? GQ_CACHE_EXT_METADATA : GQ_CACHE_EXT_XMP_METADATA, NULL);
			
					metadata_path = g_build_filename(dest_dir, filename, NULL);
					g_free(filename);
					}
				}
			if (access_file(metadata_path, W_OK) || (!isname(metadata_path) && access_file(dest_dir, W_OK)))
				{
				file_data_update_ci_dest(fd, metadata_path);
				have_dest = TRUE;
				}
			else
				{
				ret |= CHANGE_NO_WRITE_PERM_DEST;
				DEBUG_1("Change checked: file is not writable: %s", metadata_path);
				}
			g_free(metadata_path);
			}
		g_free(dest_dir);
		}
		
	if (fd->change->dest && fd->change->type != FILEDATA_CHANGE_WRITE_METADATA)
		{
		gboolean same;
		gchar *dest_dir;
			
		same = (strcmp(fd->path, fd->change->dest) == 0);

		if (!same)
			{
			const gchar *dest_ext = extension_from_path(fd->change->dest);
			if (!dest_ext) dest_ext = "";

			if (g_ascii_strcasecmp(fd->extension, dest_ext) != 0)
				{
				ret |= CHANGE_WARN_CHANGED_EXT;
				DEBUG_1("Change checked: source and destination have different extensions: %s -> %s", fd->path, fd->change->dest);
				}
			}
		else
			{
			if (fd->change->type != FILEDATA_CHANGE_UNSPECIFIED) /* FIXME this is now needed for running editors */
		   		{
				ret |= CHANGE_WARN_SAME;
				DEBUG_1("Change checked: source and destination are the same: %s -> %s", fd->path, fd->change->dest);
				}
			}

		dest_dir = remove_level_from_path(fd->change->dest);

		if (!isdir(dest_dir))
			{
			ret |= CHANGE_NO_DEST_DIR;
			DEBUG_1("Change checked: destination dir does not exist: %s -> %s", fd->path, fd->change->dest);
			}
		else if (!access_file(dest_dir, W_OK))
			{
			ret |= CHANGE_NO_WRITE_PERM_DEST_DIR;
			DEBUG_1("Change checked: destination dir is readonly: %s -> %s", fd->path, fd->change->dest);
			}
		else if (!same)
			{
			if (isfile(fd->change->dest))
				{
				if (!access_file(fd->change->dest, W_OK))
					{
					ret |= CHANGE_NO_WRITE_PERM_DEST;
					DEBUG_1("Change checked: destination file exists and is readonly: %s -> %s", fd->path, fd->change->dest);
					}
				else
					{
					ret |= CHANGE_WARN_DEST_EXISTS;
					DEBUG_1("Change checked: destination exists: %s -> %s", fd->path, fd->change->dest);
					}
				}
			else if (isdir(fd->change->dest))
				{
				ret |= CHANGE_DEST_EXISTS;
				DEBUG_1("Change checked: destination exists: %s -> %s", fd->path, fd->change->dest);
				}
			}

		g_free(dest_dir);
		}
		
	fd->change->error = ret;
	if (ret == 0) DEBUG_1("Change checked: OK: %s", fd->path);

	g_free(dir);
	return ret;
}


gint file_data_sc_verify_ci(FileData *fd)
{
	GList *work;
	gint ret;

	ret = file_data_verify_ci(fd);

	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;

		ret |= file_data_verify_ci(sfd);
		work = work->next;
		}

	return ret;
}

gchar *file_data_get_error_string(gint error)
{
	GString *result = g_string_new("");

	if (error & CHANGE_NO_SRC)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("file or directory does not exist"));
		}

	if (error & CHANGE_DEST_EXISTS)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("destination already exists"));
		}

	if (error & CHANGE_NO_WRITE_PERM_DEST)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("destination can't be overwritten"));
		}

	if (error & CHANGE_NO_WRITE_PERM_DEST_DIR)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("destination directory is not writable"));
		}

	if (error & CHANGE_NO_DEST_DIR)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("destination directory does not exist"));
		}

	if (error & CHANGE_NO_WRITE_PERM_DIR)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("source directory is not writable"));
		}

	if (error & CHANGE_NO_READ_PERM)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("no read permission"));
		}

	if (error & CHANGE_WARN_NO_WRITE_PERM)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("file is readonly"));
		}

	if (error & CHANGE_WARN_DEST_EXISTS)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("destination already exists and will be overwritten"));
		}
		
	if (error & CHANGE_WARN_SAME)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("source and destination are the same"));
		}

	if (error & CHANGE_WARN_CHANGED_EXT)
		{
		if (result->len > 0) g_string_append(result, ", ");
		g_string_append(result, _("source and destination have different extension"));
		}

	return g_string_free(result, FALSE);
}

gint file_data_verify_ci_list(GList *list, gchar **desc, gboolean with_sidecars)
{
	GList *work;
	gint all_errors = 0;
	gint common_errors = ~0;
	gint num;
	gint *errors;
	gint i;
	
	if (!list) return 0;
	
	num = g_list_length(list);
	errors = g_new(int, num);
	work = list;
	i = 0;
	while (work)
		{
		FileData *fd;
		gint error;

		fd = work->data;
		work = work->next;
			
		error = with_sidecars ? file_data_sc_verify_ci(fd) : file_data_verify_ci(fd);
		all_errors |= error;
		common_errors &= error;
		
		errors[i] = error;
		
		i++;
		}
	
	if (desc && all_errors)
		{
		GList *work;
		GString *result = g_string_new("");
		
		if (common_errors)
			{
			gchar *str = file_data_get_error_string(common_errors);
			g_string_append(result, str);
			g_string_append(result, "\n");
			g_free(str);
			}
		
		work = list;
		i = 0;
		while (work)
			{
			FileData *fd;
			gint error;

			fd = work->data;
			work = work->next;
			
			error = errors[i] & ~common_errors;
			
			if (error)
				{
				gchar *str = file_data_get_error_string(error);
				g_string_append_printf(result, "%s: %s\n", fd->name, str);
				g_free(str);
				}
			i++;
			}
		*desc = g_string_free(result, FALSE);
		}

	g_free(errors);
	return all_errors;
}


/*
 * perform the change described by FileFataChangeInfo
 * it is used for internal operations,
 * this function actually operates with files on the filesystem
 * it should implement safe delete
 */

static gboolean file_data_perform_move(FileData *fd)
{
	g_assert(!strcmp(fd->change->source, fd->path));
	return move_file(fd->change->source, fd->change->dest);
}

static gboolean file_data_perform_copy(FileData *fd)
{
	g_assert(!strcmp(fd->change->source, fd->path));
	return copy_file(fd->change->source, fd->change->dest);
}

static gboolean file_data_perform_delete(FileData *fd)
{
	if (isdir(fd->path) && !islink(fd->path))
		return rmdir_utf8(fd->path);
	else
		if (options->file_ops.safe_delete_enable)
			return file_util_safe_unlink(fd->path);
		else
			return unlink_file(fd->path);
}

gboolean file_data_perform_ci(FileData *fd)
{
	FileDataChangeType type = fd->change->type;

	switch (type)
		{
		case FILEDATA_CHANGE_MOVE:
			return file_data_perform_move(fd);
		case FILEDATA_CHANGE_COPY:
			return file_data_perform_copy(fd);
		case FILEDATA_CHANGE_RENAME:
			return file_data_perform_move(fd); /* the same as move */
		case FILEDATA_CHANGE_DELETE:
			return file_data_perform_delete(fd);
		case FILEDATA_CHANGE_WRITE_METADATA:
			return metadata_write_perform(fd);
		case FILEDATA_CHANGE_UNSPECIFIED:
			/* nothing to do here */
			break;
		}
	return TRUE;
}



gboolean file_data_sc_perform_ci(FileData *fd)
{
	GList *work;
	gboolean ret = TRUE;
	FileDataChangeType type = fd->change->type;
	
	if (!file_data_sc_check_ci(fd, type)) return FALSE;

	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		
		if (!file_data_perform_ci(sfd)) ret = FALSE;
		work = work->next;
		}
	
	if (!file_data_perform_ci(fd)) ret = FALSE;
	
	return ret;
}

/*
 * updates FileData structure according to FileDataChangeInfo
 */

gboolean file_data_apply_ci(FileData *fd)
{
	FileDataChangeType type = fd->change->type;

	/* FIXME delete ?*/
	if (type == FILEDATA_CHANGE_MOVE || type == FILEDATA_CHANGE_RENAME)
		{
		DEBUG_1("planned change: applying %s -> %s", fd->change->dest, fd->path);
		file_data_planned_change_remove(fd);
		
		if (g_hash_table_lookup(file_data_pool, fd->change->dest))
			{
			/* this change overwrites another file which is already known to other modules
			   renaming fd would create duplicate FileData structure
			   the best thing we can do is nothing
			   FIXME: maybe we could copy stuff like marks
			*/
			DEBUG_1("can't rename fd, target exists %s -> %s", fd->change->dest, fd->path);
			}
		else
			{
			file_data_set_path(fd, fd->change->dest);
			}
		}
	file_data_increment_version(fd);
	file_data_send_notification(fd, NOTIFY_TYPE_CHANGE);
	
	return TRUE;
}

gboolean file_data_sc_apply_ci(FileData *fd)
{
	GList *work;
	FileDataChangeType type = fd->change->type;
	
	if (!file_data_sc_check_ci(fd, type)) return FALSE;

	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		
		file_data_apply_ci(sfd);
		work = work->next;
		}
	
	file_data_apply_ci(fd);
	
	return TRUE;
}

/*
 * notify other modules about the change described by FileFataChangeInfo
 */

/* might use file_maint_ functions for now, later it should be changed to a system of callbacks
   FIXME do we need the ignore_list? It looks like a workaround for ineffective
   implementation in view_file_list.c */




typedef struct _NotifyData NotifyData;

struct _NotifyData {
	FileDataNotifyFunc func;
	gpointer data;
	NotifyPriority priority;
};

static GList *notify_func_list = NULL;

static gint file_data_notify_sort(gconstpointer a, gconstpointer b)
{
	NotifyData *nda = (NotifyData *)a;
	NotifyData *ndb = (NotifyData *)b;

	if (nda->priority < ndb->priority) return -1;
	if (nda->priority > ndb->priority) return 1;
	return 0;
}

gboolean file_data_register_notify_func(FileDataNotifyFunc func, gpointer data, NotifyPriority priority)
{
	NotifyData *nd;
	
	nd = g_new(NotifyData, 1);
	nd->func = func;
	nd->data = data;
	nd->priority = priority;

	notify_func_list = g_list_insert_sorted(notify_func_list, nd, file_data_notify_sort);
	DEBUG_1("Notify func registered: %p", nd);
	
	return TRUE;
}

gboolean file_data_unregister_notify_func(FileDataNotifyFunc func, gpointer data)
{
	GList *work = notify_func_list;
	
	while (work)
		{
		NotifyData *nd = (NotifyData *)work->data;
	
		if (nd->func == func && nd->data == data)
			{
			notify_func_list = g_list_delete_link(notify_func_list, work);
			g_free(nd);
			DEBUG_1("Notify func unregistered: %p", nd);
			return TRUE;
			}
		work = work->next;
		}

	return FALSE;
}


void file_data_send_notification(FileData *fd, NotifyType type)
{
	GList *work = notify_func_list;

	while (work)
		{
		NotifyData *nd = (NotifyData *)work->data;
		
		DEBUG_1("Notify func calling: %p %s", nd, fd->path);
		nd->func(fd, type, nd->data);
		work = work->next;
		}
}

static GHashTable *file_data_monitor_pool = NULL;
static gint realtime_monitor_id = -1;

static void realtime_monitor_check_cb(gpointer key, gpointer value, gpointer data)
{
	FileData *fd = key;

	file_data_check_changed_files(fd);
	
	DEBUG_1("monitor %s", fd->path);
}

static gboolean realtime_monitor_cb(gpointer data)
{
	if (!options->update_on_time_change) return TRUE;
	g_hash_table_foreach(file_data_monitor_pool, realtime_monitor_check_cb, NULL);
	return TRUE;
}

gboolean file_data_register_real_time_monitor(FileData *fd)
{
	gint count;
	
	file_data_ref(fd);
	
	if (!file_data_monitor_pool)
		file_data_monitor_pool = g_hash_table_new(g_direct_hash, g_direct_equal);
	
	count = GPOINTER_TO_INT(g_hash_table_lookup(file_data_monitor_pool, fd));

	DEBUG_1("Register realtime %d %s", count, fd->path);
	
	count++;
	g_hash_table_insert(file_data_monitor_pool, fd, GINT_TO_POINTER(count));
	
	if (realtime_monitor_id == -1)
		{
		realtime_monitor_id = g_timeout_add(5000, realtime_monitor_cb, NULL);
		}
	
	return TRUE;
}

gboolean file_data_unregister_real_time_monitor(FileData *fd)
{
	gint count;

	g_assert(file_data_monitor_pool);
	
	count = GPOINTER_TO_INT(g_hash_table_lookup(file_data_monitor_pool, fd));
	
	DEBUG_1("Unregister realtime %d %s", count, fd->path);
	
	g_assert(count > 0);
	
	count--;
	
	if (count == 0)
		g_hash_table_remove(file_data_monitor_pool, fd);
	else
		g_hash_table_insert(file_data_monitor_pool, fd, GINT_TO_POINTER(count));

	file_data_unref(fd);
	
	if (g_hash_table_size(file_data_monitor_pool) == 0)
		{
		g_source_remove(realtime_monitor_id);
		realtime_monitor_id = -1;
		return FALSE;
		}
	
	return TRUE;
}
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
