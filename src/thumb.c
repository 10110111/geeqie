/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"
#include "icons/img_unknown.xpm" /* fixme! duplicate, included in image.c too */

#define THUMBNAIL_CACHE_DIR "/.gqview_thmb"

static guchar *load_xv_thumbnail(gchar *filename, gint *widthp, gint *heightp);
static void normalize_thumb(gint *width, gint *height);
static gint get_xv_thumbnail(gchar *thumb_filename, GdkPixmap **thumb_pixmap, GdkBitmap **thumb_mask);

/*
 *-----------------------------------------------------------------------------
 * thumbnail routines: creation, caching, and maintenance (public)
 *-----------------------------------------------------------------------------
 */

gint create_thumbnail(gchar *path, GdkPixmap **thumb_pixmap, GdkBitmap **thumb_mask)
{
	gint width, height;
	gint space;
	GdkImlibImage *thumb = NULL;
	GdkImlibImage *image = NULL;
	gint cached = FALSE;

	if (debug) printf("Gen thumbnail:%s\n",path);

	/* if xvpics enabled, check for that first */
	if (use_xvpics_thumbnails)
		{
		space = get_xv_thumbnail(path, thumb_pixmap, thumb_mask);
		if (space != -1)
			{
			if (debug) printf("XV thumbnail found, loaded\n");
			return space;
			}
		}

	/* start load from cache */

	if (enable_thumb_caching)
		{
		gchar *cache_path;
		cache_path = g_strconcat(homedir(), THUMBNAIL_CACHE_DIR, path, ".png", NULL);

		if (isfile(cache_path) && filetime(cache_path) >= filetime(path))
			{
			if (debug) printf("Found in cache:%s\n", path);
			image = gdk_imlib_load_image(cache_path);
			if (image && image->rgb_width != thumb_max_width && image->rgb_height != thumb_max_height)
				{
				if (debug) printf("Thumbnail size may have changed, reloading:%s\n", path);
				unlink(cache_path);
				gdk_imlib_destroy_image(image);
				image = gdk_imlib_load_image(path);
				}
			else
				{
				cached = TRUE;
				}
			}
		else
			image = gdk_imlib_load_image(path);
		
		}
	else
		image = gdk_imlib_load_image(path);

	if (!image)
		{
		image = gdk_imlib_create_image_from_xpm_data((gchar **)img_unknown_xpm);
		cached = TRUE; /* no need to save a thumbnail of the unknown pixmap */
		}

	if (image)
		{
		if (image->rgb_width > thumb_max_width || image->rgb_height > thumb_max_height)
			{
			if (((float)thumb_max_width / image->rgb_width) < ((float)thumb_max_height / image->rgb_height))
				{
				width = thumb_max_width;
				height = (float)width / image->rgb_width * image->rgb_height;
				if (height < 1) height = 1;
				}
			else
				{
				height = thumb_max_height;
				width = (float)height / image->rgb_height * image->rgb_width;
				if (width < 1) width = 1;
				}
			}
		else
			{
			width = image->rgb_width;
			height = image->rgb_height;
			cached = TRUE; /* don't cache images smaller than thumbnail size */
			}
		if (*thumb_pixmap) gdk_imlib_free_pixmap(*thumb_pixmap);
		*thumb_pixmap = NULL;
		*thumb_mask = NULL;

	/* start save cache */

		if (enable_thumb_caching && !cached)
			{
			gchar *thumb_path;
			gchar *base_dir;
			gchar *thumb_dir;
			gchar *image_dir;

			/* attempt at speed-up? move this here */
			thumb = gdk_imlib_clone_scaled_image(image, width, height);
			gdk_imlib_destroy_image(image);
			image = NULL;

			base_dir = g_strconcat(homedir(), THUMBNAIL_CACHE_DIR, NULL);
			if (!isdir(base_dir))
				{
				if (debug) printf("creating thumbnail dir:%s\n", base_dir);
				if (mkdir(base_dir, 0755) < 0)
					printf(_("create dir failed: %s\n"), base_dir);
				}

			image_dir = remove_level_from_path(path);
			thumb_dir = g_strconcat(base_dir, image_dir, NULL);
			g_free(image_dir);
			if (!isdir(thumb_dir))
				{
				gchar *p = thumb_dir;
				while (p[0] != '\0')
					{
					p++;
					if (p[0] == '/' || p[0] == '\0')
						{
						gint end = TRUE;
						if (p[0] != '\0')
							{
							p[0] = '\0';
							end = FALSE;
							}
						if (!isdir(thumb_dir))
							{
							if (debug) printf("creating sub dir:%s\n",thumb_dir);
							if (mkdir(thumb_dir, 0755) < 0)
								printf(_("create dir failed: %s\n"), thumb_dir);
							}
						if (!end) p[0] = '/';
						}
					}
				}
			g_free(thumb_dir);

			thumb_path = g_strconcat(base_dir, path, ".png", NULL);
			if (debug) printf("Saving thumb: %s\n",thumb_path);

			gdk_imlib_save_image(thumb, thumb_path, NULL);

			g_free(base_dir);
			g_free(thumb_path);
			}
		else
			{
			thumb = image;
			}

	/* end save cache */

		gdk_imlib_render(thumb, width, height);
		*thumb_pixmap = gdk_imlib_move_image(thumb);
		*thumb_mask = gdk_imlib_move_mask(thumb);
		if (*thumb_pixmap)
			space = thumb_max_width - width;
		gdk_imlib_destroy_image(thumb);
		thumb = NULL;
		}
	else
		{
		space = -1;
		}
	return space;
}

gint maintain_thumbnail_dir(gchar *dir, gint recursive)
{
	gchar *thumb_dir;
	gint base_length;
	gint still_have_a_file = FALSE;

	if (debug) printf("maintainance check: %s\n", dir);

	base_length = strlen(homedir()) + strlen(THUMBNAIL_CACHE_DIR);
	thumb_dir = g_strconcat(homedir(), THUMBNAIL_CACHE_DIR, dir, NULL);

	if (isdir(thumb_dir))
		{
		DIR             *dp;
		struct dirent   *dirent;
		struct stat ent_sbuf;

		if((dp = opendir(thumb_dir))==NULL)
			{
				/* dir not found */
				g_free(thumb_dir);
				return FALSE;
			}

		while ((dirent = readdir(dp)) != NULL)
			{
			/* skips removed files */
	                if (dirent->d_ino > 0)
	                	{
				int l = 0;
				gchar *path_buf;
				if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
					continue;
				path_buf = g_strconcat(thumb_dir, "/", dirent->d_name, NULL);
				if (strlen(path_buf) > 4) l = strlen(path_buf) - 4;

				if (stat(path_buf,&ent_sbuf) >= 0 && S_ISDIR(ent_sbuf.st_mode))
					{
					/* recurse dir then delete it */
					gchar *rdir = g_strconcat(dir, "/", dirent->d_name, NULL);
					if (recursive && !maintain_thumbnail_dir(rdir, TRUE))
						{
						if (debug) printf("Deleting thumb dir: %s\n",path_buf);
						if ( (rmdir (path_buf) < 0) )
							printf(_("Unable to delete dir: %s\n"), path_buf);
						}
					else
						still_have_a_file = TRUE;
					g_free(rdir);
					}
				else
					{
					gchar *fp = path_buf + l;
					fp[0] = '\0';
					if (strlen(path_buf) > base_length &&
							!isfile(path_buf + base_length))
						{
						fp[0] = '.';
						if (debug) printf("Deleting thumb: %s\n",path_buf);
						if ( (unlink (path_buf) < 0) )
							printf(_("failed to delete:%s\n"),path_buf);
						}
					else
						 still_have_a_file = TRUE;
					}
				g_free(path_buf);
				}
			}
		closedir(dp);
		}
	g_free(thumb_dir);
	return still_have_a_file;
}

/*
 *-----------------------------------------------------------------------------
 * xvpics thumbnail support, read-only (private)
 *-----------------------------------------------------------------------------
 */

/*
 * xvpics code originally supplied by:
 * "Diederen Damien" <D.Diederen@student.ulg.ac.be>
 *
 * Note: Code has been modified to fit the style of the other code, and to use
 *       a few more glib-isms.
 */

#define XV_BUFFER 2048
static guchar *load_xv_thumbnail(gchar *filename, gint *widthp, gint *heightp)
{
	FILE *file;
	gchar buffer[XV_BUFFER];
	guchar *data;
	gint width, height, depth;

	file = fopen(filename, "rt");
	if(!file) return NULL;

	fgets(buffer, XV_BUFFER, file);
	if(strncmp(buffer, "P7 332", 6) != 0)
		{
		fclose(file);
		return NULL;
		}

	while(fgets(buffer, XV_BUFFER, file) && buffer[0] == '#') /* do_nothing() */;

	if(sscanf(buffer, "%d %d %d", &width, &height, &depth) != 3)
		{
		fclose(file);
		return NULL;
		}

	data = g_new(guchar, width * height);
	fread(data, 1, width * height, file);

	fclose(file);
	*widthp = width;
	*heightp = height;
	return data;
}
#undef XV_BUFFER

static void normalize_thumb(gint *width, gint *height)
{
	if(*width > thumb_max_width || *height > thumb_max_height)
		{
		gfloat factor = MAX((gfloat) *width / thumb_max_width, (gfloat) *height / thumb_max_height);
		*width = (gfloat) *width / factor;
		*height = (gfloat) *height / factor;
		}
}

static gint get_xv_thumbnail(gchar *thumb_filename, GdkPixmap **thumb_pixmap, GdkBitmap **thumb_mask)
{
	gint width, height;
	gchar *thumb_name;
	gchar *tmp_string;
	gchar *last_slash;
	guchar *packed_data;

	tmp_string = g_strdup(thumb_filename);  
	last_slash = strrchr(tmp_string, '/');
	if(!last_slash)	return -1;
	*last_slash++ = '\0';

	thumb_name = g_strconcat(tmp_string, "/.xvpics/", last_slash, NULL);
	packed_data = load_xv_thumbnail(thumb_name, &width, &height);
	g_free(tmp_string);
	g_free(thumb_name);

	if(packed_data)
		{
		guchar *rgb_data;
		GdkImlibImage *image;
		gint i;

		rgb_data = g_new(guchar, width * height * 3);
		for(i = 0; i < width * height; i++)
			{
			rgb_data[i * 3 + 0] = (packed_data[i] >> 5) * 36;
			rgb_data[i * 3 + 1] = ((packed_data[i] & 28) >> 2) * 36;
			rgb_data[i * 3 + 2] = (packed_data[i] & 3) * 85;
			}

		g_free(packed_data);
		image = gdk_imlib_create_image_from_data(rgb_data, NULL, width, height);
		g_free(rgb_data);
		normalize_thumb(&width, &height);
		gdk_imlib_render(image, width, height);
	
		if(*thumb_pixmap) gdk_imlib_free_pixmap(*thumb_pixmap);

		*thumb_pixmap = gdk_imlib_move_image(image);
		*thumb_mask = gdk_imlib_move_mask(image);
		gdk_imlib_destroy_image(image);
		return thumb_max_width - width;
		}

	return -1;
}

