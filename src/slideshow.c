/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"

static GList *slide_list = NULL;
static GList *past_slide_list = NULL;
static gchar *slide_img = NULL;
static gchar *slide_path = NULL;
static gint slide_count = 0;
static gint slide_active = FALSE;
static gint slide_sel_list = FALSE;
static gint slide_timeout_id = -1;

static void slideshow_free_all()
{
	slide_active = FALSE;

	g_list_free(slide_list);
	slide_list = NULL;

	g_list_free(past_slide_list);
	past_slide_list = NULL;

	g_free(slide_path);
	slide_path = NULL;

	g_free(slide_img);
	slide_img = NULL;

	slide_count = 0;
	slide_sel_list = FALSE;
}

static GList *generate_list()
{
	GList *list = NULL;

	if (file_selection_count() < 2)
		{
		gint i;
		gint c = file_count();
		for(i = 0; i < c; i++)
			{
			list = g_list_prepend(list, GINT_TO_POINTER(i));
			}
		slide_sel_list = FALSE;
		}
	else
		{
		GList *work = GTK_CLIST(file_clist)->selection;
		while(work)
			{
			list = g_list_prepend(list, work->data);
			work = work->next;
			}
		slide_sel_list = TRUE;
		}
	list = g_list_reverse(list);

	return list;
}

static GList *generate_random_list()
{
	GList *src_list = NULL;
	GList *list = NULL;
	GList *work;

	src_list = generate_list();

	while(src_list)
		{
		gint p = (float)rand() / RAND_MAX * g_list_length(src_list);
		work = g_list_nth(src_list, p);
		list = g_list_prepend(list, work->data);
		src_list = g_list_remove(src_list, work->data);
		}

	return list;
}

static void slideshow_init_list()
{
	if (slide_list)
		{
		g_list_free(slide_list);
		}

	if (past_slide_list)
		{
		g_list_free(past_slide_list);
		past_slide_list = NULL;
		}

	if (slideshow_random)
		{
		slide_list = generate_random_list();
		}
	else
		{
		slide_list = generate_list();
		}
}

static void slideshow_move_list(gint forward)
{
	if (forward)
		{
		if (slide_list)
			{
			past_slide_list = g_list_prepend (past_slide_list, slide_list->data);
			slide_list = g_list_remove(slide_list, slide_list->data);
			}
		}
	else
		{
		if (past_slide_list)
			{
			slide_list = g_list_prepend(slide_list, past_slide_list->data);
			past_slide_list = g_list_remove(past_slide_list, past_slide_list->data);
			}
		}
}

static gint slideshow_should_continue()
{
	if (!slide_active || !slide_list || !slide_path ||
	    slide_count != file_count() ||
	    (slide_img && image_get_path() && strcmp(image_get_path(), slide_img) != 0) ||
	    current_path == NULL ||
	    strcmp(current_path, slide_path) != 0)
		{
		return FALSE;
		}

	return TRUE;
}

static gint real_slideshow_prev()
{
	gint row;
	gchar *buf;

	if (!slide_active) return FALSE;
	if (!past_slide_list || !past_slide_list->next) return TRUE;

	if (!slideshow_should_continue())
		{
		slideshow_free_all();
		slide_timeout_id = -1;
		return FALSE;
		}

	slideshow_move_list(FALSE);

	row = GPOINTER_TO_INT(past_slide_list->data);

	g_free(slide_img);
	slide_img = NULL;
	buf = file_get_path(row);

	if (slide_sel_list)
		{
		image_change_to(buf);
		update_status_label(NULL);
		}
	else
		{
		file_image_change_to(row);
		}

	slide_img = buf;

	return TRUE;
}

/* the return is TRUE if slideshow should continue */
static gint real_slideshow_next()
{
	gint row;
	gchar *buf;

	if (!slide_active) return FALSE;

	if (!slideshow_should_continue())
		{
		slideshow_free_all();
		slide_timeout_id = -1;
		return FALSE;
		}

	row = GPOINTER_TO_INT(slide_list->data);

	g_free(slide_img);
	slide_img = NULL;
	buf = file_get_path(row);
	slideshow_move_list(TRUE);

	if (!slide_list && slideshow_repeat)
		{
		slideshow_init_list();
		}

	if (slide_sel_list)
		{
		image_change_to(buf);
		update_status_label(NULL);
		}
	else
		{
		file_image_change_to(row);
		}

	slide_img = buf;

	if (!slide_list)
		{
		slideshow_free_all();
		slide_timeout_id = -1;
		return FALSE;
		}

	return TRUE;
}

static gint slideshow_loop_cb(gpointer data)
{
	return real_slideshow_next();
}

void slideshow_start()
{
	gint row;
	gchar *buf;

	if (slide_active) return;

	if (file_count() < 2) return;

	slideshow_init_list();
	if (!slide_list) return;

	row = GPOINTER_TO_INT(slide_list->data);

	slide_active = TRUE;
	slide_path = g_strdup(current_path);
	slide_count = file_count();
	g_free(slide_img);
	slide_img = NULL;
	buf = file_get_path(row);
	slideshow_move_list(TRUE);

	if (slide_sel_list)
		{
		image_change_to(buf);
		update_status_label(NULL);
		}
	else
		{
		file_image_change_to(row);
		}

	slide_img = buf;

	slide_timeout_id = gtk_timeout_add(slideshow_delay * 1000, slideshow_loop_cb, NULL);
}

void slideshow_stop()
{
	if (!slide_active) return;

	slideshow_free_all();
	if (slide_timeout_id != -1)
		{
		gtk_timeout_remove(slide_timeout_id);
		slide_timeout_id = -1;
		}
	update_status_label(NULL);
}

static void slideshow_reset_timeout(gint reset)
{
	if (reset)
		{
		if (slide_timeout_id != -1) gtk_timeout_remove(slide_timeout_id);
		slide_timeout_id = gtk_timeout_add(slideshow_delay * 1000, slideshow_loop_cb, NULL);
		}
	else
		{
		if (slide_timeout_id != -1)
			{
			gtk_timeout_remove(slide_timeout_id);
			slide_timeout_id = -1;
			}
		}
}

void slideshow_next()
{
	if (!slide_active) return;
	slideshow_reset_timeout(real_slideshow_next());
}

void slideshow_prev()
{
	if (!slide_active) return;
	slideshow_reset_timeout(real_slideshow_prev());
}

void slideshow_toggle()
{
	if (!slide_active)
		{
		slideshow_start();
		}
	else
		{
		slideshow_stop();
		}
}

gint slideshow_is_running()
{
	if (!slide_active) return FALSE;

	if (!slideshow_should_continue())
		{
		slideshow_free_all();
		if (slide_timeout_id != -1)
			{
			gtk_timeout_remove(slide_timeout_id);
			slide_timeout_id = -1;
			}
		return FALSE;
		}

	return TRUE;
}

