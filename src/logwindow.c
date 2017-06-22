/*
 * Copyright (C) 2008 - 2016 The Geeqie Team
 *
 * Authors: Vladimir Nadvornik, Laurent Monin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "main.h"
#include "logwindow.h"

#include "misc.h"
#include "secure_save.h"
#include "ui_misc.h"
#include "window.h"

#include <gdk/gdkkeysyms.h>


typedef struct _LogWindow LogWindow;

struct _LogWindow
{
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	GdkColor colors[LOG_COUNT];

	guint lines;
	GtkWidget *regexp_box;
	GtkWidget *bar;
	GtkWidget *pause;
	GtkWidget *wrap;
	GtkWidget *debug_level;
};

typedef struct _LogDef LogDef;
struct _LogDef
{
	LogType type;
	const gchar *tag;
	const gchar *color;
};

/* Keep LogType order !! */
static LogDef logdefs[LOG_COUNT] = {
	{ LOG_NORMAL,	"normal", 	"black"	 },
	{ LOG_MSG,	"message",	"blue"	 },
	{ LOG_WARN,	"warning",	"orange" },
	{ LOG_ERROR,	"error",	"red"	 },
};

static LogWindow *logwindow = NULL;

static void hide_cb(GtkWidget *widget, LogWindow *logwin)
{
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    LogWindow *logwin)
{
	if (event && event->keyval == GDK_KEY_Escape)
		gtk_widget_hide(logwin->window);
	return FALSE;
}


static void log_window_pause_cb(GtkWidget *widget, gpointer data)
{
	options->log_window.paused = !options->log_window.paused;
}

static void log_window_line_wrap_cb(GtkWidget *widget, gpointer data)
{
	LogWindow *logwin = data;

	options->log_window.line_wrap = !options->log_window.line_wrap;

	if (options->log_window.line_wrap)
		{
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(logwin->text), GTK_WRAP_WORD);
		}
	else
		{
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(logwin->text), GTK_WRAP_NONE);
		}
}

static void log_window_regexp_cb(GtkWidget *text_entry, gpointer data)
{
	gchar *new_regexp;

	new_regexp = g_strdup(gtk_entry_get_text(GTK_ENTRY(text_entry)));
	set_regexp(new_regexp);
	g_free(new_regexp);
}

static void log_window_debug_spin_cb(GtkSpinButton *debug_level, gpointer data)
{
	set_debug_level(gtk_spin_button_get_value(debug_level));
}

static LogWindow *log_window_create(LayoutWindow *lw)
{
	LogWindow *logwin;
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkWidget *button;
	GtkWidget *win_vbox;
	GtkWidget *textbox;
	GtkWidget *hbox;

	logwin = g_new0(LogWindow, 1);

	window = window_new(GTK_WINDOW_TOPLEVEL, "log", NULL, NULL, _("Log"));
	win_vbox = gtk_vbox_new(FALSE, PREF_PAD_SPACE);
	gtk_container_add(GTK_CONTAINER(window), win_vbox);
	gtk_widget_show(win_vbox);

	gtk_widget_set_size_request(window, lw->options.log_window.w, lw->options.log_window.h);
	gtk_window_move(GTK_WINDOW(window), lw->options.log_window.x, lw->options.log_window.y);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), logwin);
	g_signal_connect(G_OBJECT(window), "hide",
			 G_CALLBACK(hide_cb), logwin);
	gtk_widget_realize(window);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	gtk_container_add(GTK_CONTAINER(win_vbox), scrolledwin);
	gtk_widget_show(scrolledwin);

	hbox = pref_box_new(win_vbox, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);

	gtk_widget_show(hbox);
	logwin->debug_level = pref_spin_new_mnemonic(hbox, _("Debug level:"), NULL,
			  0, 4, 1, 1, get_debug_level(),G_CALLBACK(log_window_debug_spin_cb),
			  logwin->debug_level );

	logwin->pause = pref_button_new(hbox, NULL, "Pause", FALSE,
					   G_CALLBACK(log_window_pause_cb), NULL);

	logwin->wrap = pref_button_new(hbox, NULL, "Line wrap", FALSE,
					   G_CALLBACK(log_window_line_wrap_cb), logwin);

	pref_label_new(hbox, "Filter regexp");

	textbox = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(hbox), textbox);
	gtk_widget_show(textbox);
	g_signal_connect(G_OBJECT(textbox), "activate",
			 G_CALLBACK(log_window_regexp_cb), logwin);

	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_create_mark(buffer, "end", &iter, FALSE);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);
	gtk_widget_show(text);

	logwin->window = window;
	logwin->scrolledwin = scrolledwin;
	logwin->text = text;
	logwin->lines = 1;
	logwin->regexp_box = textbox;
	lw->log_window = logwin->window;
	return logwin;
}

static void log_window_init(LogWindow *logwin)
{
	GtkTextBuffer *buffer;
#if !GTK_CHECK_VERSION(3,0,0)
	GdkColormap *colormap;
	gboolean success[LOG_COUNT];
#endif
	gint i;

	g_assert(logwin != NULL);
	g_assert(logwin->colors != NULL);
#if !GTK_CHECK_VERSION(3,0,0)
	for (i = LOG_NORMAL; i < LOG_COUNT; i++)
		{
		gboolean ok = gdk_color_parse(logdefs[i].color, &logwin->colors[i]);
		if (ok == TRUE) continue;

		if (i == LOG_NORMAL) return;
		memcpy(&logwin->colors[i], &logwin->colors[LOG_NORMAL], sizeof(GdkColor));
		}

	colormap = gdk_drawable_get_colormap(gtk_widget_get_window(logwin->window));
	gdk_colormap_alloc_colors(colormap, logwin->colors, LOG_COUNT, FALSE, TRUE, success);

	for (i = LOG_NORMAL; i < LOG_COUNT; i++)
		{
		if (success[i] == FALSE)
			{
			GtkStyle *style;
			gint j;

			g_warning("LogWindow: color allocation failed\n");
			style = gtk_widget_get_style(logwin->window);
			for (j = LOG_NORMAL; j < LOG_COUNT; j++)
				logwin->colors[j] = style->black;
			break;
			}
		}
#endif
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(logwin->text));
	for (i = LOG_NORMAL; i < LOG_COUNT; i++)
		gtk_text_buffer_create_tag(buffer, logdefs[i].tag,
				   	   "foreground-gdk", &logwin->colors[i],
					   "family", "MonoSpace",
				   	   NULL);
}

static void log_window_show(LogWindow *logwin)
{
	GtkTextView *text = GTK_TEXT_VIEW(logwin->text);
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	gchar *regexp;

	g_assert(logwin != NULL);

	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_mark(buffer, "end");
	gtk_text_view_scroll_mark_onscreen(text, mark);

	gtk_window_present(GTK_WINDOW(logwin->window));

	log_window_append("", LOG_NORMAL); // to flush memorized lines

	regexp = g_strdup(get_regexp());
	if (regexp != NULL)
		{
		gtk_entry_set_text(GTK_ENTRY(logwin->regexp_box), regexp);
		g_free(regexp);
		}
}

void log_window_new(LayoutWindow *lw)
{
	if (logwindow == NULL)
		{
		LogWindow *logwin;

		logwin = log_window_create(lw);
		log_window_init(logwin);
		logwindow = logwin;
		}

	log_window_show(logwindow);
}

typedef struct _LogMsg LogMsg;

struct _LogMsg {
	gchar *text;
	LogType type;
};


static void log_window_insert_text(GtkTextBuffer *buffer, GtkTextIter *iter,
				   const gchar *text, const gchar *tag)
{
	gchar *str_utf8;

	if (!text || !*text) return;

	str_utf8 = utf8_validate_or_convert(text);
	gtk_text_buffer_insert_with_tags_by_name(buffer, iter, str_utf8, -1, tag, NULL);
	g_free(str_utf8);
}


void log_window_append(const gchar *str, LogType type)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	static GList *memory = NULL;

	if (logwindow == NULL)
		{
		if (*str) {
			LogMsg *msg = g_new(LogMsg, 1);

			msg->text = g_strdup(str);
			msg->type = type;

			memory = g_list_prepend(memory, msg);

			while (g_list_length(memory) >= options->log_window_lines)
				{
				GList *work = g_list_last(memory);
				LogMsg *oldest_msg = work->data;

				g_free(oldest_msg->text);
				memory = g_list_delete_link(memory, work);
				}
			}
		return;
		}

	text = GTK_TEXT_VIEW(logwindow->text);
	buffer = gtk_text_view_get_buffer(text);

	if (options->log_window_lines > 0 && logwindow->lines >= options->log_window_lines)
		{
		GtkTextIter start, end;

		gtk_text_buffer_get_start_iter(buffer, &start);
		end = start;
		gtk_text_iter_forward_lines(&end, logwindow->lines - options->log_window_lines);
		gtk_text_buffer_delete(buffer, &start, &end);
		}

	gtk_text_buffer_get_end_iter(buffer, &iter);

	{
	GList *work = g_list_last(memory);

	while (work)
		{
		GList *prev;
		LogMsg *oldest_msg = work->data;

		log_window_insert_text(buffer, &iter, oldest_msg->text, logdefs[oldest_msg->type].tag);

		prev = work->prev;
		memory = g_list_delete_link(memory, work);
		work = prev;
		}
	}

	log_window_insert_text(buffer, &iter, str, logdefs[type].tag);

	if (!options->log_window.paused)
		{
		if (gtk_widget_get_visible(GTK_WIDGET(text)))
			{
			GtkTextMark *mark;

			mark = gtk_text_buffer_get_mark(buffer, "end");
			gtk_text_view_scroll_mark_onscreen(text, mark);
			}
		}

	logwindow->lines = gtk_text_buffer_get_line_count(buffer);
}
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
