SUBDIRS = icons
DIST_SUBDIRS = icons

INCLUDES = -I$(srcdir) -I$(builddir)    \
	-I$(top_srcdir) -I$(top_builddir)       \
	-DLOCALEDIR=\""$(localedir)"\"		\
	-I$(includedir) $(GTK_CFLAGS) $(GDK_IMLIB_CFLAGS)

bin_PROGRAMS = gqview

gqview_SOURCES = \
	config.c	\
	dnd.c		\
	filelist.c	\
	fileops.c	\
	globals.c	\
	gqview.h	\
	image.c		\
	image.h		\
	img-main.c	\
	img-view.c	\
	intl.h		\
	logo.h		\
	main.c		\
	menu.c		\
	pathsel.c	\
	rcfile.c	\
	slideshow.c	\
	tabcomp.c	\
	thumb.c		\
	utildlg.c	\
	utildlg.h	\
	utilops.c	\
	window.c

gqview_LDADD = $(GTK_LIBS) $(GDK_IMLIB_LIBS)

EXTRA_DIST = tabcomp.xpm

