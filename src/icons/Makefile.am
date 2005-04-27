## GQview inline icons

ICONS_INLINE = \
	folder_closed.png	\
	folder_locked.png	\
	folder_open.png		\
	folder_up.png		\
	scroller.png		\
	sheet_broken.png	\
	gqview_logo.png		\
	icon_float.png		\
	icon_thumb.png

ICONS_INLINE_PAIRS = \
	folder_closed		$(srcdir)/folder_closed.png	\
	folder_locked		$(srcdir)/folder_locked.png	\
	folder_open		$(srcdir)/folder_open.png	\
	folder_up		$(srcdir)/folder_up.png		\
	icon_scroller		$(srcdir)/scroller.png		\
	icon_broken		$(srcdir)/sheet_broken.png	\
	gqview_logo		$(srcdir)/gqview_logo.png	\
	icon_float		$(srcdir)/icon_float.png	\
	icon_thumb		$(srcdir)/icon_thumb.png

icons_inline.h: $(ICONS_INLINE)
	gdk-pixbuf-csource --raw --extern --build-list $(ICONS_INLINE_PAIRS) > icons_inline.h

noinst_DATA = icons_inline.h
CLEANFILES = $(noinst_DATA)

## end GQview specific icons


EXTRA_DIST = \
	$(ICONS_INLINE)		\
	collect.xpm		\
	config.xpm		\
	dialog.xpm		\
	icon.xpm		\
	marker.xpm		\
	tools.xpm		\
	view.xpm

