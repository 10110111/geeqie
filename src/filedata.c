/*
 * Geeqie
 * (C) 2006 John Ellis
 * Copyright (C) 2008 The Geeqie Team
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
#include "debug.h"
#include "rcfile.h"
#include "secure_save.h"
#include "thumb_standard.h"
#include "ui_fileops.h"


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
		return g_strdup_printf(_("%.1f K"), (double)size / 1024.0);
		}
	if (size < (gint64)1073741824)
		{
		return g_strdup_printf(_("%.1f MB"), (double)size / 1048576.0);
		}

	/* to avoid overflowing the double, do division in two steps */
	size /= 1048576;
	return g_strdup_printf(_("%.1f GB"), (double)size / 1024.0);
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
		printf("Error converting locale strftime to UTF-8: %s\n", error->message);
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


static void file_data_set_path(FileData *fd, const gchar *path)
{

	if (strcmp(path, "/") == 0)
		{
		fd->path = g_strdup(path);
		fd->name = fd->path;
		fd->extension = fd->name + 1;
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
		return;
		}
	else if (strcmp(fd->name, ".") == 0)
		{
		g_free(fd->path);
		fd->path = remove_level_from_path(path);
		fd->name = ".";
		fd->extension = fd->name + 1;
		return;
		}

	fd->extension = extension_from_path(fd->path);
	if (fd->extension == NULL)
		fd->extension = fd->name + strlen(fd->name);
}

static void file_data_check_changed_files(FileData *fd, struct stat *st)
{
	GList *work;
	if (fd->size != st->st_size ||
	    fd->date != st->st_mtime)
		{
		fd->size = st->st_size;
		fd->date = st->st_mtime;
		if (fd->pixbuf) g_object_unref(fd->pixbuf);
		fd->pixbuf = NULL;
		}

	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		struct stat st;

		if (!stat_utf8(sfd->path, &st))
			{
			file_data_disconnect_sidecar_file(fd, sfd);
			}

		file_data_check_changed_files(sfd, &st);
		work = work->next;
		}
}

static GHashTable *file_data_pool = NULL;

static FileData *file_data_new(const gchar *path_utf8, struct stat *st, gboolean check_sidecars)
{
	FileData *fd;

	DEBUG_2("file_data_new: '%s' %d", path_utf8, check_sidecars);

	if (!file_data_pool)
		file_data_pool = g_hash_table_new(g_str_hash, g_str_equal);

	fd = g_hash_table_lookup(file_data_pool, path_utf8);
	if (fd)
		{
		file_data_check_changed_files(fd, st);
		DEBUG_2("file_data_pool hit: '%s'", fd->path);
		return file_data_ref(fd);
		}

	fd = g_new0(FileData, 1);

	file_data_set_path(fd, path_utf8);

	fd->original_path = g_strdup(path_utf8);
	fd->size = st->st_size;
	fd->date = st->st_mtime;
	fd->pixbuf = NULL;
	fd->sidecar_files = NULL;
	fd->ref = 1;
	fd->magick = 0x12345678;

	g_hash_table_insert(file_data_pool, fd->original_path, fd);

	if (check_sidecars && sidecar_file_priority(fd->extension))
		file_data_check_sidecars(fd);
	return fd;
}

static void file_data_check_sidecars(FileData *fd)
{
	int base_len = fd->extension - fd->path;
	GString *fname = g_string_new_len(fd->path, base_len);
	FileData *parent_fd = NULL;
	GList *work = sidecar_ext_get_list();
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

		if (strcmp(ext, fd->extension) == 0)
			{
			new_fd = fd; /* processing the original file */
			}
		else
			{
			struct stat nst;
			g_string_truncate(fname, base_len);
			g_string_append(fname, ext);

			if (!stat_utf8(fname->str, &nst))
				continue;

			new_fd = file_data_new(fname->str, &nst, FALSE);
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
	if(!g_list_find(target->sidecar_files, sfd))
		target->sidecar_files = g_list_prepend(target->sidecar_files, sfd);
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



FileData *file_data_ref(FileData *fd)
{
	if (fd == NULL) return NULL;

//	return g_memdup(fd, sizeof(FileData));
	g_assert(fd->magick == 0x12345678);
	fd->ref++;
	return fd;
}

static void file_data_free(FileData *fd)
{
	g_assert(fd->magick == 0x12345678);
	g_assert(fd->ref == 0);

	g_hash_table_remove(file_data_pool, fd->original_path);

	g_free(fd->path);
	g_free(fd->original_path);
	if (fd->pixbuf) g_object_unref(fd->pixbuf);


	g_assert(fd->sidecar_files == NULL); /* sidecar files must be freed before calling this */

	file_data_change_info_free(NULL, fd);
	g_free(fd);
}

void file_data_unref(FileData *fd)
{
	if (fd == NULL) return;
	g_assert(fd->magick == 0x12345678);

	fd->ref--;
	DEBUG_2("file_data_unref (%d): '%s'", fd->ref, fd->path);

	if (fd->ref == 0)
		{
		FileData *parent = fd->parent ? fd->parent : fd;

		GList *work;

		if (parent->ref > 0)
			return;

		work = parent->sidecar_files;
		while (work)
			{
			FileData *sfd = work->data;
			if (sfd->ref > 0)
				return;
			work = work->next;
			}

		/* none of parent/children is referenced, we can free everything */

		DEBUG_2("file_data_unref: deleting '%s', parent '%s'", fd->path, parent->path);

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

	target->sidecar_files = g_list_remove(target->sidecar_files, sfd);
	sfd->parent = NULL;

	if (sfd->ref == 0) {
		file_data_free(sfd);
		return NULL;
	}

	return sfd;
}

/* compare name without extension */
gint file_data_compare_name_without_ext(FileData *fd1, FileData *fd2)
{
	size_t len1 = fd1->extension - fd1->name;
	size_t len2 = fd2->extension - fd2->name;

	if (len1 < len2) return -1;
	if (len1 > len2) return 1;

	return strncmp(fd1->name, fd2->name, len1);
}

void file_data_do_change(FileData *fd)
{
//FIXME sidecars
	g_assert(fd->change);
	g_free(fd->path);
	g_hash_table_remove(file_data_pool, fd->original_path);
	g_free(fd->original_path);
	file_data_set_path(fd, fd->change->dest);
	fd->original_path = g_strdup(fd->change->dest);
	g_hash_table_insert(file_data_pool, fd->original_path, fd);

}

gboolean file_data_add_change_info(FileData *fd, FileDataChangeType type, const gchar *src, const gchar *dest)
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

void file_data_change_info_free(FileDataChangeInfo *fdci, FileData *fd)
{
	if (!fdci && fd)
		fdci = fd->change;

	if (!fdci)
		return;

	g_free(fdci->source);
	g_free(fdci->dest);

	g_free(fdci);

	if (fd)
		fd->change = NULL;
}




/*
 *-----------------------------------------------------------------------------
 * sidecar file info struct
 *-----------------------------------------------------------------------------
 */



static gint sidecar_file_priority(const gchar *path)
{
	const char *extension = extension_from_path(path);
	int i = 1;
	GList *work;
	if (extension == NULL)
		return 0;

	work = sidecar_ext_get_list();

	while (work) {
		gchar *ext = work->data;
		work = work->next;
		if (strcmp(extension, ext) == 0) return i;
		i++;
	}
	return 0;
}

gchar *sidecar_file_data_list_to_string(FileData *fd)
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
 *-----------------------------------------------------------------------------
 * load file list
 *-----------------------------------------------------------------------------
 */

static SortType filelist_sort_method = SORT_NONE;
static gint filelist_sort_ascend = TRUE;


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
		case SORT_SIZE:
			if (fa->size < fb->size) return -1;
			if (fa->size > fb->size) return 1;
			return CASE_SORT(fa->name, fb->name); /* fall back to name */
			break;
		case SORT_TIME:
			if (fa->date < fb->date) return -1;
			if (fa->date > fb->date) return 1;
			return CASE_SORT(fa->name, fb->name); /* fall back to name */
			break;
#ifdef HAVE_STRVERSCMP
		case SORT_NUMBER:
			return strverscmp(fa->name, fb->name);
			break;
#endif
		case SORT_NAME:
		default:
			return CASE_SORT(fa->name, fb->name);
			break;
		}
}

gint filelist_sort_compare_filedata_full(FileData *fa, FileData *fb, SortType method, gint ascend)
{
	filelist_sort_method = method;
	filelist_sort_ascend = ascend;
	return filelist_sort_compare_filedata(fa, fb);
}

static gint filelist_sort_file_cb(void *a, void *b)
{
	return filelist_sort_compare_filedata(a, b);
}

GList *filelist_sort_full(GList *list, SortType method, gint ascend, GCompareFunc cb)
{
	filelist_sort_method = method;
	filelist_sort_ascend = ascend;
	return g_list_sort(list, cb);
}

GList *filelist_insert_sort_full(GList *list, void *data, SortType method, gint ascend, GCompareFunc cb)
{
	filelist_sort_method = method;
	filelist_sort_ascend = ascend;
	return g_list_insert_sorted(list, data, cb);
}

GList *filelist_sort(GList *list, SortType method, gint ascend)
{
	return filelist_sort_full(list, method, ascend, (GCompareFunc) filelist_sort_file_cb);
}

GList *filelist_insert_sort(GList *list, FileData *fd, SortType method, gint ascend)
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

static gint filelist_read_real(const gchar *path, GList **files, GList **dirs, gint follow_symlinks)
{
	DIR *dp;
	struct dirent *dir;
	struct stat ent_sbuf;
	gchar *pathl;
	GList *dlist;
	GList *flist;

	dlist = NULL;
	flist = NULL;

	pathl = path_from_utf8(path);
	if (!pathl || (dp = opendir(pathl)) == NULL)
		{
		g_free(pathl);
		if (files) *files = NULL;
		if (dirs) *dirs = NULL;
		return FALSE;
		}

	/* root dir fix */
	if (pathl[0] == '/' && pathl[1] == '\0')
		{
		g_free(pathl);
		pathl = g_strdup("");
		}

	while ((dir = readdir(dp)) != NULL)
		{
		gchar *name = dir->d_name;
		if (options->file_filter.show_hidden_files || !ishidden(name))
			{
			gchar *filepath = g_strconcat(pathl, "/", name, NULL);
			if ((follow_symlinks ?
				stat(filepath, &ent_sbuf) :
				lstat(filepath, &ent_sbuf)) >= 0)
				{
				if (S_ISDIR(ent_sbuf.st_mode))
					{
					/* we ignore the .thumbnails dir for cleanliness */
					if ((dirs) &&
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
					if ((files) && filter_name_exists(name))
						{
						flist = g_list_prepend(flist, file_data_new_local(filepath, &ent_sbuf, TRUE));
						}
					}
				}
			g_free(filepath);
			}
		}

	closedir(dp);

	g_free(pathl);

	flist = filelist_filter_out_sidecars(flist);

	if (dirs) *dirs = dlist;
	if (files) *files = flist;

	return TRUE;
}

gint filelist_read(const gchar *path, GList **files, GList **dirs)
{
	return filelist_read_real(path, files, dirs, TRUE);
}

gint filelist_read_lstat(const gchar *path, GList **files, GList **dirs)
{
	return filelist_read_real(path, files, dirs, FALSE);
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

GList *filelist_filter(GList *list, gint is_dir_list)
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
			work = work->next;
			list = g_list_remove_link(list, link);
			file_data_unref(fd);
			g_list_free(link);
			}
		else
			{
			work = work->next;
			}
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
		const gchar *path = fd->path;
		GList *f = NULL;
		GList *d = NULL;

		if (filelist_read(path, &f, &d))
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

GList *filelist_recursive(const gchar *path)
{
	GList *list = NULL;
	GList *d = NULL;

	if (!filelist_read(path, &list, &d)) return NULL;
	list = filelist_filter(list, FALSE);
	list = filelist_sort_path(list);

	d = filelist_filter(d, TRUE);
	d = filelist_sort_path(d);
	filelist_recursive_append(&list, d);
	filelist_free(d);

	return list;
}



/*
 * file_data    - operates on the given fd
 * file_data_sc - operates on the given fd + sidecars - all fds linked via fd->sidecar_files or fd->parent
 */


/* return list of sidecar file extensions in a string */
gchar *file_data_sc_list_to_string(FileData *fd); // now gchar *sidecar_file_data_list_to_string(FileData *fd)


/* disables / enables grouping for particular file, sends UPDATE notification */
void file_data_disable_grouping(FileData *fd); // now file_data_disconnect_sidecar_file, broken
void file_data_disable_grouping(FileData *fd);

/* runs stat on a file and sends UPDATE notification if it has been changed */
void file_data_sc_update(FileData *fd);




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
   MOVE - patch is changed, name may be changed too
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

void file_data_free_ci(FileData *fd)
{
	FileDataChangeInfo *fdci = fd->change;

	if (!fdci)
		return;

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
	
	if (!fd->change) return FALSE;
	if (fd->change->type != type) return FALSE;
	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		if (!sfd->change) return FALSE;
		if (sfd->change->type != type) return FALSE;
		work = work->next;
		}
	return TRUE;
}


gboolean file_data_sc_add_ci_copy(FileData *fd, gchar *dest_path)
{
	if (!file_data_sc_add_ci(fd, FILEDATA_CHANGE_COPY)) return FALSE;
	file_data_sc_update_ci_copy(fd, dest_path);
	return TRUE;
}

gboolean file_data_sc_add_ci_move(FileData *fd, gchar *dest_path)
{
	if (!file_data_sc_add_ci(fd, FILEDATA_CHANGE_MOVE)) return FALSE;
	file_data_sc_update_ci_move(fd, dest_path);
	return TRUE;
}

gboolean file_data_sc_add_ci_rename(FileData *fd, gchar *dest_path)
{
	if (!file_data_sc_add_ci(fd, FILEDATA_CHANGE_RENAME)) return FALSE;
	file_data_sc_update_ci_rename(fd, dest_path);
	return TRUE;
}

gboolean file_data_sc_add_ci_delete(FileData *fd)
{
	return file_data_sc_add_ci(fd, FILEDATA_CHANGE_DELETE);
}

gboolean file_data_sc_add_ci_update(FileData *fd)
{
	return file_data_sc_add_ci(fd, FILEDATA_CHANGE_UPDATE);
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


/* 
 * update existing fd->change, it will be used from dialog callbacks for interactive editing
 * fails if fd->change does not exist or the change type does not match
 */

static void file_data_update_ci_dest(FileData *fd, gchar *dest_path)
{
	g_free(fd->change->dest);
	fd->change->dest = g_strdup(dest_path);
}

static void file_data_update_ci_dest_preserve_ext(FileData *fd, gchar *dest_path)
{
	const char *extension = extension_from_path(fd->change->source);
	g_free(fd->change->dest);
	fd->change->dest = g_strdup_printf("%*s%s", (int)(extension_from_path(dest_path) - dest_path), dest_path, extension);
}

static void file_data_sc_update_ci(FileData *fd, gchar *dest_path)
{
	GList *work;
	if (fd->parent) fd = fd->parent;
	
	file_data_update_ci_dest(fd, dest_path);
	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		file_data_update_ci_dest_preserve_ext(sfd, dest_path);
		work = work->next;
		}
}

gint file_data_sc_update_ci_copy(FileData *fd, gchar *dest_path)
{
	if (!file_data_sc_check_ci(fd, FILEDATA_CHANGE_COPY)) return FALSE;
	file_data_sc_update_ci(fd, dest_path);
	return TRUE;
}
	
gint file_data_sc_update_ci_move(FileData *fd, gchar *dest_path)
{
	if (!file_data_sc_check_ci(fd, FILEDATA_CHANGE_MOVE)) return FALSE;
	file_data_sc_update_ci(fd, dest_path);
	return TRUE;
}

gint file_data_sc_update_ci_rename(FileData *fd, gchar *dest_path)
{
	if (!file_data_sc_check_ci(fd, FILEDATA_CHANGE_RENAME)) return FALSE;
	file_data_sc_update_ci(fd, dest_path);
	return TRUE;
}



/*
 * check dest paths - dest image exists, etc.
 * returns FIXME
 * it should detect all possible problems with the planned operation
 */
 
gint file_data_sc_check_ci_dest(FileData *fd)
{
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
	return unlink_file(fd->path);
}

static gboolean file_data_perform_ci(FileData *fd)
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
		case FILEDATA_CHANGE_UPDATE:
			/* notring to do here */
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
 
static void file_data_apply_ci(FileData *fd)
{
	FileDataChangeType type = fd->change->type;
	/* FIXME delete ?*/
	if (type == FILEDATA_CHANGE_MOVE || type == FILEDATA_CHANGE_COPY || type == FILEDATA_CHANGE_RENAME)
		{
		g_free(fd->path);
		g_hash_table_remove(file_data_pool, fd->original_path);
		g_free(fd->original_path);
		file_data_set_path(fd, fd->change->dest);
		fd->original_path = g_strdup(fd->change->dest);
		g_hash_table_insert(file_data_pool, fd->original_path, fd);
		}
}

gint file_data_sc_apply_ci(FileData *fd) // now file_data_do_change
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

void file_data_sc_send_notification(FileData *fd)
{
}


