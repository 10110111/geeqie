/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

ImageWindow *image_area_new(GtkWidget *top_window);
void image_area_free(ImageWindow *imd);

/* for attaching the top window for resizing */
void image_area_set_topwindow(ImageWindow *imd, GtkWidget *window, gchar *title, gint show_zoom);

/* attach labels to be updated */
void image_area_set_labels(ImageWindow *imd, GtkWidget *info, GtkWidget *zoom);

/* set the current image to a different path */
void image_area_set_path(ImageWindow *imd, gchar *newpath);

/* attach handler functions for mouse buttons (1-3) */
void image_area_set_button(ImageWindow *imd, gint button,
	void (*func)(ImageWindow *, GdkEventButton *, gpointer), gpointer data);

/* get the current image's path, etc. */
gchar *image_area_get_path(ImageWindow *imd);
gchar *image_area_get_name(ImageWindow *imd);

/* load a new image, or NULL sets to logo */
void image_area_set_image(ImageWindow *imd, gchar *path, gint zoom);

/* image manipulation */
void image_area_scroll(ImageWindow *imd, gint x, gint y);
gint image_area_get_zoom(ImageWindow *imd);
void image_area_adjust_zoom(ImageWindow *imd, gint increment);
void image_area_set_zoom(ImageWindow *imd, gint zoom);

/* get the default zoom for an image */
gint get_default_zoom(ImageWindow *imd);

/* set the root window to the current image */
void image_area_to_root(ImageWindow *imd, gint scaled);


