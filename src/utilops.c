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
#include "utilops.h"


#include "cache.h"
#include "cache_maint.h"
#include "collect.h"
#include "dupe.h"
#include "filedata.h"
#include "filefilter.h"
#include "image.h"
#include "img-view.h"
#include "layout.h"
#include "search.h"
#include "thumb_standard.h"
#include "trash.h"
#include "ui_bookmark.h"
#include "ui_fileops.h"
#include "ui_misc.h"
#include "ui_tabcomp.h"
#include "editors.h"

/*
 *--------------------------------------------------------------------------
 * Adds 1 or 2 images (if 2, side by side) to a GenericDialog
 *--------------------------------------------------------------------------
 */

#define DIALOG_DEF_IMAGE_DIM_X 200
#define DIALOG_DEF_IMAGE_DIM_Y 150

static void generic_dialog_add_image(GenericDialog *gd, GtkWidget *box,
				     FileData *fd1, const gchar *header1,
				     FileData *fd2, const gchar *header2,
				     gint show_filename)
{
	ImageWindow *imd;
	GtkWidget *hbox = NULL;
	GtkWidget *vbox;
	GtkWidget *label = NULL;

	if (!box) box = gd->vbox;

	if (fd2)
		{
		hbox = pref_box_new(box, TRUE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);
		}

	/* image 1 */

	vbox = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	if (hbox)
		{
		GtkWidget *sep;

		gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

		sep = gtk_vseparator_new();
		gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);
		gtk_widget_show(sep);
		}
	else
		{
		gtk_box_pack_start(GTK_BOX(box), vbox, TRUE, TRUE, PREF_PAD_GAP);
		}
	gtk_widget_show(vbox);

	if (header1)
		{
		GtkWidget *head;

		head = pref_label_new(vbox, header1);
		pref_label_bold(head, TRUE, FALSE);
		gtk_misc_set_alignment(GTK_MISC(head), 0.0, 0.5);
		}

	imd = image_new(FALSE);
	g_object_set(G_OBJECT(imd->pr), "zoom_expand", FALSE, NULL);
	gtk_widget_set_size_request(imd->widget, DIALOG_DEF_IMAGE_DIM_X, DIALOG_DEF_IMAGE_DIM_Y);
	gtk_box_pack_start(GTK_BOX(vbox), imd->widget, TRUE, TRUE, 0);
	image_change_fd(imd, fd1, 0.0);
	gtk_widget_show(imd->widget);

	if (show_filename)
		{
		label = pref_label_new(vbox, (fd1 == NULL) ? "" : fd1->name);
		}

	/* only the first image is stored (for use in gd_image_set) */
	g_object_set_data(G_OBJECT(gd->dialog), "img_image", imd);
	g_object_set_data(G_OBJECT(gd->dialog), "img_label", label);


	/* image 2 */

	if (hbox && fd2)
		{
		vbox = pref_box_new(hbox, TRUE, GTK_ORIENTATION_VERTICAL, PREF_PAD_GAP);

		if (header2)
			{
			GtkWidget *head;

			head = pref_label_new(vbox, header2);
			pref_label_bold(head, TRUE, FALSE);
			gtk_misc_set_alignment(GTK_MISC(head), 0.0, 0.5);
			}

		imd = image_new(FALSE);
		g_object_set(G_OBJECT(imd->pr), "zoom_expand", FALSE, NULL);
		gtk_widget_set_size_request(imd->widget, DIALOG_DEF_IMAGE_DIM_X, DIALOG_DEF_IMAGE_DIM_Y);
		gtk_box_pack_start(GTK_BOX(vbox), imd->widget, TRUE, TRUE, 0);
		image_change_fd(imd, fd2, 0.0);
		gtk_widget_show(imd->widget);

		pref_label_new(vbox, fd2->name);
		}
}

static void generic_dialog_image_set(GenericDialog *gd, FileData *fd)
{
	ImageWindow *imd;
	GtkWidget *label;

	imd = g_object_get_data(G_OBJECT(gd->dialog), "img_image");
	label = g_object_get_data(G_OBJECT(gd->dialog), "img_label");

	if (!imd) return;

	image_change_fd(imd, fd, 0.0);
	if (label) gtk_label_set_text(GTK_LABEL(label), fd->name);
}

/*
 *--------------------------------------------------------------------------
 * Wrappers to aid in setting additional dialog properties (unde mouse, etc.)
 *--------------------------------------------------------------------------
 */

GenericDialog *file_util_gen_dlg(const gchar *title,
				 const gchar *wmclass, const gchar *wmsubclass,
				 GtkWidget *parent, gint auto_close,
				 void (*cancel_cb)(GenericDialog *, gpointer), gpointer data)
{
	GenericDialog *gd;

	gd = generic_dialog_new(title, wmclass, wmsubclass, parent, auto_close, cancel_cb, data);
	if (options->place_dialogs_under_mouse)
		{
		gtk_window_set_position(GTK_WINDOW(gd->dialog), GTK_WIN_POS_MOUSE);
		}

	return gd;
}

FileDialog *file_util_file_dlg(const gchar *title,
			       const gchar *wmclass, const gchar *wmsubclass,
			       GtkWidget *parent,
			       void (*cancel_cb)(FileDialog *, gpointer), gpointer data)
{
	FileDialog *fdlg;

	fdlg = file_dialog_new(title, wmclass, wmsubclass, parent, cancel_cb, data);
	if (options->place_dialogs_under_mouse)
		{
		gtk_window_set_position(GTK_WINDOW(GENERIC_DIALOG(fdlg)->dialog), GTK_WIN_POS_MOUSE);
		}

	return fdlg;
}

/* this warning dialog is copied from SLIK's ui_utildg.c,
 * because it does not have a mouse center option,
 * and we must center it before show, implement it here.
 */
static void file_util_warning_dialog_ok_cb(GenericDialog *gd, gpointer data)
{
	/* no op */
}

GenericDialog *file_util_warning_dialog(const gchar *heading, const gchar *message,
					const gchar *icon_stock_id, GtkWidget *parent)
{
	GenericDialog *gd;

	gd = file_util_gen_dlg(heading, GQ_WMCLASS, "warning", parent, TRUE, NULL, NULL);
	generic_dialog_add_message(gd, icon_stock_id, heading, message);
	generic_dialog_add_button(gd, GTK_STOCK_OK, NULL, file_util_warning_dialog_ok_cb, TRUE);
	if (options->place_dialogs_under_mouse)
		{
		gtk_window_set_position(GTK_WINDOW(gd->dialog), GTK_WIN_POS_MOUSE);
		}
	gtk_widget_show(gd->dialog);

	return gd;
}

static gint filename_base_length(const gchar *name)
{
	gint n;

	if (!name) return 0;

	n = strlen(name);

	if (filter_name_exists(name))
		{
		const gchar *ext;

		ext = extension_from_path(name);
		if (ext) n -= strlen(ext);
		}

	return n;
}



/*
 *--------------------------------------------------------------------------
 * call these when names change, files move, deleted, etc.
 * so that any open windows are also updated
 *--------------------------------------------------------------------------
 */

/* FIXME this is a temporary solution */
void file_data_notify_ci(FileData *fd)
{
	FileDataChangeType type = fd->change->type;
	switch (type)
		{
		case FILEDATA_CHANGE_MOVE:
			cache_maint_moved(fd);
			collection_maint_renamed(fd);

			layout_maint_moved(fd, NULL);
			view_window_maint_moved(fd);
			dupe_maint_renamed(fd);
			search_maint_renamed(fd);
			break;
		case FILEDATA_CHANGE_COPY:
			cache_maint_copied(fd);
			break;
		case FILEDATA_CHANGE_RENAME:
			cache_maint_moved(fd);
			collection_maint_renamed(fd);

			layout_maint_renamed(fd);
			view_window_maint_moved(fd);
			dupe_maint_renamed(fd);
			search_maint_renamed(fd);
			break;
		case FILEDATA_CHANGE_DELETE:
			layout_maint_removed(fd, NULL);
			view_window_maint_removed(fd, NULL);
			dupe_maint_removed(fd);
			search_maint_removed(fd);

			collection_maint_removed(fd);
			cache_maint_removed(fd);
			break;
		case FILEDATA_CHANGE_UNSPECIFIED:
			/* FIXME */
			break;
		}
}

void file_data_sc_notify_ci(FileData *fd)
{
	GList *work;
	if (fd->parent) fd = fd->parent;
	
	file_data_notify_ci(fd);
	
	work = fd->sidecar_files;
	while (work)
		{
		FileData *sfd = work->data;
		file_data_notify_ci(sfd);
		work = work->next;
		}
}


typedef enum {
	UTILITY_TYPE_COPY,
	UTILITY_TYPE_MOVE,
	UTILITY_TYPE_RENAME,
	UTILITY_TYPE_EDITOR,
	UTILITY_TYPE_FILTER,
	UTILITY_TYPE_DELETE,
	UTILITY_TYPE_DELETE_LINK,
	UTILITY_TYPE_DELETE_FOLDER
} UtilityType;

typedef enum {
	UTILITY_PHASE_START = 0,
	UTILITY_PHASE_ENTERING,
	UTILITY_PHASE_CHECKED,
	UTILITY_PHASE_DONE,
	UTILITY_PHASE_CANCEL
} UtilityPhase;

enum {
	UTILITY_RENAME = 0,
	UTILITY_RENAME_AUTO,
	UTILITY_RENAME_FORMATTED
};

typedef struct _UtilityDataMessages UtilityDataMessages;
struct _UtilityDataMessages {
	gchar *title;
	gchar *question;
	gchar *desc_flist;
	gchar *desc_dlist;
	gchar *desc_source_fd;
	gchar *fail;
};

typedef struct _UtilityData UtilityData;

struct _UtilityData {
	UtilityType type;
	UtilityPhase phase;
	
	FileData *source_fd;
	GList *dlist;
	GList *flist;

	GtkWidget *parent;
	GenericDialog *gd;
	FileDialog *fdlg;
	
	gint update_idle_id;
	
	/* alternative dialog parts */
	GtkWidget *notebook;

	UtilityDataMessages messages;

	/* helper entries for various modes */
	GtkWidget *rename_entry;
	GtkWidget *rename_label;
	GtkWidget *auto_entry_front;
	GtkWidget *auto_entry_end;
	GtkWidget *auto_spin_start;
	GtkWidget *auto_spin_pad;
	GtkWidget *format_entry;
	GtkWidget *format_spin;

	GtkWidget *listview;


	gchar *dest_path;

	/* data for the operation itself, internal or external */
	gboolean external; /* TRUE for external command, FALSE for internal */
	
	gint external_command;
	gpointer resume_data;
};

enum {
	UTILITY_COLUMN_FD = 0,
	UTILITY_COLUMN_PATH,
	UTILITY_COLUMN_NAME,
	UTILITY_COLUMN_SIDECARS,
	UTILITY_COLUMN_DEST_PATH,
	UTILITY_COLUMN_DEST_NAME,
	UTILITY_COLUMN_COUNT
};

#define UTILITY_LIST_MIN_WIDTH  250
#define UTILITY_LIST_MIN_HEIGHT 150

/* thumbnail spec has a max depth of 4 (.thumb??/fail/appname/??.png) */
#define UTILITY_DELETE_MAX_DEPTH 5

static UtilityData *file_util_data_new(UtilityType type)
{
	UtilityData *ud;

	ud = g_new0(UtilityData, 1);
	ud->type = type;
	ud->phase = UTILITY_PHASE_START;
	ud->update_idle_id = -1;
	ud->external_command = -1;
	return ud;
}

static void file_util_data_free(UtilityData *ud)
{
	if (!ud) return;

	if (ud->update_idle_id != -1) g_source_remove(ud->update_idle_id);

	file_data_unref(ud->source_fd);
	filelist_free(ud->dlist);
	filelist_free(ud->flist);

	if (ud->gd) generic_dialog_close(ud->gd);
	
	g_free(ud->dest_path);

	g_free(ud);
}

static GtkTreeViewColumn *file_util_dialog_add_list_column(GtkWidget *view, const gchar *text, gint n)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, text);
	gtk_tree_view_column_set_min_width(column, 4);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", n);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	return column;
}

static void file_util_dialog_list_select(GtkWidget *view, gint n)
{
	GtkTreeModel *store;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	if (gtk_tree_model_iter_nth_child(store, &iter, NULL, n))
		{
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
		gtk_tree_selection_select_iter(selection, &iter);
		}
}

static GtkWidget *file_util_dialog_add_list(GtkWidget *box, GList *list, gint full_paths)
{
	GtkWidget *scrolled;
	GtkWidget *view;
	GtkListStore *store;

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(scrolled);

	store = gtk_list_store_new(UTILITY_COLUMN_COUNT, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), FALSE);

	if (full_paths)
		{
		file_util_dialog_add_list_column(view, _("Location"), UTILITY_COLUMN_PATH);
		}
	else
		{
		file_util_dialog_add_list_column(view, _("Name"), UTILITY_COLUMN_NAME);
		}

	gtk_widget_set_size_request(view, UTILITY_LIST_MIN_WIDTH, UTILITY_LIST_MIN_HEIGHT);
	gtk_container_add(GTK_CONTAINER(scrolled), view);
	gtk_widget_show(view);

	while (list)
		{
		FileData *fd = list->data;
		GtkTreeIter iter;
		gchar *sidecars;
		
		sidecars = file_data_sc_list_to_string(fd);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   UTILITY_COLUMN_FD, fd,
				   UTILITY_COLUMN_PATH, fd->path,
				   UTILITY_COLUMN_NAME, fd->name,
				   UTILITY_COLUMN_SIDECARS, sidecars,
				   UTILITY_COLUMN_DEST_PATH, fd->change ? fd->change->dest : "error",
				   UTILITY_COLUMN_DEST_NAME, fd->change ? filename_from_path(fd->change->dest) : "error",
				   -1);
		g_free(sidecars);

		list = list->next;
		}

	return view;
}

// FIXME
static void file_util_delete_dir_ok_cb(GenericDialog *gd, gpointer data);

void file_util_perform_ci_internal(UtilityData *ud);
void file_util_dialog_run(UtilityData *ud);
static gint file_util_perform_ci_cb(gpointer resume_data, gint flags, GList *list, gpointer data);

/* call file_util_perform_ci_internal or start_editor_from_filelist_full */


static void file_util_resume_cb(GenericDialog *gd, gpointer data)
{
	UtilityData *ud = data;
	if (ud->external)
		editor_resume(ud->resume_data);
	else
		file_util_perform_ci_internal(ud);
}

static void file_util_abort_cb(GenericDialog *gd, gpointer data)
{
	UtilityData *ud = data;
	if (ud->external)
		editor_skip(ud->resume_data);
	else
		file_util_perform_ci_cb(NULL, EDITOR_ERROR_SKIPPED, ud->flist, ud);

}


static gint file_util_perform_ci_cb(gpointer resume_data, gint flags, GList *list, gpointer data)
{
	UtilityData *ud = data;
	ud->resume_data = resume_data;
	
	gint ret = EDITOR_CB_CONTINUE;
	if ((flags & EDITOR_ERROR_MASK) && !(flags & EDITOR_ERROR_SKIPPED))
		{
		GString *msg = g_string_new(editor_get_error_str(flags));
		GenericDialog *d;
		g_string_append(msg, "\n");
		g_string_append(msg, ud->messages.fail);
		g_string_append(msg, "\n");
		while (list)
			{
			FileData *fd = list->data;

			g_string_append(msg, fd->path);
			g_string_append(msg, "\n");
			list = list->next;
			}
		if (resume_data)
			{
			g_string_append(msg, _("\n Continue multiple file operation?"));
			d = file_util_gen_dlg(ud->messages.fail, GQ_WMCLASS, "dlg_confirm",
					      NULL, TRUE,
					      file_util_abort_cb, ud);

			generic_dialog_add_message(d, GTK_STOCK_DIALOG_WARNING, NULL, msg->str);

			generic_dialog_add_button(d, GTK_STOCK_GO_FORWARD, _("Co_ntinue"),
						  file_util_resume_cb, TRUE);
			gtk_widget_show(d->dialog);
			ret = EDITOR_CB_SUSPEND;
			}
		else
			{
			file_util_warning_dialog(ud->messages.fail, msg->str, GTK_STOCK_DIALOG_ERROR, NULL);
			}
		g_string_free(msg, TRUE);
		}


	while (list)  /* be careful, file_util_perform_ci_internal can pass ud->flist as list */
		{
		FileData *fd = list->data;
		list = list->next; 

		if (!(flags & EDITOR_ERROR_MASK)) /* files were successfully deleted, call the maint functions */
			{
			file_data_sc_apply_ci(fd);
			file_data_sc_notify_ci(fd);
			}
		
		ud->flist = g_list_remove(ud->flist, fd);
		file_data_sc_free_ci(fd);
		file_data_unref(fd);
		}
		
	if (!resume_data) /* end of the list */
		{
		ud->phase = UTILITY_PHASE_DONE;
		file_util_dialog_run(ud);
		}
	
	return ret;
}


/*
 * Perform the operation described by FileDataChangeInfo on all files in the list
 * it is an alternative to start_editor_from_filelist_full, it should use similar interface
 */ 


void file_util_perform_ci_internal(UtilityData *ud)
{
	
	while (ud->flist)
		{
		gint ret;
		
		/* take a single entry each time, this allows better control over the operation */
		GList *single_entry = g_list_append(NULL, ud->flist->data);
		gboolean last = !ud->flist->next;
		gint status = EDITOR_ERROR_STATUS;
	
		if (file_data_sc_perform_ci(single_entry->data))
			status = 0; /* OK */
		
		ret = file_util_perform_ci_cb(GINT_TO_POINTER(!last), status, single_entry, ud);
		g_list_free(single_entry);
		
		if (ret == EDITOR_CB_SUSPEND || last) return;
		
		if (ret == EDITOR_CB_SKIP)
			{
			file_util_perform_ci_cb(NULL, EDITOR_ERROR_SKIPPED, ud->flist, ud);
			}
		
		/* FIXME: convert the loop to idle call */
		
		}
}


void file_util_perform_ci(UtilityData *ud)
{
	switch (ud->type)
		{
		case UTILITY_TYPE_COPY:
			ud->external_command = CMD_COPY;
			break;
		case UTILITY_TYPE_MOVE:
			ud->external_command = CMD_MOVE;
			break;
		case UTILITY_TYPE_RENAME:
			ud->external_command = CMD_RENAME;
			break;
		case UTILITY_TYPE_DELETE:
		case UTILITY_TYPE_DELETE_LINK:
		case UTILITY_TYPE_DELETE_FOLDER:
			ud->external_command = CMD_DELETE;
			break;
		case UTILITY_TYPE_FILTER:
		case UTILITY_TYPE_EDITOR:
			g_assert(ud->external_command != -1); /* it should be already set */
			break;
		}

	if (is_valid_editor_command(ud->external_command))
		{
		gint flags;
		
		ud->external = TRUE;
		flags = start_editor_from_filelist_full(ud->external_command, ud->flist, file_util_perform_ci_cb, ud);

		if (flags)
			{
			gchar *text = g_strdup_printf(_("%s\nUnable to start external command.\n"), editor_get_error_str(flags));
			file_util_warning_dialog(ud->messages.fail, text, GTK_STOCK_DIALOG_ERROR, NULL);
			g_free(text);
			}
		}
	else
		{
		ud->external = FALSE;
		file_util_perform_ci_internal(ud);
		}
}

static void file_util_cancel_cb(GenericDialog *gd, gpointer data)
{
	UtilityData *ud = data;
	
	generic_dialog_close(gd);

	ud->gd = NULL;
	
	ud->phase = UTILITY_PHASE_CANCEL;
	file_util_dialog_run(ud);
}

static void file_util_ok_cb(GenericDialog *gd, gpointer data)
{
	UtilityData *ud = data;
	
	generic_dialog_close(gd);
	
	ud->gd = NULL;

	file_util_dialog_run(ud);
}

static void file_util_fdlg_cancel_cb(FileDialog *fdlg, gpointer data)
{
	UtilityData *ud = data;
	
	file_dialog_close(fdlg);

	ud->fdlg = NULL;
	
	ud->phase = UTILITY_PHASE_CANCEL;
	file_util_dialog_run(ud);
}

static void file_util_fdlg_ok_cb(FileDialog *fdlg, gpointer data)
{
	UtilityData *ud = data;
	
	file_dialog_close(fdlg);
	
	ud->fdlg = NULL;

	file_util_dialog_run(ud);
}


static void file_util_dest_folder_entry_cb(GtkWidget *entry, gpointer data)
{
	UtilityData *ud = data;
	
	g_free(ud->dest_path);
	ud->dest_path = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	
	switch (ud->type)
		{
		case UTILITY_TYPE_COPY:
			file_data_sc_update_ci_copy_list(ud->flist, ud->dest_path);
			break;
		case UTILITY_TYPE_MOVE:
			file_data_sc_update_ci_move_list(ud->flist, ud->dest_path);
			break;
		case UTILITY_TYPE_FILTER:
		case UTILITY_TYPE_EDITOR:
			file_data_sc_update_ci_unspecified_list(ud->flist, ud->dest_path);
			break;
		case UTILITY_TYPE_DELETE:
		case UTILITY_TYPE_DELETE_LINK:
		case UTILITY_TYPE_DELETE_FOLDER:
		case UTILITY_TYPE_RENAME:
			g_warning("unhandled operation");
		}
}


/* format: * = filename without extension, ## = number position, extension is kept */
static gchar *file_util_rename_multiple_auto_format_name(const gchar *format, const gchar *name, gint n)
{
	gchar *new_name;
	gchar *parsed;
	const gchar *ext;
	gchar *middle;
	gchar *tmp;
	gchar *pad_start;
	gchar *pad_end;
	gint padding;

	if (!format || !name) return NULL;

	tmp = g_strdup(format);
	pad_start = strchr(tmp, '#');
	if (pad_start)
		{
		pad_end = pad_start;
		padding = 0;
		while (*pad_end == '#')
			{
			pad_end++;
			padding++;
			}
		*pad_start = '\0';

		parsed = g_strdup_printf("%s%0*d%s", tmp, padding, n, pad_end);
		g_free(tmp);
		}
	else
		{
		parsed = tmp;
		}

	ext = extension_from_path(name);

	middle = strchr(parsed, '*');
	if (middle)
		{
		gchar *base;

		*middle = '\0';
		middle++;

		base = remove_extension_from_path(name);
		new_name = g_strconcat(parsed, base, middle, ext, NULL);
		g_free(base);
		}
	else
		{
		new_name = g_strconcat(parsed, ext, NULL);
		}

	g_free(parsed);

	return new_name;
}


static void file_util_rename_preview_update(UtilityData *ud)
{
	GtkTreeModel *store;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	const gchar *front;
	const gchar *end;
	const gchar *format;
	gint valid;
	gint start_n;
	gint padding;
	gint n;
	gint mode;

	mode = gtk_notebook_get_current_page(GTK_NOTEBOOK(ud->notebook));

	if (mode == UTILITY_RENAME)
		{
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ud->listview));
		if (gtk_tree_selection_get_selected(selection, &store, &iter))
			{
			FileData *fd;
			const gchar *dest = gtk_entry_get_text(GTK_ENTRY(ud->rename_entry));
			
			gtk_tree_model_get(store, &iter, UTILITY_COLUMN_FD, &fd, -1);
			file_data_sc_update_ci_rename(fd, dest);
			gtk_list_store_set(GTK_LIST_STORE(store), &iter, 
			                   UTILITY_COLUMN_DEST_PATH, fd->change->dest,
				           UTILITY_COLUMN_DEST_NAME, filename_from_path(fd->change->dest),
					   -1);
			}
		return;
		}


	front = gtk_entry_get_text(GTK_ENTRY(ud->auto_entry_front));
	end = gtk_entry_get_text(GTK_ENTRY(ud->auto_entry_end));
	padding = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ud->auto_spin_pad));

	format = gtk_entry_get_text(GTK_ENTRY(ud->format_entry));

	if (mode == UTILITY_RENAME_FORMATTED)
		{
		start_n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ud->format_spin));
		}
	else
		{
		start_n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ud->auto_spin_start));
		}

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(ud->listview));
	n = start_n;
	valid = gtk_tree_model_get_iter_first(store, &iter);
	while (valid)
		{
		gchar *dest;
		FileData *fd;
		gtk_tree_model_get(store, &iter, UTILITY_COLUMN_FD, &fd, -1);

		if (mode == UTILITY_RENAME_FORMATTED)
			{
			dest = file_util_rename_multiple_auto_format_name(format, fd->name, n);
			}
		else
			{
			dest = g_strdup_printf("%s%0*d%s", front, padding, n, end);
			}

		file_data_sc_update_ci_rename(fd, dest);
		gtk_list_store_set(GTK_LIST_STORE(store), &iter, 
		                   UTILITY_COLUMN_DEST_PATH, fd->change->dest,
				   UTILITY_COLUMN_DEST_NAME, filename_from_path(fd->change->dest),
				   -1);
		g_free(dest);

		n++;
		valid = gtk_tree_model_iter_next(store, &iter);
		}

}

static void file_util_rename_preview_entry_cb(GtkWidget *entry, gpointer data)
{
	UtilityData *ud = data;
	file_util_rename_preview_update(ud);
}

static void file_util_rename_preview_adj_cb(GtkWidget *spin, gpointer data)
{
	UtilityData *ud = data;
	file_util_rename_preview_update(ud);
}

static gboolean file_util_rename_idle_cb(gpointer data)
{
	UtilityData *ud = data;

	file_util_rename_preview_update(ud);

	ud->update_idle_id = -1;
	return FALSE;
}

static void file_util_rename_preview_order_cb(GtkTreeModel *treemodel, GtkTreePath *tpath,
						       GtkTreeIter *iter, gpointer data)
{
	UtilityData *ud = data;

	if (ud->update_idle_id != -1) return;

	ud->update_idle_id = g_idle_add(file_util_rename_idle_cb, ud);
}


static gboolean file_util_preview_cb(GtkTreeSelection *selection, GtkTreeModel *store,
						GtkTreePath *tpath, gboolean path_currently_selected,
						gpointer data)
{
	UtilityData *ud = data;
	GtkTreeIter iter;
	FileData *fd = NULL;

	if (path_currently_selected ||
	    !gtk_tree_model_get_iter(store, &iter, tpath)) return TRUE;

	gtk_tree_model_get(store, &iter, UTILITY_COLUMN_FD, &fd, -1);
	generic_dialog_image_set(ud->gd, fd);
	
	if (ud->type == UTILITY_TYPE_RENAME)
		{
		const gchar *name = filename_from_path(fd->change->dest);

		gtk_widget_grab_focus(ud->rename_entry);
		gtk_label_set_text(GTK_LABEL(ud->rename_label), fd->name);
		g_signal_handlers_block_by_func(ud->rename_entry, G_CALLBACK(file_util_rename_preview_entry_cb), ud);
		gtk_entry_set_text(GTK_ENTRY(ud->rename_entry), name);
		gtk_editable_select_region(GTK_EDITABLE(ud->rename_entry), 0, filename_base_length(name));
		g_signal_handlers_unblock_by_func(ud->rename_entry, G_CALLBACK(file_util_rename_preview_entry_cb), ud);
		}

	return TRUE;
}



static void box_append_safe_delete_status(GenericDialog *gd)
{
	GtkWidget *label;
	gchar *buf;

	buf = file_util_safe_delete_status();
	label = pref_label_new(gd->vbox, buf);
	g_free(buf);

	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_widget_set_sensitive(label, FALSE);
}


static void file_util_dialog_init_simple_list(UtilityData *ud)
{
	GtkWidget *box;
	GtkTreeSelection *selection;

	ud->gd = file_util_gen_dlg(ud->messages.title, GQ_WMCLASS, "dlg_confirm",
				   ud->parent, FALSE,  file_util_cancel_cb, ud);
	generic_dialog_add_button(ud->gd, GTK_STOCK_DELETE, NULL, file_util_ok_cb, TRUE);

	box = generic_dialog_add_message(ud->gd, GTK_STOCK_DIALOG_QUESTION,
					 ud->messages.question,
					 ud->messages.desc_flist);

	box = pref_group_new(box, TRUE, ud->messages.desc_flist, GTK_ORIENTATION_HORIZONTAL);

	ud->listview = file_util_dialog_add_list(box, ud->flist, FALSE);
	file_util_dialog_add_list_column(ud->listview, _("Sidecars"), UTILITY_COLUMN_SIDECARS);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ud->listview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	gtk_tree_selection_set_select_function(selection, file_util_preview_cb, ud, NULL);

	generic_dialog_add_image(ud->gd, box, NULL, NULL, NULL, NULL, FALSE);

	box_append_safe_delete_status(ud->gd);

	gtk_widget_show(ud->gd->dialog);

	file_util_dialog_list_select(ud->listview, 0);
}

static void file_util_dialog_init_dest_folder(UtilityData *ud)
{
	FileDialog *fdlg;
	GtkWidget *label;
	const gchar *stock_id;

	if (ud->type == UTILITY_TYPE_COPY) 
		{
		stock_id = GTK_STOCK_COPY;
		}
	else
		{
		stock_id = GTK_STOCK_OK;
		}

	fdlg = file_util_file_dlg(ud->messages.title, GQ_WMCLASS, "dlg_dest_folder", ud->parent,
				  file_util_fdlg_cancel_cb, ud);
	
	ud->fdlg = fdlg;
	
	generic_dialog_add_message(GENERIC_DIALOG(fdlg), NULL, ud->messages.question, NULL);

	label = pref_label_new(GENERIC_DIALOG(fdlg)->vbox, _("Choose the destination folder."));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	pref_spacer(GENERIC_DIALOG(fdlg)->vbox, 0);

	file_dialog_add_button(fdlg, stock_id, ud->messages.title, file_util_fdlg_ok_cb, TRUE);

	file_dialog_add_path_widgets(fdlg, NULL, ud->dest_path, "move_copy", NULL, NULL);

	g_signal_connect(G_OBJECT(fdlg->entry), "changed",
			 G_CALLBACK(file_util_dest_folder_entry_cb), ud);

	gtk_widget_show(GENERIC_DIALOG(fdlg)->dialog);
}


static GtkWidget *furm_simple_vlabel(GtkWidget *box, const gchar *text, gint expand)
{
	GtkWidget *vbox;
	GtkWidget *label;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), vbox, expand, expand, 0);
	gtk_widget_show(vbox);

	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	return vbox;
}


static void file_util_dialog_init_source_dest(UtilityData *ud)
{
	GtkTreeModel *store;
	GtkTreeSelection *selection;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *box2;
	GtkWidget *table;
	GtkWidget *combo;
	GtkWidget *page;

	ud->gd = file_util_gen_dlg(ud->messages.title, GQ_WMCLASS, "dlg_confirm",
				   ud->parent, FALSE,  file_util_cancel_cb, ud);

	box = generic_dialog_add_message(ud->gd, NULL, ud->messages.question, NULL);
	generic_dialog_add_button(ud->gd, GTK_STOCK_OK, ud->messages.title, file_util_ok_cb, TRUE);

	box = pref_group_new(box, TRUE, ud->messages.desc_flist, GTK_ORIENTATION_HORIZONTAL);

	ud->listview = file_util_dialog_add_list(box, ud->flist, FALSE);
	file_util_dialog_add_list_column(ud->listview, _("Sidecars"), UTILITY_COLUMN_SIDECARS);

	file_util_dialog_add_list_column(ud->listview, _("New name"), UTILITY_COLUMN_DEST_NAME);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ud->listview));
	gtk_tree_selection_set_mode(GTK_TREE_SELECTION(selection), GTK_SELECTION_SINGLE);
	gtk_tree_selection_set_select_function(selection, file_util_preview_cb, ud, NULL);


//	column = file_util_rename_multiple_add_column(rd, _("Preview"), RENAME_COLUMN_PREVIEW);
//	gtk_tree_view_column_set_visible(column, FALSE);

	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(ud->listview), TRUE);
	
	store = gtk_tree_view_get_model(GTK_TREE_VIEW(ud->listview));
	g_signal_connect(G_OBJECT(store), "row_changed",
			 G_CALLBACK(file_util_rename_preview_order_cb), ud);
	gtk_widget_set_size_request(ud->listview, 300, 150);

	generic_dialog_add_image(ud->gd, box, NULL, NULL, NULL, NULL, FALSE);

//	gtk_container_add(GTK_CONTAINER(scrolled), view);
	gtk_widget_show(ud->gd->dialog);


	ud->notebook = gtk_notebook_new();
	
	gtk_box_pack_start(GTK_BOX(ud->gd->vbox), ud->notebook, FALSE, FALSE, 0);
	gtk_widget_show(ud->notebook);
	

	page = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	gtk_notebook_append_page(GTK_NOTEBOOK(ud->notebook), page, gtk_label_new(_("Manual rename")));
	gtk_widget_show(page);
	
	table = pref_table_new(page, 2, 2, FALSE, FALSE);

	pref_table_label(table, 0, 0, _("Original name:"), 1.0);
	ud->rename_label = pref_table_label(table, 1, 0, "", 0.0);

	pref_table_label(table, 0, 1, _("New name:"), 1.0);

	ud->rename_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), ud->rename_entry, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	generic_dialog_attach_default(GENERIC_DIALOG(ud->gd), ud->rename_entry);
	gtk_widget_grab_focus(ud->rename_entry);

	g_signal_connect(G_OBJECT(ud->rename_entry), "changed",
			 G_CALLBACK(file_util_rename_preview_entry_cb), ud);

	gtk_widget_show(ud->rename_entry);

	page = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	gtk_notebook_append_page(GTK_NOTEBOOK(ud->notebook), page, gtk_label_new(_("Auto rename")));
	gtk_widget_show(page);


	hbox = pref_box_new(page, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_GAP);

	box2 = furm_simple_vlabel(hbox, _("Begin text"), TRUE);

	combo = history_combo_new(&ud->auto_entry_front, "", "numerical_rename_prefix", -1);
	g_signal_connect(G_OBJECT(ud->auto_entry_front), "changed",
			 G_CALLBACK(file_util_rename_preview_entry_cb), ud);
	gtk_box_pack_start(GTK_BOX(box2), combo, TRUE, TRUE, 0);
	gtk_widget_show(combo);

	box2 = furm_simple_vlabel(hbox, _("Start #"), FALSE);

	ud->auto_spin_start = pref_spin_new(box2, NULL, NULL,
					    0.0, 1000000.0, 1.0, 0, 1.0,
					    G_CALLBACK(file_util_rename_preview_adj_cb), ud);

	box2 = furm_simple_vlabel(hbox, _("End text"), TRUE);

	combo = history_combo_new(&ud->auto_entry_end, "", "numerical_rename_suffix", -1);
	g_signal_connect(G_OBJECT(ud->auto_entry_end), "changed",
			 G_CALLBACK(file_util_rename_preview_entry_cb), ud);
	gtk_box_pack_start(GTK_BOX(box2), combo, TRUE, TRUE, 0);
	gtk_widget_show(combo);

	ud->auto_spin_pad = pref_spin_new(page, _("Padding:"), NULL,
					  1.0, 8.0, 1.0, 0, 1.0,
					  G_CALLBACK(file_util_rename_preview_adj_cb), ud);

	page = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	gtk_notebook_append_page(GTK_NOTEBOOK(ud->notebook), page, gtk_label_new(_("Formatted rename")));
	gtk_widget_show(page);

	hbox = pref_box_new(page, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_GAP);

	box2 = furm_simple_vlabel(hbox, _("Format (* = original name, ## = numbers)"), TRUE);

	combo = history_combo_new(&ud->format_entry, "", "auto_rename_format", -1);
	g_signal_connect(G_OBJECT(ud->format_entry), "changed",
			 G_CALLBACK(file_util_rename_preview_entry_cb), ud);
	gtk_box_pack_start(GTK_BOX(box2), combo, TRUE, TRUE, 0);
	gtk_widget_show(combo);

	box2 = furm_simple_vlabel(hbox, _("Start #"), FALSE);

	ud->format_spin = pref_spin_new(box2, NULL, NULL,
					0.0, 1000000.0, 1.0, 0, 1.0,
					G_CALLBACK(file_util_rename_preview_adj_cb), ud);

//	gtk_combo_box_set_active(GTK_COMBO_BOX(ud->combo_type), 0); /* callback will take care of the rest */

	file_util_dialog_list_select(ud->listview, 0);
}


void file_util_dialog_run(UtilityData *ud)
{
	switch (ud->phase)
		{
		case UTILITY_PHASE_START:
			/* create the dialogs */
			switch (ud->type)
				{
				case UTILITY_TYPE_DELETE:
				case UTILITY_TYPE_DELETE_LINK:
				case UTILITY_TYPE_DELETE_FOLDER:
				case UTILITY_TYPE_EDITOR:
					file_util_dialog_init_simple_list(ud);
					break;
				case UTILITY_TYPE_RENAME:
					file_util_dialog_init_source_dest(ud);
					break;
				case UTILITY_TYPE_COPY:
				case UTILITY_TYPE_MOVE:
				case UTILITY_TYPE_FILTER:
					file_util_dialog_init_dest_folder(ud);
					break;
				}
			ud->phase = UTILITY_PHASE_ENTERING;
			break;
		case UTILITY_PHASE_ENTERING:
			/* FIXME use file_data_sc_check_ci_dest to detect problems and eventually go back to PHASE_START 
			or to PHASE_CANCEL */
		
			ud->phase = UTILITY_PHASE_CHECKED;
		case UTILITY_PHASE_CHECKED:
			file_util_perform_ci(ud);
			break;
		case UTILITY_PHASE_CANCEL:
		case UTILITY_PHASE_DONE:
			file_data_sc_free_ci_list(ud->flist);
			file_data_sc_free_ci_list(ud->dlist);
			if (ud->source_fd) file_data_sc_free_ci(ud->source_fd);
			file_util_data_free(ud);
			break;
		}
}




static gint file_util_unlink(FileData *fd)
{
}

static void file_util_warn_op_in_progress(const gchar *title)
{
	file_util_warning_dialog(title, _("Another operation in progress.\n"), GTK_STOCK_DIALOG_ERROR, NULL);
}

static void file_util_delete_full(FileData *source_fd, GList *source_list, GtkWidget *parent, UtilityPhase phase)
{
	UtilityData *ud;
	GList *flist = filelist_copy(source_list);
	
	if (source_fd)
		flist = g_list_append(flist, file_data_ref(source_fd));

	if (!file_data_sc_add_ci_delete_list(flist))
		{
		file_util_warn_op_in_progress(_("File deletion failed"));
		filelist_free(flist);
		return;
		}

	ud = file_util_data_new(UTILITY_TYPE_DELETE);
	
	ud->phase = phase;

	ud->source_fd = NULL;
	ud->flist = flist;
	ud->dlist = NULL;
	ud->parent = parent;
	
	ud->messages.title = _("Delete");
	ud->messages.question = _("Delete files?");
	ud->messages.desc_flist = _("This will delete the following files");
	ud->messages.desc_dlist = "";
	ud->messages.desc_source_fd = "";
	ud->messages.fail = _("File deletion failed");

	file_util_dialog_run(ud);
}

static void file_util_move_full(FileData *source_fd, GList *source_list, const gchar *dest_path, GtkWidget *parent, UtilityPhase phase)
{
	UtilityData *ud;
	GList *flist = filelist_copy(source_list);
	
	if (source_fd)
		flist = g_list_append(flist, file_data_ref(source_fd));

	if (!file_data_sc_add_ci_move_list(flist, dest_path))
		{
		file_util_warn_op_in_progress(_("Move failed"));
		filelist_free(flist);
		return;
		}

	ud = file_util_data_new(UTILITY_TYPE_MOVE);

	ud->phase = phase;

	ud->source_fd = NULL;
	ud->flist = flist;
	ud->dlist = NULL;
	ud->parent = parent;

	ud->dest_path = g_strdup(dest_path ? dest_path : "");
	
	ud->messages.title = _("Move");
	ud->messages.question = _("Move files?");
	ud->messages.desc_flist = _("This will move the following files");
	ud->messages.desc_dlist = "";
	ud->messages.desc_source_fd = "";
	ud->messages.fail = _("Move failed");

	file_util_dialog_run(ud);
}
static void file_util_copy_full(FileData *source_fd, GList *source_list, const gchar *dest_path, GtkWidget *parent, UtilityPhase phase)
{
	UtilityData *ud;
	GList *flist = filelist_copy(source_list);
	
	if (source_fd)
		flist = g_list_append(flist, file_data_ref(source_fd));

	if (!file_data_sc_add_ci_copy_list(flist, dest_path))
		{
		file_util_warn_op_in_progress(_("Copy failed"));
		filelist_free(flist);
		return;
		}

	ud = file_util_data_new(UTILITY_TYPE_COPY);

	ud->phase = phase;

	ud->source_fd = NULL;
	ud->flist = flist;
	ud->dlist = NULL;
	ud->parent = parent;

	ud->dest_path = g_strdup(dest_path ? dest_path : "");
	
	ud->messages.title = _("Copy");
	ud->messages.question = _("Copy files?");
	ud->messages.desc_flist = _("This will copy the following files");
	ud->messages.desc_dlist = "";
	ud->messages.desc_source_fd = "";
	ud->messages.fail = _("Copy failed");

	file_util_dialog_run(ud);
}

static void file_util_rename_full(FileData *source_fd, GList *source_list, const gchar *dest_path, GtkWidget *parent, UtilityPhase phase)
{
	UtilityData *ud;
	GList *flist = filelist_copy(source_list);
	
	if (source_fd)
		flist = g_list_append(flist, file_data_ref(source_fd));

	if (!file_data_sc_add_ci_rename_list(flist, dest_path))
		{
		file_util_warn_op_in_progress(_("Rename failed"));
		filelist_free(flist);
		return;
		}

	ud = file_util_data_new(UTILITY_TYPE_RENAME);

	ud->phase = phase;

	ud->source_fd = NULL;
	ud->flist = flist;
	ud->dlist = NULL;
	ud->parent = parent;
	
	ud->messages.title = _("Rename");
	ud->messages.question = _("Rename files?");
	ud->messages.desc_flist = _("This will rename the following files");
	ud->messages.desc_dlist = "";
	ud->messages.desc_source_fd = "";
	ud->messages.fail = _("Rename failed");

	file_util_dialog_run(ud);
}

static void file_util_start_editor_full(gint n, FileData *source_fd, GList *source_list, const gchar *dest_path, GtkWidget *parent, UtilityPhase phase)
{
	UtilityData *ud;
	GList *flist = filelist_copy(source_list);
	
	if (source_fd)
		flist = g_list_append(flist, file_data_ref(source_fd));

	if (!file_data_sc_add_ci_unspecified_list(flist, dest_path))
		{
		file_util_warn_op_in_progress(_("Can't run external editor"));
		filelist_free(flist);
		return;
		}

	if (editor_is_filter(n))
		ud = file_util_data_new(UTILITY_TYPE_FILTER);
	else
		ud = file_util_data_new(UTILITY_TYPE_EDITOR);
		
		
	/* ask for destination if we don't have it */
	if (ud->type == UTILITY_TYPE_FILTER && dest_path == NULL) phase = UTILITY_PHASE_START;
	
	ud->phase = phase;
	
	ud->external_command = n;

	ud->source_fd = NULL;
	ud->flist = flist;
	ud->dlist = NULL;
	ud->parent = parent;

	ud->dest_path = g_strdup(dest_path ? dest_path : "");
	
	ud->messages.title = _("Editor");
	ud->messages.question = _("Run editor?");
	ud->messages.desc_flist = _("This will copy the following files");
	ud->messages.desc_dlist = "";
	ud->messages.desc_source_fd = "";
	ud->messages.fail = _("External command failed");

	file_util_dialog_run(ud);
}


/* FIXME: */
void file_util_create_dir(const gchar *path, GtkWidget *parent)
{
}

gint file_util_rename_dir(FileData *source_fd, const gchar *new_path, GtkWidget *parent)
{
}

/* full-featured entry points
*/

void file_util_delete(FileData *source_fd, GList *source_list, GtkWidget *parent)
{
	file_util_delete_full(source_fd, source_list, parent, UTILITY_PHASE_START);
}

void file_util_copy(FileData *source_fd, GList *source_list, const gchar *dest_path, GtkWidget *parent)
{
	file_util_copy_full(source_fd, source_list, dest_path, parent, UTILITY_PHASE_START);
}

void file_util_move(FileData *source_fd, GList *source_list, const gchar *dest_path, GtkWidget *parent)
{
	file_util_move_full(source_fd, source_list, dest_path, parent, UTILITY_PHASE_START);
}

void file_util_rename(FileData *source_fd, GList *source_list, GtkWidget *parent)
{
	file_util_rename_full(source_fd, source_list, NULL, parent, UTILITY_PHASE_START);
}

/* these avoid the location entry dialog unless there is an error, list must be files only and
 * dest_path must be a valid directory path
 */
void file_util_move_simple(GList *list, const gchar *dest_path, GtkWidget *parent)
{
	file_util_move_full(NULL, list, dest_path, parent, UTILITY_PHASE_ENTERING);
}

void file_util_copy_simple(GList *list, const gchar *dest_path, GtkWidget *parent)
{
	file_util_copy_full(NULL, list, dest_path, parent, UTILITY_PHASE_ENTERING);
}

void file_util_rename_simple(FileData *fd, const gchar *dest_path, GtkWidget *parent)
{
	file_util_rename_full(fd, NULL, dest_path, parent, UTILITY_PHASE_ENTERING);
}


void file_util_start_editor_from_file(gint n, FileData *fd, GtkWidget *parent)
{
	file_util_start_editor_full(n, fd, NULL, NULL, parent, UTILITY_PHASE_ENTERING);
}

void file_util_start_editor_from_filelist(gint n, GList *list, GtkWidget *parent)
{
	file_util_start_editor_full(n, NULL, list, NULL, parent, UTILITY_PHASE_ENTERING);
}

void file_util_start_filter_from_file(gint n, FileData *fd, const gchar *dest_path, GtkWidget *parent)
{
	file_util_start_editor_full(n, fd, NULL, dest_path, parent, UTILITY_PHASE_ENTERING);
}

void file_util_start_filter_from_filelist(gint n, GList *list, const gchar *dest_path, GtkWidget *parent)
{
	file_util_start_editor_full(n, NULL, list, dest_path, parent, UTILITY_PHASE_ENTERING);
}


/*
 *--------------------------------------------------------------------------
 * Delete directory routines
 *--------------------------------------------------------------------------
 */

// FIXME



FileData *file_util_delete_dir_empty_path(FileData *fd, gint real_content, gint level)
{
	GList *dlist = NULL;
	GList *flist = NULL;
	GList *work;
	FileData *fail = NULL;

	DEBUG_1("deltree into: %s", fd->path);

	level++;
	if (level > UTILITY_DELETE_MAX_DEPTH)
		{
		log_printf("folder recursion depth past %d, giving up\n", UTILITY_DELETE_MAX_DEPTH);
		return file_data_ref(fd);
		}

	if (!filelist_read_lstat(fd->path, &flist, &dlist)) file_data_ref(fd);

	work = dlist;
	while (work && !fail)
		{
		FileData *lfd;

		lfd = work->data;
		work = work->next;

		fail = file_util_delete_dir_empty_path(lfd, real_content, level);
		}

	work = flist;
	while (work && !fail)
		{
		FileData *lfd;

		lfd = work->data;
		work = work->next;

		DEBUG_1("deltree child: %s", lfd->path);

		if (real_content && !islink(lfd->path))
			{
			if (!file_util_unlink(lfd)) fail = file_data_ref(lfd);
			}
		else
			{
			if (!unlink_file(lfd->path)) fail = file_data_ref(lfd);
			}
		}

	filelist_free(dlist);
	filelist_free(flist);

	if (!fail && !rmdir_utf8(fd->path))
		{
		fail = file_data_ref(fd);
		}

	DEBUG_1("deltree done: %s", fd->path);

	return fail;
}

static void file_util_delete_dir_ok_cb(GenericDialog *gd, gpointer data)
{
	UtilityData *ud = data;

	ud->gd = NULL;

	if (ud->type == UTILITY_TYPE_DELETE_LINK)
		{
		if (!unlink_file(ud->source_fd->path))
			{
			gchar *text;

			text = g_strdup_printf("Unable to remove symbolic link:\n %s", ud->source_fd->path);
			file_util_warning_dialog(_("Delete failed"), text,
						 GTK_STOCK_DIALOG_ERROR, NULL);
			g_free(text);
			}
		}
	else
		{
		FileData *fail = NULL;
		GList *work;

		work = ud->dlist;
		while (work && !fail)
			{
			FileData *fd;

			fd = work->data;
			work = work->next;

			fail = file_util_delete_dir_empty_path(fd, FALSE, 0);
			}

		work = ud->flist;
		while (work && !fail)
			{
			FileData *fd;

			fd = work->data;
			work = work->next;

			DEBUG_1("deltree unlink: %s", fd->path);

			if (islink(fd->path))
				{
				if (!unlink_file(fd->path)) fail = file_data_ref(fd);
				}
			else
				{
				if (!file_util_unlink(fd)) fail = file_data_ref(fd);
				}
			}

		if (!fail)
			{
			if (!rmdir_utf8(ud->source_fd->path)) fail = file_data_ref(ud->source_fd);
			}

		if (fail)
			{
			gchar *text;

			text = g_strdup_printf(_("Unable to delete folder:\n\n%s"), ud->source_fd->path);
			gd = file_util_warning_dialog(_("Delete failed"), text, GTK_STOCK_DIALOG_ERROR, NULL);
			g_free(text);

			if (fail != ud->source_fd)
				{
				pref_spacer(gd->vbox, PREF_PAD_GROUP);
				text = g_strdup_printf(_("Removal of folder contents failed at this file:\n\n%s"),
							fail->path);
				pref_label_new(gd->vbox, text);
				g_free(text);
				}

			file_data_unref(fail);
			}
		}

	file_util_data_free(ud);
}

static GList *file_util_delete_dir_remaining_folders(GList *dlist)
{
	GList *rlist = NULL;

	while (dlist)
		{
		FileData *fd;

		fd = dlist->data;
		dlist = dlist->next;

		if (!fd->name ||
		    (strcmp(fd->name, THUMB_FOLDER_GLOBAL) != 0 &&
		     strcmp(fd->name, THUMB_FOLDER_LOCAL) != 0 &&
		     strcmp(fd->name, GQ_CACHE_LOCAL_METADATA) != 0) )
			{
			rlist = g_list_prepend(rlist, fd);
			}
		}

	return g_list_reverse(rlist);
}

void file_util_delete_dir(FileData *fd, GtkWidget *parent)
{
	GList *dlist = NULL;
	GList *flist = NULL;
	GList *rlist;

	if (!isdir(fd->path)) return;

	if (islink(fd->path))
		{
		UtilityData *ud;
		gchar *text;

		ud = g_new0(UtilityData, 1);
		ud->type = UTILITY_TYPE_DELETE_LINK;
		ud->source_fd = file_data_ref(fd);
		ud->dlist = NULL;
		ud->flist = NULL;

		ud->gd = file_util_gen_dlg(_("Delete folder"), GQ_WMCLASS, "dlg_confirm",
					   parent, TRUE,
					   file_util_cancel_cb, ud);

		text = g_strdup_printf(_("This will delete the symbolic link:\n\n%s\n\n"
					 "The folder this link points to will not be deleted."),
				       fd->path);
		generic_dialog_add_message(ud->gd, GTK_STOCK_DIALOG_QUESTION,
					   _("Delete symbolic link to folder?"),
					   text);
		g_free(text);

		generic_dialog_add_button(ud->gd, GTK_STOCK_DELETE, NULL, file_util_delete_dir_ok_cb, TRUE);

		gtk_widget_show(ud->gd->dialog);

		return;
		}

	if (!access_file(fd->path, W_OK | X_OK))
		{
		gchar *text;

		text = g_strdup_printf(_("Unable to remove folder %s\n"
					 "Permissions do not allow writing to the folder."), fd->path);
		file_util_warning_dialog(_("Delete failed"), text, GTK_STOCK_DIALOG_ERROR, parent);
		g_free(text);

		return;
		}

	if (!filelist_read_lstat(fd->path, &flist, &dlist))
		{
		gchar *text;

		text = g_strdup_printf(_("Unable to list contents of folder %s"), fd->path);
		file_util_warning_dialog(_("Delete failed"), text, GTK_STOCK_DIALOG_ERROR, parent);
		g_free(text);

		return;
		}

	rlist = file_util_delete_dir_remaining_folders(dlist);
	if (rlist)
		{
		GenericDialog *gd;
		GtkWidget *box;
		gchar *text;

		gd = file_util_gen_dlg(_("Folder contains subfolders"), GQ_WMCLASS, "dlg_warning",
					parent, TRUE, NULL, NULL);
		generic_dialog_add_button(gd, GTK_STOCK_CLOSE, NULL, NULL, TRUE);

		text = g_strdup_printf(_("Unable to delete the folder:\n\n%s\n\n"
					 "This folder contains subfolders which must be moved before it can be deleted."),
					fd->path);
		box = generic_dialog_add_message(gd, GTK_STOCK_DIALOG_WARNING,
						 _("Folder contains subfolders"),
						 text);
		g_free(text);

		box = pref_group_new(box, TRUE, _("Subfolders:"), GTK_ORIENTATION_VERTICAL);

		rlist = filelist_sort_path(rlist);
		file_util_dialog_add_list(box, rlist, FALSE);

		gtk_widget_show(gd->dialog);
		}
	else
		{
		UtilityData *ud;
		GtkWidget *box;
		GtkWidget *view;
		GtkTreeSelection *selection;
		gchar *text;

		ud = g_new0(UtilityData, 1);
		ud->type = UTILITY_TYPE_DELETE_FOLDER;
		ud->source_fd = file_data_ref(fd);
		ud->dlist = dlist;
		dlist = NULL;
		ud->flist = filelist_sort_path(flist);
		flist = NULL;

		ud->gd = file_util_gen_dlg(_("Delete folder"), GQ_WMCLASS, "dlg_confirm",
					   parent, TRUE,  file_util_cancel_cb, ud);
		generic_dialog_add_button(ud->gd, GTK_STOCK_DELETE, NULL, file_util_delete_dir_ok_cb, TRUE);

		text = g_strdup_printf(_("This will delete the folder:\n\n%s\n\n"
					 "The contents of this folder will also be deleted."),
					fd->path);
		box = generic_dialog_add_message(ud->gd, GTK_STOCK_DIALOG_QUESTION,
						 _("Delete folder?"),
						 text);
		g_free(text);

		box = pref_group_new(box, TRUE, _("Contents:"), GTK_ORIENTATION_HORIZONTAL);

		view = file_util_dialog_add_list(box, ud->flist, FALSE);
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
		gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
		gtk_tree_selection_set_select_function(selection, file_util_preview_cb, ud, NULL);

		generic_dialog_add_image(ud->gd, box, NULL, NULL, NULL, NULL, FALSE);

		box_append_safe_delete_status(ud->gd);

		gtk_widget_show(ud->gd->dialog);
		}

	g_list_free(rlist);
	filelist_free(dlist);
	filelist_free(flist);
}


void file_util_copy_path_to_clipboard(FileData *fd)
{
	GtkClipboard *clipboard;

	if (!fd || !*fd->path) return;

	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, g_shell_quote(fd->path), -1);
}

void file_util_copy_path_list_to_clipboard(GList *list)
{
	GtkClipboard *clipboard;
	GList *work;
	GString *new;

	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	
	new = g_string_new("");
	work = list;
	while (work) {
		FileData *fd = work->data;
		work = work->next;

		if (!fd || !*fd->path) continue;
	
		g_string_append(new, g_shell_quote(fd->path));
		if (work) g_string_append_c(new, ' ');
		}
	
	gtk_clipboard_set_text(clipboard, new->str, new->len);
	g_string_free(new, TRUE);
}
