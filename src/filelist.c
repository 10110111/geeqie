/*
 * Geeqie
 * (C) 2006 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "main.h"
#include "filelist.h"

#include "cache.h"
#include "rcfile.h"
#include "thumb_standard.h"
#include "ui_fileops.h"


/*
 *-----------------------------------------------------------------------------
 * file filtering
 *-----------------------------------------------------------------------------
 */

static GList *filter_list = NULL;
static GList *extension_list = NULL;
static GList *sidecar_ext_list = NULL;

static GList *file_class_extension_list[FILE_FORMAT_CLASSES];

static gint sidecar_file_priority(const gchar *path);


gint ishidden(const gchar *name)
{
	if (name[0] != '.') return FALSE;
	if (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')) return FALSE;
	return TRUE;
}

static FilterEntry *filter_entry_new(const gchar *key, const gchar *description,
				     const gchar *extensions, FileFormatClass file_class, gint enabled)
{
	FilterEntry *fe;

	fe = g_new0(FilterEntry, 1);
	fe->key = g_strdup(key);
	fe->description = g_strdup(description);
	fe->extensions = g_strdup(extensions);
	fe->enabled = enabled;
	fe->file_class = file_class;
	
	return fe;
}

static void filter_entry_free(FilterEntry *fe)
{
	if (!fe) return;

	g_free(fe->key);
	g_free(fe->description);
	g_free(fe->extensions);
	g_free(fe);
}

GList *filter_get_list(void)
{
	return filter_list;
}

void filter_remove_entry(FilterEntry *fe)
{
	if (!g_list_find(filter_list, fe)) return;

	filter_list = g_list_remove(filter_list, fe);
	filter_entry_free(fe);
}

static gint filter_key_exists(const gchar *key)
{
	GList *work;

	if (!key) return FALSE;

	work = filter_list;
	while (work)
		{
		FilterEntry *fe = work->data;
		work = work->next;

		if (strcmp(fe->key, key) == 0) return TRUE;
		}

	return FALSE;
}

void filter_add(const gchar *key, const gchar *description, const gchar *extensions, FileFormatClass file_class, gint enabled)
{
	filter_list = g_list_append(filter_list, filter_entry_new(key, description, extensions, file_class, enabled));
}

void filter_add_unique(const gchar *description, const gchar *extensions, FileFormatClass file_class, gint enabled)
{
	gchar *key;
	gint n;

	key = g_strdup("user0");
	n = 1;
	while (filter_key_exists(key))
		{
		g_free(key);
		if (n > 999) return;
		key = g_strdup_printf("user%d", n);
		n++;
		}

	filter_add(key, description, extensions, file_class, enabled);
	g_free(key);
}

static void filter_add_if_missing(const gchar *key, const gchar *description, const gchar *extensions, FileFormatClass file_class, gint enabled)
{
	GList *work;

	if (!key) return;

	work = filter_list;
	while (work)
		{
		FilterEntry *fe = work->data;
		work = work->next;
		if (fe->key && strcmp(fe->key, key) == 0)
			{
			if (fe->file_class == FORMAT_CLASS_UNKNOWN)
				fe->file_class = file_class;	/* for compatibility */
			return;
			}
		}

	filter_add(key, description, extensions, file_class, enabled);
}

void filter_reset(void)
{
	GList *work;

	work = filter_list;
	while (work)
		{
		FilterEntry *fe = work->data;
		work = work->next;
		filter_entry_free(fe);
		}

	g_list_free(filter_list);
	filter_list = NULL;
}

void filter_add_defaults(void)
{
	GSList *list, *work;

	list = gdk_pixbuf_get_formats();
	work = list;
	while (work)
		{
		GdkPixbufFormat *format;
		gchar *name;
		gchar *desc;
		gchar **extensions;
		GString *filter = NULL;
		gint i;
		
		format = work->data;
		work = work->next;

		name = gdk_pixbuf_format_get_name(format);
		desc = gdk_pixbuf_format_get_description(format);
		extensions = gdk_pixbuf_format_get_extensions(format);

		i = 0;
		while (extensions[i])
			{
			if (!filter)
				{
				filter = g_string_new(".");
				filter = g_string_append(filter, extensions[i]);
				}
			else
				{
				filter = g_string_append(filter, ";.");
				filter = g_string_append(filter, extensions[i]);
				}
			i++;
			}

		if (debug) printf("loader reported [%s] [%s] [%s]\n", name, desc, filter->str);

		filter_add_if_missing(name, desc, filter->str, FORMAT_CLASS_IMAGE, TRUE);

		g_free(name);
		g_free(desc);
		g_strfreev(extensions);
		g_string_free(filter, TRUE);
		}
	g_slist_free(list);

	/* add defaults even if gdk-pixbuf does not have them, but disabled */
	filter_add_if_missing("jpeg", "JPEG group", ".jpg;.jpeg;.jpe", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("png", "Portable Network Graphic", ".png", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("tiff", "Tiff", ".tif;.tiff", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("pnm", "Packed Pixel formats", ".pbm;.pgm;.pnm;.ppm", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("gif", "Graphics Interchange Format", ".gif", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("xbm", "X bitmap", ".xbm", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("xpm", "X pixmap", ".xpm", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("bmp", "Bitmap", ".bmp", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("ico", "Icon file", ".ico;.cur", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("ras", "Raster", ".ras", FORMAT_CLASS_IMAGE, FALSE);
	filter_add_if_missing("svg", "Scalable Vector Graphics", ".svg", FORMAT_CLASS_IMAGE, FALSE);
	
	/* non-image files that might be desirable to show */
	filter_add_if_missing("xmp", "XMP sidecar", ".xmp", FORMAT_CLASS_META, TRUE);

	/* These are the raw camera formats with embedded jpeg/exif.
	 * (see format_raw.c and/or exiv2.cc)
	 */
	filter_add_if_missing("arw", "Sony raw format", ".arw;.srf;.sr2", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("crw", "Canon raw format", ".crw;.cr2", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("kdc", "Kodak raw format", ".kdc;.dcr", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("raf", "Fujifilm raw format", ".raf", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("mef", "Mamiya raw format", ".mef;.mos", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("mrw", "Minolta raw format", ".mrw", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("nef", "Nikon raw format", ".nef", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("orf", "Olympus raw format", ".orf", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("pef", "Pentax or Samsung raw format", ".pef;.ptx", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("dng", "Adobe Digital Negative raw format", ".dng", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("x3f", "Sigma raw format", ".x3f", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("raw", "Panasonic raw format", ".raw", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("r3d", "Red raw format", ".r3d", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("3fr", "Hasselblad raw format", ".3fr", FORMAT_CLASS_RAWIMAGE, TRUE);
	filter_add_if_missing("erf", "Epson raw format", ".erf", FORMAT_CLASS_RAWIMAGE, TRUE);
}

GList *filter_to_list(const gchar *extensions)
{
	GList *list = NULL;
	const gchar *p;

	if (!extensions) return NULL;

	p = extensions;
	while (*p != '\0')
		{
		const gchar *b;
		gint l = 0;

		b = p;
		while (*p != '\0' && *p != ';')
			{
			p++;
			l++;
			}
		list = g_list_append(list, g_strndup(b, l));
		if (*p == ';') p++;
		}

	return list;
}

void filter_rebuild(void)
{
	GList *work;
	gint i;

	string_list_free(extension_list);
	extension_list = NULL;

	for (i = 0; i < FILE_FORMAT_CLASSES; i++)
		{
		string_list_free(file_class_extension_list[i]);
		file_class_extension_list[i] = NULL;
		}

	work = filter_list;
	while (work)
		{
		FilterEntry *fe;

		fe = work->data;
		work = work->next;

		if (fe->enabled)
			{
			GList *ext;

			ext = filter_to_list(fe->extensions);
			if (ext) extension_list = g_list_concat(extension_list, ext);
			
			if (fe->file_class >= 0 && fe->file_class < FILE_FORMAT_CLASSES)
				{
				ext = filter_to_list(fe->extensions);
				if (ext) file_class_extension_list[fe->file_class] = g_list_concat(file_class_extension_list[fe->file_class], ext);
				}
			else
				{
				printf("WARNING: invalid file class %d\n", fe->file_class);
				}
			}
		}
}

gint filter_name_exists(const gchar *name)
{
	GList *work;
	gint ln;

	if (!extension_list || file_filter_disable) return TRUE;

	ln = strlen(name);
	work = extension_list;
	while (work)
		{
		gchar *filter = work->data;
		gint lf = strlen(filter);

		if (ln >= lf)
			{
			if (strncasecmp(name + ln - lf, filter, lf) == 0) return TRUE;
			}
		work = work->next;
		}

	return FALSE;
}

gint filter_file_class(const gchar *name, FileFormatClass file_class)
{
	GList *work;
	gint ln;

	if (file_class < 0 || file_class >= FILE_FORMAT_CLASSES)
		{
		printf("WARNING: invalid file class %d\n", file_class);
		return FALSE;
		}

	ln = strlen(name);
	work = file_class_extension_list[file_class];
	while (work)
		{
		gchar *filter = work->data;
		gint lf = strlen(filter);

		if (ln >= lf)
			{
			if (strncasecmp(name + ln - lf, filter, lf) == 0) return TRUE;
			}
		work = work->next;
		}

	return FALSE;
}

void filter_write_list(SecureSaveInfo *ssi)
{
	GList *work;

	work = filter_list;
	while (work)
		{
		FilterEntry *fe = work->data;
		work = work->next;
		
		gchar *extensions = escquote_value(fe->extensions);
		gchar *description = escquote_value(fe->description);

		secure_fprintf(ssi, "filter_ext: \"%s%s\" %s %s\n",
			       (fe->enabled) ? "" : "#",
			       fe->key, extensions, description);
		g_free(extensions);
		g_free(description);
		}
}

void filter_parse(const gchar *text)
{
	const gchar *p;
	gchar *key;
	gchar *ext;
	gchar *desc;
	gint enabled = TRUE;

	if (!text || text[0] != '"') return;

	key = quoted_value(text, &p);
	if (!key) return;

	ext = quoted_value(p, &p);
	desc = quoted_value(p, &p);

	if (key && key[0] == '#')
		{
		gchar *tmp;
		tmp = g_strdup(key + 1);
		g_free(key);
		key = tmp;

		enabled = FALSE;
		}

	if (key && strlen(key) > 0 && ext) filter_add(key, desc, ext, FORMAT_CLASS_UNKNOWN, enabled);

	g_free(key);
	g_free(ext);
	g_free(desc);
}

GList *path_list_filter(GList *list, gint is_dir_list)
{
	GList *work;

	if (!is_dir_list && file_filter_disable && show_dot_files) return list;

	work = list;
	while (work)
		{
		gchar *name = work->data;
		const gchar *base;

		base = filename_from_path(name);

		if ((!show_dot_files && ishidden(base)) ||
		    (!is_dir_list && !filter_name_exists(base)) ||
		    (is_dir_list && base[0] == '.' && (strcmp(base, GQVIEW_CACHE_LOCAL_THUMB) == 0 ||
						       strcmp(base, GQVIEW_CACHE_LOCAL_METADATA) == 0)) )
			{
			GList *link = work;
			work = work->next;
			list = g_list_remove_link(list, link);
			g_free(name);
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
 * sidecar extension list
 *-----------------------------------------------------------------------------
 */

static GList *sidecar_ext_get_list(void)
{
	return sidecar_ext_list;
}

void sidecar_ext_parse(const gchar *text, gint quoted)
{
	GList *work;
	gchar *value;

	work = sidecar_ext_list;
	while (work)
		{
		gchar *ext = work->data;
		work = work->next;
		g_free(ext);
		}
	g_list_free(sidecar_ext_list);
	sidecar_ext_list = NULL;
	
	if (quoted)
		value = quoted_value(text, NULL);
	else
		value = g_strdup(text);

	if (value == NULL) return;

	sidecar_ext_list = filter_to_list(value);
	
	g_free(value);
}

void sidecar_ext_write(SecureSaveInfo *ssi)
{
	secure_fprintf(ssi, "\nsidecar_ext: \"%s\"\n", sidecar_ext_to_string());
}

char *sidecar_ext_to_string()
{
	GList *work;
	GString *str = g_string_new("");

	work = sidecar_ext_list;
	while (work)
		{
		gchar *ext = work->data;
		work = work->next;
		g_string_append(str, ext);
		if (work) g_string_append(str, ";");
		}
	return g_string_free(str, FALSE);
}

void sidecar_ext_add_defaults()
{
	sidecar_ext_parse(".jpg;.cr2;.nef;.crw;.xmp", FALSE);
}

/*
 *-----------------------------------------------------------------------------
 * path list recursive
 *-----------------------------------------------------------------------------
 */

static gint path_list_sort_cb(gconstpointer a, gconstpointer b)
{
	return CASE_SORT((gchar *)a, (gchar *)b);
}

GList *path_list_sort(GList *list)
{
	return g_list_sort(list, path_list_sort_cb);
}

static void path_list_recursive_append(GList **list, GList *dirs)
{
	GList *work;

	work = dirs;
	while (work)
		{
		const gchar *path = work->data;
		GList *f = NULL;
		GList *d = NULL;

		if (path_list(path, &f, &d))
			{
			f = path_list_filter(f, FALSE);
			f = path_list_sort(f);
			*list = g_list_concat(*list, f);

			d = path_list_filter(d, TRUE);
			d = path_list_sort(d);
			path_list_recursive_append(list, d);
			path_list_free(d);
			}

		work = work->next;
		}
}

GList *path_list_recursive(const gchar *path)
{
	GList *list = NULL;
	GList *d = NULL;

	if (!path_list(path, &list, &d)) return NULL;
	list = path_list_filter(list, FALSE);
	list = path_list_sort(list);

	d = path_list_filter(d, TRUE);
	d = path_list_sort(d);
	path_list_recursive_append(&list, d);
	path_list_free(d);

	return list;
}

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

	if (debug) printf("file_data_new: '%s' %d\n", path_utf8, check_sidecars);
	
	if (!file_data_pool)
		file_data_pool = g_hash_table_new (g_str_hash, g_str_equal);
	
	fd = g_hash_table_lookup(file_data_pool, path_utf8);
	if (fd)
		{
		file_data_check_changed_files(fd, st);
		if (debug) printf("file_data_pool hit: '%s'\n", fd->path);
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
	if (debug) printf("file_data_unref: '%s'\n", fd->path);
	fd->ref--;
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
		
		if (debug) printf("file_data_unref: deleting '%s', parent '%s'\n", fd->path, parent->path);
		
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
		if (show_dot_files || !ishidden(name))
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
					    strcmp(name, GQVIEW_CACHE_LOCAL_THUMB) != 0 &&
					    strcmp(name, GQVIEW_CACHE_LOCAL_METADATA) != 0 &&
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

	if (!is_dir_list && file_filter_disable && show_dot_files) return list;

	work = list;
	while (work)
		{
		FileData *fd = (FileData *)(work->data);
		const gchar *name = fd->name;

		if ((!show_dot_files && ishidden(name)) ||
		    (!is_dir_list && !filter_name_exists(name)) ||
		    (is_dir_list && name[0] == '.' && (strcmp(name, GQVIEW_CACHE_LOCAL_THUMB) == 0 ||
						       strcmp(name, GQVIEW_CACHE_LOCAL_METADATA) == 0)) )
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
