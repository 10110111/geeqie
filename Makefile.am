## Process this file with automake to produce Makefile.in.

SUBDIRS = src intl po
DIST_SUBDIRS = src intl po

EXTRA_DIST = \
	gqview.desktop	\
	gqview.png	\
	gqview.spec.in

if HAVE_GNOME
desktopdir = $(GNOME_DATADIR)/gnome/apps/Graphics
desktop_DATA = gqview.desktop

icondir = $(GNOME_DATADIR)/pixmaps
icon_DATA = gqview.png
endif

dist-hook: gqview.spec
	cp $(top_builddir)/gqview.spec $(distdir)

