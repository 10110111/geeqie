      ###################################################################
      ##                          Geeqie x.x                           ##
      ##                                                               ##
      ##              Copyright (C) 2008 - 2017 The Geeqie Team        ##
      ##              Copyright (C) 1999 - 2006 John Ellis.            ##
      ##                                                               ##
      ##                      Use at your own risk!                    ##
      ##                                                               ##
      ##  This software released under the GNU General Public License. ##
      ##       Please read the COPYING file for more information.      ##
      ###################################################################

This is Geeqie, a successor of GQview.

[![Build Status](https://api.travis-ci.org/BestImageViewer/geeqie.svg?branch=master)](https://travis-ci.org/BestImageViewer/geeqie)

Geeqie has been forked from GQview project, because it was not possible to
contact GQview author and the only maintainer. Geeqie projects goal is to move
the development forward and also integrate the existing patches.

Geeqie is currently considered stable. Please report any crash or odd behavior
to the [mailing list](https://lists.sourceforge.net/lists/listinfo/geeqie-devel)
and/or to [Github](https://github.com/BestImageViewer/geeqie/issues)

For more info see: <http://www.geeqie.org/>

Please send any question or suggestions to <geeqie-devel@lists.sourceforge.net> or
open an issue on <https://github.com/BestImageViewer/geeqie/issues>

# README contents:

* Requirements
* Notes and changes for this release
* Downloading
* Installation
* Description / Features

## Requirements

### Required libraries:
    GTK+ 3.00
        www.gtk.org
        enabled by default
        disable with configure option: --disable-gtk3
    or
    GTK+ 2.20
        disabled by default when GTK+3 libraries are found.
        enable with configure option: --disable-gtk3
        optional items map display and GPU acceleration are not available
        with GTK2

        Note: GTK+3 is still somehow experimental. It is needed for some
        features but we have several complains about the GTK+3 usability.

        So if you need a stable version, you are advised to compile it
        with GTK+2. If you want to play with the cool new features, use
        GTK+3.

### Optional libraries:
    lcms2 2.0
    or
    lcms 1.14
        www.littlecms.com
        for color management support
        enabled by default
        disable with configure option: --disable-lcms

    exiv2 0.11
        www.exiv2.org
        for enhanced exif support
        enabled by default
        disable with configure option: --disable-exiv2

    lirc
        www.lirc.org
        for remote control support
        enabled by default
        disable with configure option: --disable-lirc

    libchamplain-gtk 0.12
    libchamplain 0.12
    libclutter 1.0
        wiki.gnome.org/Projects/libchamplain
        for map display
        disabled by default
        enable with configure option: --enable-map
        enabling will also enable GPU acceleration

    libclutter 1.0
        www.clutter-project.org
        for GPU acceleration (a check-box on Preferences/Image must also be ticked)
        disabled by default
        enable with configure option: --enable-gpu-accel
        explicitly disabling will also disable the map feature

    lua 5.1
        www.lua.org
        support for lua scripting
        enabled by default
        disable with configure option: --disable-lua

    awk
        when running Geeqie, to use the geo-decode function

    markdown
        when compiling Geeqie, to create this file in html format


## Notes and changes for this release            [section:release_notes]

See NEWS file.

### Code hackers:

If you plan on making any major changes to the code that will be offered for
inclusion to the main source, please contact us first - so that we can avoid
duplication of effort.
                                                         The Geeqie Team

### Known bugs:

See the Geeqie Bug Tracker at <https://github.com/BestImageViewer/geeqie/issues>


## Downloading

Geeqie is available as a package with some distributions.

The source tar of the latest release may be downloaded: <http://geeqie.org/geeqie-1.3.tar.xz>

To download the sources of the latest commits you must have installed git:

Either: `git clone git://www.geeqie.org/geeqie.git`

Or: `git clone http://www.geeqie.org/git/geeqie.git`


## Installation

Update secondary help documents (optional -  requires use of git):
        `./gen_changelog.sh ; markdown README.md > README.html`

Show compile options: `./autogen.sh --help`

Compilation: `./autogen.sh ; make`

General install: `[sudo] make install`

Removal: `[sudo] make uninstall`

## Description / Features

Geeqie is a graphics file viewer. Basic features:

* Single click image viewing / navigation.
* Zoom functions.
* Thumbnails, with optional caching and .xvpics support.
* Multiple file selection for move, copy, delete, rename, drag and drop.
* Drag and drop.
* Slideshow.
* Full screen.
* Ability to open images in external editors (configurable).
* Collections.
* Comparison of images to find duplicates by name, size, date, dimensions, or image content similarity.
    * Rotation invariant detection
* EXIF support.
* support for stereoscopic images
    * input: side-by-side (JPS) and MPO format
    * output: single image, anaglyph, SBS, mirror, SBS half size (3DTV)

