SUBDIRS = svg
DIST_SUBDIRS = svg

## GQview inline icons

ICONS_INLINE = \
	folder_closed.png	\
	folder_locked.png	\
	folder_open.png		\
	folder_up.png		\
	gqview_icon.png		\
	geeqie_logo.png		\
	scroller.png		\
	sheet_broken.png	\
	sheet_metadata.png	\
	sheet_unknown.png	\
	sheet_video.png	\
	collection.png	\
	icon_float.png		\
	icon_thumb.png		\
	icon_book.png		\
	icon_config.png		\
	icon_tools.png		\
	icon_view.png		\
	icon_guidelines.png	\
	icon_panorama.png	\
	icon_maintenance.png	\
	icon_zoomfillhor.png	\
	icon_zoomfillvert.png	\
	icon_hidetools.png	\
	icon_exif.png	\
	icon_marks.png	\
	icon_info.png	\
	icon_sort.png	\
	icon_pdf.png	\
	icon_draw_rectangle.png \
	icon_move.png \
	icon_rename.png \
	icon_select_all.png \
	icon_select_none.png \
	icon_select_invert.png \
	icon_select_rectangle.png


ICONS_INLINE_PAIRS = \
	folder_closed		$(srcdir)/folder_closed.png	\
	folder_locked		$(srcdir)/folder_locked.png	\
	folder_open		$(srcdir)/folder_open.png	\
	folder_up		$(srcdir)/folder_up.png		\
	gqview_icon		$(srcdir)/gqview_icon.png	\
	geeqie_logo		$(srcdir)/geeqie_logo.png	\
	icon_float		$(srcdir)/icon_float.png	\
	icon_thumb		$(srcdir)/icon_thumb.png	\
	icon_scroller		$(srcdir)/scroller.png		\
	icon_broken		$(srcdir)/sheet_broken.png	\
	icon_metadata		$(srcdir)/sheet_metadata.png	\
	icon_unknown		$(srcdir)/sheet_unknown.png	\
	icon_video		$(srcdir)/sheet_video.png	\
	icon_collection	$(srcdir)/collection.png	\
	icon_book		$(srcdir)/icon_book.png		\
	icon_config		$(srcdir)/icon_config.png	\
	icon_tools		$(srcdir)/icon_tools.png	\
	icon_view		$(srcdir)/icon_view.png		\
	icon_guidelines	$(srcdir)/icon_guidelines.png	\
	icon_panorama	$(srcdir)/icon_panorama.png	\
	icon_maintenance	$(srcdir)/icon_maintenance.png	\
	icon_zoomfillhor	$(srcdir)/icon_zoomfillhor.png	\
	icon_zoomfillvert	$(srcdir)/icon_zoomfillvert.png	\
	icon_hidetools	$(srcdir)/icon_hidetools.png	\
	icon_exif	$(srcdir)/icon_exif.png	\
	icon_marks	$(srcdir)/icon_marks.png	\
	icon_info	$(srcdir)/icon_info.png	\
	icon_sort	$(srcdir)/icon_sort.png	\
	icon_pdf	$(srcdir)/icon_pdf.png \
	icon_draw_rectangle	$(srcdir)/icon_draw_rectangle.png \
	icon_move	$(srcdir)/icon_move.png \
	icon_rename	$(srcdir)/icon_rename.png \
	icon_select_all	$(srcdir)/icon_select_all.png \
	icon_select_none	$(srcdir)/icon_select_none.png \
	icon_select_invert	$(srcdir)/icon_select_invert.png \
	icon_select_rectangle	$(srcdir)/icon_select_rectangle.png

icons_inline.h: $(ICONS_INLINE) Makefile.in
	@sh -ec "echo '/* Auto generated file, do not edit */'; echo; \
	 echo '#ifndef ICONS_INLINE_H'; \
	 echo '#define ICONS_INLINE_H'; echo; \
	 $(GDK_PIXBUF_CSOURCE) --raw --extern --build-list $(ICONS_INLINE_PAIRS); \
	 echo '#endif /* ICONS_INLINE_H */'" > $@ || echo "!!! Failed to generate $@ !!!"

noinst_DATA = icons_inline.h
CLEANFILES = $(noinst_DATA)

## end GQview specific icons


EXTRA_DIST = \
	$(ICONS_INLINE)		\
	marker.xpm
