dist_gq_bin_SCRIPTS = geeqie-image-crop

gq_desktopdir = $(appdir)/applications
gq_desktop_in_files = image-crop.desktop.in
gq_desktop_DATA = $(gq_desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(gq_desktop_in_files)
CLEANFILES = $(gq_desktop_DATA)
