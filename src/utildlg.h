/*
 * GQview image viewer
 * (C)1999 John Ellis
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
	GtkWidget *hbox;
	void (*cancel_cb)(GtkWidget *, gpointer);
	gpointer data;
};


void warning_dialog(gchar *title, gchar *message);

ConfirmDialog *confirm_dialog_new(gchar *title, gchar *message, void (*cancel_cb)(GtkWidget *, gpointer), gpointer data);
void confirm_dialog_add(ConfirmDialog *cd, gchar *text, void (*func_cb)(GtkWidget *, gpointer));

FileDialog *generic_dialog_new(gchar *title, gchar *text, gchar *btn1, gchar *btn2,
		void (*btn1_cb)(GtkWidget *, gpointer),
		void (*btn2_cb)(GtkWidget *, gpointer));
void generic_dialog_close(GtkWidget *widget, gpointer data);

