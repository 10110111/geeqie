/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

typedef struct _FileDialog FileDialog;
struct _FileDialog
{
	gint type;
	gint multiple_files;

	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *entry;

	gchar *source_path;
	GList *source_list;

	gchar *dest_path;
};

typedef struct _ConfirmDialog ConfirmDialog;
struct _ConfirmDialog
{
	GtkWidget *dialog;
	GtkWidget *util_hbox;	/* place anything you want here */
	GtkWidget *hbox;	/* buttons */
	void (*cancel_cb)(GtkWidget *, gpointer);
	gpointer data;
};


void warning_dialog(gchar *title, gchar *message);

ConfirmDialog *confirm_dialog_new(gchar *title, gchar *message, void (*cancel_cb)(GtkWidget *, gpointer), gpointer data);
ConfirmDialog *confirm_dialog_new_with_image(gchar *title, gchar *message,
					     gchar *img_path1, gchar *img_path2,
					     void (*cancel_cb)(GtkWidget *, gpointer), gpointer data);

void confirm_dialog_add(ConfirmDialog *cd, gchar *text, void (*func_cb)(GtkWidget *, gpointer));

FileDialog *generic_dialog_new(gchar *title, gchar *text, gchar *btn1, gchar *btn2,
		void (*btn1_cb)(GtkWidget *, gpointer),
		void (*btn2_cb)(GtkWidget *, gpointer));
void generic_dialog_close(GtkWidget *widget, gpointer data);

