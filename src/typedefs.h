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


#ifndef TYPEDEFS_H
#define TYPEDEFS_H

typedef enum {
	DIRVIEW_LIST,
	DIRVIEW_TREE
} DirViewType;

typedef enum {
	CMD_COPY = GQ_EDITOR_GENERIC_SLOTS,
	CMD_MOVE,
	CMD_RENAME,
	CMD_DELETE,
	CMD_FOLDER,
	GQ_EDITOR_SLOTS
} SpecialEditor;

typedef enum {
	SORT_NONE,
	SORT_NAME,
	SORT_SIZE,
	SORT_TIME,
	SORT_PATH,
	SORT_NUMBER
} SortType;

typedef enum {
	ALTER_NONE,		/* do nothing */
	ALTER_ROTATE_90,
	ALTER_ROTATE_90_CC,	/* counterclockwise */
	ALTER_ROTATE_180,
	ALTER_MIRROR,
	ALTER_FLIP,
	ALTER_DESATURATE
} AlterType;

typedef enum {
	LAYOUT_HIDE   = 0,
	LAYOUT_LEFT   = 1 << 0,
	LAYOUT_RIGHT  = 1 << 1,
	LAYOUT_TOP    = 1 << 2,
	LAYOUT_BOTTOM = 1 << 3
} LayoutLocation;


typedef enum {
	IMAGE_STATE_NONE	= 0,
	IMAGE_STATE_IMAGE	= 1 << 0,
	IMAGE_STATE_LOADING	= 1 << 1,
	IMAGE_STATE_ERROR	= 1 << 2,
	IMAGE_STATE_COLOR_ADJ	= 1 << 3,
	IMAGE_STATE_ROTATE_AUTO	= 1 << 4,
	IMAGE_STATE_ROTATE_USER	= 1 << 5,
	IMAGE_STATE_DELAY_FLIP	= 1 << 6
} ImageState;

typedef enum {
	SPLIT_NONE = 0,
	SPLIT_VERT,
	SPLIT_HOR,
	SPLIT_QUAD,
} ImageSplitMode;

typedef enum {
	FILEDATA_CHANGE_DELETE,
	FILEDATA_CHANGE_MOVE,
	FILEDATA_CHANGE_RENAME,
	FILEDATA_CHANGE_COPY
} FileDataChangeType;

typedef enum {
	MTS_MODE_MINUS,
	MTS_MODE_SET,
	MTS_MODE_OR,
	MTS_MODE_AND
} MarkToSelectionMode;

typedef enum {
	STM_MODE_RESET,
	STM_MODE_SET,
	STM_MODE_TOGGLE
} SelectionToMarkMode;

typedef enum {
	FORMAT_CLASS_UNKNOWN,
	FORMAT_CLASS_IMAGE,
	FORMAT_CLASS_RAWIMAGE,
	FORMAT_CLASS_META,
	FILE_FORMAT_CLASSES
} FileFormatClass;

typedef enum {
	SS_ERR_NONE = 0,
	SS_ERR_DISABLED, /**< secsave is disabled. */
	SS_ERR_OUT_OF_MEM, /**< memory allocation failure */

	/* see err field in SecureSaveInfo */
	SS_ERR_OPEN_READ,
	SS_ERR_OPEN_WRITE,
	SS_ERR_STAT,
	SS_ERR_ACCESS,
	SS_ERR_MKSTEMP,
	SS_ERR_RENAME,
	SS_ERR_OTHER,
} SecureSaveErrno;


#define MAX_SPLIT_IMAGES 4

typedef struct _ImageLoader ImageLoader;
typedef struct _ThumbLoader ThumbLoader;

typedef struct _CollectInfo CollectInfo;
typedef struct _CollectionData CollectionData;
typedef struct _CollectTable CollectTable;
typedef struct _CollectWindow CollectWindow;

typedef struct _ImageWindow ImageWindow;

typedef struct _FileData FileData;
typedef struct _FileDataChangeInfo FileDataChangeInfo;

typedef struct _LayoutWindow LayoutWindow;
typedef struct _ViewDir ViewDir;
typedef struct _ViewDirInfoList ViewDirInfoList;
typedef struct _ViewDirInfoTree ViewDirInfoTree;
typedef struct _ViewFileList ViewFileList;
typedef struct _ViewFileIcon ViewFileIcon;

typedef struct _SlideShowData SlideShowData;
typedef struct _FullScreenData FullScreenData;

typedef struct _PixmapFolders PixmapFolders;
typedef struct _Histogram Histogram;

typedef struct _SecureSaveInfo SecureSaveInfo;

typedef struct _ConfOptions ConfOptions;

struct _ImageLoader
{
	GdkPixbuf *pixbuf;
	FileData *fd;
	gchar *path;

	gint bytes_read;
	gint bytes_total;

	guint buffer_size;

	gint requested_width;
	gint requested_height;
	gint shrunk;

	gint done;
	gint idle_id;
	gint idle_priority;

	GdkPixbufLoader *loader;
	gint load_fd;

	void (*func_area_ready)(ImageLoader *, guint x, guint y, guint w, guint h, gpointer);
	void (*func_error)(ImageLoader *, gpointer);
	void (*func_done)(ImageLoader *, gpointer);
	void (*func_percent)(ImageLoader *, gdouble, gpointer);

	gpointer data_area_ready;
	gpointer data_error;
	gpointer data_done;
	gpointer data_percent;

	gint idle_done_id;
};

typedef void (* ThumbLoaderFunc)(ThumbLoader *tl, gpointer data);

struct _ThumbLoader
{
	gint standard_loader;

	GdkPixbuf *pixbuf;	/* contains final (scaled) image when done */
	ImageLoader *il;
	gchar *path;

	gint cache_enable;
	gint cache_hit;
	gdouble percent_done;

	gint max_w;
	gint max_h;

	ThumbLoaderFunc func_done;
	ThumbLoaderFunc func_error;
	ThumbLoaderFunc func_progress;

	gpointer data;

	gint idle_done_id;
};

struct _CollectInfo
{
	FileData *fd;
	GdkPixbuf *pixbuf;
	gint flag_mask;
};

struct _CollectionData
{
	gchar *path;
	gchar *name;
	GList *list;
	SortType sort_method;

	ThumbLoader *thumb_loader;
	CollectInfo *thumb_info;

	void (*info_updated_func)(CollectionData *, CollectInfo *, gpointer);
	gpointer info_updated_data;

	gint ref;

	/* geometry */
	gint window_read;
	gint window_x;
	gint window_y;
	gint window_w;
	gint window_h;

	/* contents changed since save flag */
	gint changed;

	GHashTable *existence;
};

struct _CollectTable
{
	GtkWidget *scrolled;
	GtkWidget *listview;
	gint columns;
	gint rows;

	CollectionData *cd;

	GList *selection;
	CollectInfo *prev_selection;

	CollectInfo *click_info;

	GtkWidget *tip_window;
	gint tip_delay_id;
	CollectInfo *tip_info;

	GdkWindow *marker_window;
	CollectInfo *marker_info;

	GtkWidget *status_label;
	GtkWidget *extra_label;

	gint focus_row;
	gint focus_column;
	CollectInfo *focus_info;

	GtkWidget *popup;
	CollectInfo *drop_info;
	GList *drop_list;

	gint sync_idle_id;
	gint drop_idle_id;

	gint show_text;
};

struct _CollectWindow
{
	GtkWidget *window;
	CollectTable *table;
	GtkWidget *status_box;
	GList *list;

	GtkWidget *close_dialog;

	CollectionData *cd;
};

typedef gint (* ImageTileRequestFunc)(ImageWindow *imd, gint x, gint y,
				      gint width, gint height, GdkPixbuf *pixbuf, gpointer);
typedef void (* ImageTileDisposeFunc)(ImageWindow *imd, gint x, gint y,
				      gint width, gint height, GdkPixbuf *pixbuf, gpointer);

struct _ImageWindow
{
	GtkWidget *widget;	/* use this to add it and show it */
	GtkWidget *pr;
	GtkWidget *frame;

	FileData *image_fd;

	gint64 size;		/* file size (bytes) */
	time_t mtime;		/* file modified time stamp */
	gint unknown;		/* failed to load image */

	ImageLoader *il;

	gint has_frame;

	/* top level (not necessarily parent) window */
	gint top_window_sync;	/* resize top_window when image dimensions change */
	GtkWidget *top_window;	/* window that gets title, and window to resize when 'fitting' */
	gchar *title;		/* window title to display left of file name */
	gchar *title_right;	/* window title to display right of file name */
	gint title_show_zoom;	/* option to include zoom in window title */

	gint completed;
	ImageState state;	/* mask of IMAGE_STATE_* flags about current image */

	void (*func_update)(ImageWindow *imd, gpointer data);
	void (*func_complete)(ImageWindow *imd, gint preload, gpointer data);
	void (*func_state)(ImageWindow *imd, ImageState state, gpointer data);
	ImageTileRequestFunc func_tile_request;
	ImageTileDisposeFunc func_tile_dispose;

	gpointer data_update;
	gpointer data_complete;
	gpointer data_state;
	gpointer data_tile;

	/* button, scroll functions */
	void (*func_button)(ImageWindow *, gint button,
			    guint32 time, gdouble x, gdouble y, guint state, gpointer);
	void (*func_drag)(ImageWindow *, gint button,
			    guint32 time, gdouble x, gdouble y, guint state, gdouble dx, gdouble dy,gpointer);
	void (*func_scroll)(ImageWindow *, GdkScrollDirection direction,
			    guint32 time, gdouble x, gdouble y, guint state, gpointer);

	gpointer data_button;
	gpointer data_drag;
	gpointer data_scroll;

	/* scroll notification (for scroll bar implementation) */
	void (*func_scroll_notify)(ImageWindow *, gint x, gint y, gint width, gint height, gpointer);

	gpointer data_scroll_notify;

	/* collection info */
	CollectionData *collection;
	CollectInfo *collection_info;

	/* color profiles */
	gint color_profile_enable;
	gint color_profile_input;
	gint color_profile_screen;
	gint color_profile_use_image;
	gpointer cm;

	AlterType delay_alter_type;

	ImageLoader *read_ahead_il;
	GdkPixbuf *read_ahead_pixbuf;
	FileData *read_ahead_fd;

	GdkPixbuf *prev_pixbuf;
	FileData *prev_fd;
	gint prev_color_row;

	gint auto_refresh_id;
	gint auto_refresh_interval;

	gint delay_flip;
	gint orientation;
	gint desaturate;
};

#define FILEDATA_MARKS_SIZE 6

struct _FileDataChangeInfo {
	FileDataChangeType type;
	gchar *source;
	gchar *dest;
};

struct _FileData {
	gint magick;
	gint type;
	gchar *original_path; /* key to file_data_pool hash table */
	gchar *path;
	const gchar *name;
	const gchar *extension;
	gint64 size;
	time_t date;
	gboolean marks[FILEDATA_MARKS_SIZE];
	GList *sidecar_files;
	FileData *parent; /* parent file if this is a sidecar file, NULL otherwise */
	FileDataChangeInfo *change; /* for rename, move ... */
	GdkPixbuf *pixbuf;
	gint ref;
};

struct _LayoutWindow
{
	gchar *path;

	/* base */

	GtkWidget *window;

	GtkWidget *main_box;

	GtkWidget *group_box;
	GtkWidget *h_pane;
	GtkWidget *v_pane;

	/* menus, path selector */

	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;

	GtkWidget *path_entry;

	/* image */

	LayoutLocation image_location;

	ImageWindow *image;

	ImageWindow *split_images[MAX_SPLIT_IMAGES];
        ImageSplitMode split_mode;
	gint active_split_image;

        GtkWidget *split_image_widget;
	
	gint connect_zoom, connect_scroll;
	
	/* tools window (float) */

	GtkWidget *tools;
	GtkWidget *tools_pane;

	gint tools_float;
	gint tools_hidden;

	/* toolbar */

	GtkWidget *toolbar;
	gint toolbar_hidden;

	GtkWidget *thumb_button;
	gint thumbs_enabled;
	gint marks_enabled;
    
	/* dir view */

	LayoutLocation dir_location;

	ViewDir *vd;
	GtkWidget *dir_view;

	DirViewType dir_view_type;

	/* file view */

	LayoutLocation file_location;

	ViewFileList *vfl;
	ViewFileIcon *vfi;
	GtkWidget *file_view;

	gint icon_view;
	SortType sort_method;
	gint sort_ascend;

	/* status bar */

	GtkWidget *info_box;
	GtkWidget *info_progress_bar;
	GtkWidget *info_sort;
	GtkWidget *info_color;
	GtkWidget *info_status;
	GtkWidget *info_details;
	GtkWidget *info_zoom;

	/* slide show */

	SlideShowData *slideshow;

	/* full screen */

	FullScreenData *full_screen;

	/* dividers */

	gint div_h;
	gint div_v;
	gint div_float;

	/* directory update check */

	gint last_time_id;
	time_t last_time;

	/* misc */

	GtkWidget *utility_box;
	GtkWidget *bar_sort;
	GtkWidget *bar_exif;
	GtkWidget *bar_info;

	gint histogram_enabled;
	Histogram *histogram;

	gint bar_sort_enabled;
	gint bar_exif_enabled;
	gint bar_info_enabled;

	gint bar_exif_size;
	gint bar_exif_advanced;
};

struct _ViewDir
{
	DirViewType type;
	gpointer info;

	GtkWidget *widget;
	GtkWidget *view;

	gchar *path;

	FileData *click_fd;

	FileData *drop_fd;
	GList *drop_list;
	gint drop_scroll_id;

	/* func list */
	void (*select_func)(ViewDir *vd, const gchar *path, gpointer data);
	gpointer select_data;

	void (*dnd_drop_update_func)(ViewDir *vd);
	void (*dnd_drop_leave_func)(ViewDir *vd);
	
	LayoutWindow *layout;

	GtkWidget *popup;

	PixmapFolders *pf;
};

struct _ViewDirInfoList
{
	GList *list;
};

struct _ViewDirInfoTree
{
	gint drop_expand_id;
	gint busy_ref;
};

struct _ViewFileList
{
	GtkWidget *widget;
	GtkWidget *listview;

	gchar *path;
	GList *list;

	SortType sort_method;
	gint sort_ascend;

	FileData *click_fd;
	FileData *select_fd;

	gint thumbs_enabled;
	gint marks_enabled;
	gint active_mark;
    
	/* thumb updates */
	gint thumbs_running;
	gint thumbs_count;
	ThumbLoader *thumbs_loader;
	FileData *thumbs_filedata;

	/* func list */
	void (*func_thumb_status)(ViewFileList *vfl, gdouble val, const gchar *text, gpointer data);
	gpointer data_thumb_status;

	void (*func_status)(ViewFileList *vfl, gpointer data);
	gpointer data_status;

	gint select_idle_id;
	LayoutWindow *layout;

	GtkWidget *popup;
};

struct _IconData;

struct _ViewFileIcon
{
	GtkWidget *widget;
	GtkWidget *listview;

	gchar *path;
	GList *list;

	/* table stuff */

	gint columns;
	gint rows;

	GList *selection;
	struct _IconData *prev_selection;

	GtkWidget *tip_window;
	gint tip_delay_id;
	struct _IconData *tip_id;

	struct _IconData *click_id;

	struct _IconData *focus_id;
	gint focus_row;
	gint focus_column;

	SortType sort_method;
	gint sort_ascend;

	gint show_text;

	gint sync_idle_id;

	/* thumbs */
	
	gint thumbs_running;
	GList *thumbs_list;
	gint thumbs_count;
	ThumbLoader *thumbs_loader;
	FileData *thumbs_fd;

	/* func list */
	void (*func_thumb_status)(ViewFileIcon *vfi, gdouble val, const gchar *text, gpointer data);
	gpointer data_thumb_status;

	void (*func_status)(ViewFileIcon *vfi, gpointer data);
	gpointer data_status;

	LayoutWindow *layout;

	GtkWidget *popup;
};

struct _SlideShowData
{
	ImageWindow *imd;

	GList *filelist;
	CollectionData *cd;
	gchar *layout_path;
	LayoutWindow *layout;

	GList *list;
	GList *list_done;

	FileData *slide_fd;

	gint slide_count;
	gint timeout_id;

	gint from_selection;

	void (*stop_func)(SlideShowData *, gpointer);
	gpointer stop_data;

	gint paused;
};

struct _FullScreenData
{
	GtkWidget *window;
	ImageWindow *imd;

	GtkWidget *normal_window;
	ImageWindow *normal_imd;

	gint hide_mouse_id;
	gint busy_mouse_id;
	gint cursor_state;

	gint saver_block_id;

	void (*stop_func)(FullScreenData *, gpointer);
	gpointer stop_data;
};

struct _PixmapFolders
{
	GdkPixbuf *close;
	GdkPixbuf *open;
	GdkPixbuf *deny;
	GdkPixbuf *parent;
};

struct _SecureSaveInfo {
	FILE *fp; /**< file stream pointer */
	gchar *file_name; /**< final file name */
	gchar *tmp_file_name; /**< temporary file name */
	gint err; /**< set to non-zero value in case of error */
	gint secure_save; /**< use secure save for this file, internal use only */
	gint preserve_perms; /**< whether to preserve perms, TRUE by default */
	gint preserve_mtime; /**< whether to preserve mtime, FALSE by default */
	gint unlink_on_error; /**< whether to remove temporary file on save failure, TRUE by default */
};

struct _ConfOptions
{

	/* ui */
	gint progressive_key_scrolling;
	gint place_dialogs_under_mouse;
	gint mousewheel_scrolls;
	gint show_icon_names;

	/* various */
	gint startup_path_enable;
	gchar *startup_path;
	gint enable_metadata_dirs;

	gint tree_descend_subdirs;

	gint lazy_image_sync;
	gint update_on_time_change;

	gint duplicates_similarity_threshold;

	gint open_recent_list_maxsize;

	/* file ops */
	struct {
		gint enable_in_place_rename;
		
		gint confirm_delete;
		gint enable_delete_key;
		gint safe_delete_enable;
		gchar *safe_delete_path;
		gint safe_delete_folder_maxsize;
	} file_ops;

	/* image */
	struct {
		gint exif_rotate_enable;
		gint scroll_reset_method;
		gint fit_window_to_image;
		gint limit_window_size;
		gint max_window_size;
		gint limit_autofit_size;
		gint max_autofit_size;

		gint tile_cache_max;	/* in megabytes */
		gint dither_quality;
		gint enable_read_ahead;

		gint zoom_mode;
		gint zoom_2pass;
		gint zoom_to_fit_allow_expand;
		gint zoom_quality;
		gint zoom_increment;	/* 10 is 1.0, 5 is 0.05, 20 is 2.0, etc. */

		gint use_custom_border_color;
		GdkColor border_color;
	} image;

	/* thumbnails */
	struct {
		gint max_width;
		gint max_height;
		gint enable_caching;
		gint cache_into_dirs;
		gint fast;
		gint use_xvpics;
		gint spec_standard;
		gint quality;
	} thumbnails;

	/* file filtering */
	struct {
		gint show_hidden_files;
		gint show_dot_directory;
		gint disable;
	} file_filter;

	/* collections */
	struct {
		gint rectangular_selection;
	} collections;

	/* editors */
	gchar *editor_name[GQ_EDITOR_SLOTS];
	gchar *editor_command[GQ_EDITOR_SLOTS];

	/* file sorting */
	struct {
		SortType method;
		gint ascending;
		gint case_sensitive; /* file sorting method (case) */
	} file_sort;

	/* slideshow */
	struct {
		gint delay;	/* in tenths of a second */
		gint random;
		gint repeat;
	} slideshow;

	/* fullscreen */
	struct 	{
		gint screen;
		gint clean_flip;
		gint disable_saver;
		gint above;
		gint show_info;
		gchar *info;
	} fullscreen;

	/* layout */
	struct {
		gchar *order;
		gint style;

		gint view_as_icons;
		DirViewType dir_view_type;
		
		gint show_thumbnails;

		struct {
			gint w;
			gint h;
			gint x;
			gint y;
			gint maximized;
			gint hdivider_pos;
			gint vdivider_pos;
		} main_window;

		struct {
			gint w;
			gint h;
			gint x;
			gint y;
			gint vdivider_pos;
		} float_window;

		gint save_window_positions;

		gint tools_float;
		gint tools_hidden;
		gint tools_restore_state;

		gint toolbar_hidden;

	} layout;

	/* color profiles */
	struct {
		gint enabled;
		gint input_type;
		gchar *input_file[COLOR_PROFILE_INPUTS];
		gchar *input_name[COLOR_PROFILE_INPUTS];
		gint screen_type;
		gchar *screen_file;
		gint use_image;

	} color_profile;

};

#endif


