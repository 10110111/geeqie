SUBDIRS = icons
DIST_SUBDIRS = icons

AM_CFLAGS =				\
	$(GLIB_CFLAGS) $(GTK_CFLAGS)	\
	$(LCMS_CFLAGS)			\
	$(EXIV2_CFLAGS)			\
	-I$(top_srcdir)			\
	-I$(top_builddir)


AM_CXXFLAGS =				\
	$(GLIB_CFLAGS) $(GTK_CFLAGS)	\
	$(LCMS_CFLAGS)			\
	$(EXIV2_CFLAGS)			\
	-I$(top_srcdir)			\
	-I$(top_builddir)

module_SLIK = \
	intl.h		\
	ui_bookmark.c	\
	ui_bookmark.h	\
	ui_fileops.c	\
	ui_fileops.h	\
	ui_help.c	\
	ui_help.h	\
	ui_menu.c	\
	ui_menu.h	\
	ui_misc.c	\
	ui_misc.h	\
	ui_pathsel.c	\
	ui_pathsel.h	\
	ui_spinner.c	\
	ui_spinner.h	\
	ui_tabcomp.c	\
	ui_tabcomp.h	\
	ui_tree_edit.c	\
	ui_tree_edit.h	\
	ui_utildlg.c	\
	ui_utildlg.h

extra_ICONS = \
	ui_spinner.png	\
	ui_tabcomp.png

ICON_PAIRS = \
	icon_spinner		$(srcdir)/ui_spinner.png	\
	icon_tabcomp		$(srcdir)/ui_tabcomp.png

ui_icons.h: $(extra_ICONS) Makefile.in
	@(echo '/* Auto generated file, do not edit */'; echo; \
	 echo '#ifndef UI_ICONS_H'; \
	 echo '#define UI_ICONS_H'; echo; \
	 $(GDK_PIXBUF_CSOURCE) --raw --extern --build-list $(ICON_PAIRS); \
	 echo '#endif /* UI_ICONS_H */') > $@ || echo "!!! Failed to generate $@ !!!"

ClayRGB1998_icc.h: ClayRGB1998.icc
	echo "/*" > $@ 
	echo " * This file was generated from ClayRGB1998.icc, DO NOT EDIT " >> $@
	echo " * http://www.argyllcms.com/ClayRGB1998.zip " >> $@
	echo " */ " >> $@
	xxd -i $^ >> $@

ui_spinner.o: ui_icons.h


noinst_DATA = ui_icons.h
CLEANFILES = $(noinst_DATA)
                                                                                                                     
extra_SLIK = \
	$(extra_ICONS)

bin_PROGRAMS = geeqie

geeqie_SOURCES = \
	$(module_SLIK)	\
	ClayRGB1998.icc \
	ClayRGB1998_icc.h \
	bar_info.c	\
	bar_info.h	\
	bar_exif.c	\
	bar_exif.h	\
	bar_sort.c	\
	bar_sort.h	\
	cache.c		\
	cache.h		\
	cache-loader.c	\
	cache-loader.h	\
	cache_maint.c	\
	cache_maint.h	\
	cellrenderericon.c	\
	cellrenderericon.h	\
	collect.c	\
	collect.h	\
	collect-dlg.c	\
	collect-dlg.h	\
	collect-io.c	\
	collect-io.h	\
	collect-table.c	\
	collect-table.h	\
	color-man.c	\
	color-man.h	\
	compat.c	\
	compat.h	\
	debug.c		\
	debug.h		\
	dnd.c		\
	dnd.h		\
	dupe.c		\
	dupe.h		\
	editors.c	\
	editors.h	\
	exif.c		\
	exif.h		\
	exif-int.h	\
	exif-common.c   \
	exiv2.cc	\
	filecache.c	\
	filecache.h	\
	filedata.c	\
	filedata.h	\
	filefilter.c	\
	filefilter.h	\
	format_canon.c	\
	format_canon.h	\
	format_fuji.c	\
	format_fuji.h	\
	format_nikon.c	\
	format_nikon.h	\
	format_olympus.c	\
	format_olympus.h	\
	format_raw.c	\
	format_raw.h	\
	fullscreen.c	\
	fullscreen.h	\
	histogram.c	\
	histogram.h	\
	image.c		\
	image.h		\
	image-load.c	\
	image-load.h	\
	image-overlay.c	\
	image-overlay.h	\
	img-view.c	\
	img-view.h	\
	info.c		\
	info.h		\
	layout.c	\
	layout.h	\
	layout_config.c	\
	layout_config.h	\
	layout_image.c	\
	layout_image.h	\
	layout_util.c	\
	layout_util.h	\
	lirc.c		\
	lirc.h		\
	logwindow.c	\
	logwindow.h	\
	main.c		\
	main.h		\
	md5-util.c	\
	md5-util.h	\
	menu.c		\
	menu.h		\
	options.c	\
	options.h	\
	pan-calendar.c	\
	pan-folder.c	\
	pan-grid.c	\
	pan-item.c	\
	pan-timeline.c	\
	pan-types.h	\
	pan-util.c	\
	pan-view.c	\
	pan-view.h	\
	pixbuf-renderer.c	\
	pixbuf-renderer.h	\
	pixbuf_util.c	\
	pixbuf_util.h	\
	preferences.c	\
	preferences.h	\
	print.c		\
	print.h		\
	remote.c	\
	remote.h	\
	rcfile.c	\
	rcfile.h	\
	search.c	\
	search.h	\
	secure_save.c	\
	secure_save.h	\
	similar.c	\
	similar.h	\
	slideshow.c	\
	slideshow.h	\
	typedefs.h	\
	thumb.c		\
	thumb.h		\
	thumb_standard.c	\
	thumb_standard.h	\
	trash.c		\
	trash.h		\
	utilops.c	\
	utilops.h	\
	view_dir.c	\
	view_dir.h	\
	view_dir_list.c	\
	view_dir_list.h	\
	view_dir_tree.c	\
	view_dir_tree.h	\
	view_file.c	\
	view_file.h	\
	view_file_list.c	\
	view_file_list.h	\
	view_file_icon.c	\
	view_file_icon.h	\
	window.c	\
	window.h

geeqie_LDADD = $(GTK_LIBS) $(GLIB_LIBS) $(INTLLIBS) $(LCMS_LIBS) $(EXIV2_LIBS)

EXTRA_DIST = \
	$(extra_SLIK)

