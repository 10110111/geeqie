/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"
#include "image.h"

static void warning_dialog_close(GtkWidget *w, gpointer data);
static void warning_dialog_destroy(GtkWidget *w, GdkEvent *event, gpointer data);

static void confirm_dialog_click_cb(GtkWidget *w, gpointer data);
static void confirm_dialog_delete_cb(GtkWidget *w, GdkEvent *event, gpointer data);

static void generic_dialog_delete_cb(GtkWidget *widget, GdkEvent *event, gpointer data);


/*
 *-----------------------------------------------------------------------------
 * warning dialog routines
 *-----------------------------------------------------------------------------
 */ 

static void warning_dialog_close(GtkWidget *w, gpointer data)
{
	GtkWidget *warning_window = data;
	gtk_widget_destroy(warning_window);
}

static void warning_dialog_destroy(GtkWidget *w, GdkEvent *event, gpointer data)
{
	warning_dialog_close(NULL, data);
}

void warning_dialog(gchar *title, gchar *message)
{
	GtkWidget *warning_window;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *button;

	warning_window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_signal_connect (GTK_OBJECT (warning_window), "delete_event",(GtkSignalFunc) warning_dialog_destroy, warning_window);
	gtk_window_set_policy (GTK_WINDOW(warning_window), FALSE, FALSE, TRUE);
	gtk_window_set_title (GTK_WINDOW (warning_window), title);
	gtk_container_border_width (GTK_CONTAINER (warning_window), 10);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(warning_window),vbox);
	gtk_widget_show(vbox);

	label = gtk_label_new(message);
	gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,0);
	gtk_widget_show(label);

	label = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,0);
	gtk_widget_show(label);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(vbox),hbox);
	gtk_widget_show(hbox);

	button = gtk_button_new_with_label(_("     Ok     "));
	gtk_signal_connect (GTK_OBJECT (button), "clicked",(GtkSignalFunc) warning_dialog_close, warning_window);
	gtk_box_pack_start(GTK_BOX(hbox),button,FALSE,FALSE,0);
	gtk_widget_show(button);
	
	gtk_widget_show(warning_window);
}

/*
 *-----------------------------------------------------------------------------
 * confirmation dialog
 *-----------------------------------------------------------------------------
 */ 

static void confirm_dialog_click_cb(GtkWidget *w, gpointer data)
{
	ConfirmDialog *cd = data;
	gtk_widget_destroy(cd->dialog);
	g_free(cd);
}

static void confirm_dialog_delete_cb(GtkWidget *w, GdkEvent *event, gpointer data)
{
	ConfirmDialog *cd = data;
	cd->cancel_cb(NULL, cd->data);
	confirm_dialog_click_cb(w, data);
}

void confirm_dialog_add(ConfirmDialog *cd, gchar *text, void (*func_cb)(GtkWidget *, gpointer))
{
	GtkWidget *button;
	button = gtk_button_new_with_label(text);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", func_cb, cd->data);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", confirm_dialog_click_cb, cd);
	gtk_box_pack_end(GTK_BOX(cd->hbox), button, TRUE, TRUE, 0);
	gtk_widget_grab_focus(button);
	gtk_widget_show(button);
}

ConfirmDialog *confirm_dialog_new(gchar *title, gchar *message, void (*cancel_cb)(GtkWidget *, gpointer), gpointer data)
{
	ConfirmDialog *cd;
	GtkWidget *vbox;
	GtkWidget *label;

	cd = g_new0(ConfirmDialog, 1);
	cd->data = data;
	cd->cancel_cb = cancel_cb;

	cd->dialog = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_signal_connect (GTK_OBJECT (cd->dialog), "delete_event", confirm_dialog_delete_cb, cd);
	gtk_window_set_policy (GTK_WINDOW(cd->dialog), FALSE, FALSE, TRUE);
	gtk_window_set_title (GTK_WINDOW (cd->dialog), title);
	gtk_container_border_width (GTK_CONTAINER (cd->dialog), 10);

	vbox = gtk_vbox_new(FALSE, 15);
	gtk_container_add(GTK_CONTAINER(cd->dialog),vbox);
	gtk_widget_show(vbox);

	label = gtk_label_new(message);
	gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,0);
	gtk_widget_show(label);

	cd->util_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), cd->util_hbox, TRUE, TRUE, 0);
	gtk_widget_show(cd->util_hbox);

	label = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	cd->hbox = gtk_hbox_new(TRUE, 15);
	gtk_box_pack_start(GTK_BOX(vbox), cd->hbox, FALSE, FALSE, 0);
	gtk_widget_show(cd->hbox);

	gtk_widget_show(cd->dialog);

	confirm_dialog_add(cd, _("Cancel"), cd->cancel_cb);
	
	return cd;
}

ConfirmDialog *confirm_dialog_new_with_image(gchar *title, gchar *message,
					     gchar *img_path1, gchar *img_path2,
					     void (*cancel_cb)(GtkWidget *, gpointer), gpointer data)
{
	ConfirmDialog *cd;
	GtkWidget *vbox;
	GtkWidget *label;
	ImageWindow *image1;
	ImageWindow *image2;

	cd = confirm_dialog_new(title, message, cancel_cb, data);

	gtk_widget_set_usize (cd->dialog, 600, 450);
	gtk_window_set_policy (GTK_WINDOW(cd->dialog), FALSE, TRUE, FALSE);

	gtk_box_set_homogeneous(GTK_BOX(cd->util_hbox), TRUE);

	if (img_path1)
		{
		vbox = gtk_vbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(cd->util_hbox), vbox, TRUE, TRUE, 0);
		gtk_widget_show(vbox);

		image1 = image_area_new(NULL);
		gtk_box_pack_start(GTK_BOX(vbox), image1->eventbox, TRUE, TRUE, 0);
		image_area_set_image (image1, img_path1, ZOOM_RESET_ORIGINAL);
		gtk_widget_show_all(image1->eventbox);
		gtk_object_set_data_full(GTK_OBJECT(image1->eventbox), "image1", image1,
					 (GtkDestroyNotify)image_area_free);

		label = gtk_label_new(img_path1);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
		}

	if (img_path2)
		{
		vbox = gtk_vbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(cd->util_hbox), vbox, TRUE, TRUE, 0);
		gtk_widget_show(vbox);

		image2 = image_area_new(NULL);
		gtk_box_pack_start(GTK_BOX(vbox), image2->eventbox, TRUE, TRUE, 0);
		image_area_set_image (image2, img_path2, ZOOM_RESET_ORIGINAL);
		gtk_widget_show_all(image2->eventbox);
		gtk_object_set_data_full(GTK_OBJECT(image2->eventbox), "image2", image2,
					 (GtkDestroyNotify)image_area_free);

		label = gtk_label_new(img_path2);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
		}

	return cd;
}

/*
 *-----------------------------------------------------------------------------
 * generic file ops dialog routines
 *-----------------------------------------------------------------------------
 */ 

void generic_dialog_close(GtkWidget *widget, gpointer data)
{
	FileDialog *fd = data;
	if(fd->dialog) gtk_widget_destroy(fd->dialog);
	g_free(fd->source_path);
	g_free(fd->dest_path);
	if (fd->source_list) free_selected_list(fd->source_list);
	g_free(fd);
}

static void generic_dialog_delete_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	generic_dialog_close(NULL, data);
}

FileDialog *generic_dialog_new(gchar *title, gchar *text, gchar *btn1, gchar *btn2,
		void (*btn1_cb)(GtkWidget *, gpointer),
		void (*btn2_cb)(GtkWidget *, gpointer))
{
	FileDialog *fd = NULL;
	GtkWidget *button;
	GtkWidget *hbox;
	GtkWidget *label;

	fd = g_new0(FileDialog, 1);

	fd->dialog = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_signal_connect (GTK_OBJECT (fd->dialog), "delete_event", (GtkSignalFunc)generic_dialog_delete_cb, fd);
	gtk_window_set_policy (GTK_WINDOW(fd->dialog), FALSE, TRUE, FALSE);
	gtk_window_set_title (GTK_WINDOW (fd->dialog), title);
	gtk_container_border_width (GTK_CONTAINER (fd->dialog), 10);

	fd->vbox = gtk_vbox_new(FALSE,5);
	gtk_container_add (GTK_CONTAINER(fd->dialog), fd->vbox);
	gtk_widget_show(fd->vbox);

	if (text)
		{
		label = gtk_label_new(text);
		gtk_box_pack_start(GTK_BOX(fd->vbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
		}
	
	if (btn1_cb || btn2_cb)
		{
		GtkWidget *sep;
		hbox = gtk_hbox_new(TRUE, 15);
		gtk_box_pack_end(GTK_BOX(fd->vbox), hbox, FALSE, FALSE, 5);
		gtk_widget_show(hbox);

		sep = gtk_hseparator_new();
		gtk_box_pack_end(GTK_BOX(fd->vbox), sep, FALSE, FALSE, 0);
		gtk_widget_show(sep);
		}

	if (btn1_cb)
		{
		button = gtk_button_new_with_label(btn1);
		gtk_signal_connect (GTK_OBJECT (button), "clicked",(GtkSignalFunc) btn1_cb, fd);
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_widget_grab_focus(button);
		gtk_widget_show(button);
		}

	if (btn2_cb)
		{
		button = gtk_button_new_with_label(btn2);
		gtk_signal_connect (GTK_OBJECT (button), "clicked",(GtkSignalFunc) btn2_cb, fd);
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_widget_show(button);
		}

	gtk_widget_show(fd->dialog);
	return fd;
}

