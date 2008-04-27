## Process this file with automake to produce Makefile.in.

SUBDIRS = src po doc
DIST_SUBDIRS = src po doc

man_MANS = geeqie.1

readmedir = @readmedir@
readme_DATA = README COPYING ChangeLog TODO README.lirc

desktopdir = $(datadir)/applications
desktop_DATA = geeqie.desktop

icondir = $(datadir)/pixmaps
icon_DATA = geeqie.png

EXTRA_DIST = \
	$(readme_DATA)	\
	$(desktop_DATA)	\
	$(icon_DATA)	\
	geeqie.spec.in	\
	geeqie.1

dist-hook: geeqie.spec
	cp $(top_builddir)/geeqie.spec $(distdir)


