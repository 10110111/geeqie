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
contact the GQview author and only maintainer.

The Geeqie project will continue the development forward and maintain the existing code.

Geeqie is currently considered stable.

Please send any questions, problems or suggestions to the [mailing list](mailto:geeqie-devel@sourceforge.net) or
open an issue on [Geeqie at GitHub](https://github.com/BestImageViewer/geeqie/issues).

Subscribe to the mailing list [here](https://lists.sourceforge.net/lists/listinfo/geeqie-devel).

The project website is <http://www.geeqie.org/> and you will find the latest sources in the
[Geeqie repository](http://geeqie.org/cgi-bin/gitweb.cgi?p=geeqie.git).

# README contents:

* Description / Features
* Downloading
* Installation
* Notes and changes for this release
* Requirements


## Description / Features

Geeqie is a graphics file viewer. Basic features:


* Single click image viewing / navigation.

* Zoom functions.

* Thumbnails, with optional caching and .xvpics support.

* Multiple file selection for move, copy, delete, rename, drag and drop.

* Thumbnail preview of the destination for move, copy and rename functions.

* On-the-fly renaming for move and copy functions, with formatted and auto-rename features.

* File grouping (an image having jpeg, RAW and xmp files will appear as a single entity).

* Selectable exif auto-rotation of images.


* Single click file copy or move to pre-defined folders - with undo feature.
* Drag and drop.

* Collections.

* support for stereoscopic images
    * input: side-by-side (JPS) and MPO format
    * output: single image, anaglyph, SBS, mirror, SBS half size (3DTV)

*   Viewing raster and vector images, in the following formats:
3FR, ANI, APM, ARW, BMP, CR2, CRW, CUR, DCR, DNG, ERF, GIF, ICNS, ICO, JPE/JPEG/JPG, JP2/JPC/JPX/J2K/JPF, JPS, K25, KDC, MEF, MPO, MOS, MRW, NEF, ORF, PCX, PEF, PBM/PGM/PNM/PPM, PNG, PTX, QIF/QTIF (QuickTime Image Format), R3D, RAF, RAS (Sun raster), RAW, SR2, SRF, SVG/SVGZ, TGA/TARGA, TIF/TIFF, WBMP, WMF, X3F, XBM, XPM. Animated GIFs are supported.

* Images can be displayed singly in normal or fullscreen mode; static or slideshow mode; in sets of two or four per page for comparison; or as thumbnails of various sizes. Synchronised zoom when multi images are displayed.

* Pan(orama) view displays image thumbnails in calendar, grid, folder and other layouts.
* All available metadata and Exif/IPTC/XMP data can be displayed, as well as colour histograms and assigned tags, keywords and comments.

* Selectable image overlay display box - can contain any text or meta-data.

* Panels can be docked or floating.

* Tags, both predefined and custom, can be assigned to images, and stored either as image metadata (where the file format allows), sidecar files, or in directory metadata files. Keywords and comments can also be assigned.

* Basic editing in the form of lossless 90/180-degree rotation and flipping is supported; external programs such as GIMP, Inkscape, and custom scripts using ImageMagick can be linked to allow further processing.

* Advanced searching is available using criteria such as filename, file size, age, image dimensions, similarity to a specified image, or by keywords or comments. If images have GPS coordinates embedded, you may also search for images within a radius of a geographical point.

* Geeqie supports applying the colour profile embedded in an image along with the system monitor profile (or a user-specified monitor profile).

* Geeqie sessions can be remotely controlled from external software, so it can be used as an image-viewer component of a bigger application.

* Geeqie includes a 'find duplicates' tool which can compare images using a variety of criteria (filename, file size, visual similarity, dimensions, image content), either within a single folder or between two folders. Finding duplicates ignoring the rotation of images is also supported.
* Images may be given a rating value (also known as a "star rating").

* Maps from [OpenStreetMap](http://www.openstreetmap.org) may be displayed in a side panel. If an image has GPS coordinates embedded, its position will be displayed on the map - if Image Direction is encoded, that will be displayed also. If an image does not have embedded GPS coordinates, it may be dragged-and-dropped onto the map to encode its position.

## Downloading

Geeqie is available as a package with some distributions.

The source tar of the latest formal release may be downloaded: <http://geeqie.org/geeqie-1.3.tar.xz>

However Geeqie is stable, and you may download the latest version (if you have installed git) from here:

Either: `git clone git://www.geeqie.org/geeqie.git`

Or: `git clone http://www.geeqie.org/git/geeqie.git`


## Installation

Update secondary help documents (optional -  requires use of git):
        `./gen_changelog.sh ; markdown README.md > README.html`

List compile options: `./autogen.sh --help`

Common options:
`./autogen.sh --disable-gtk3`,
`./autogen.sh --enable-map`


Compilation: `./autogen.sh [options]; make`

Install: `[sudo] make install`

Removal: `[sudo] make uninstall`


## Notes and changes for the latest release

See the NEWS file in the installation folder, or [Geeqie News at GitHub](https://github.com/BestImageViewer/geeqie/blob/master/NEWS)

And either the ChangeLog file or [Geeqie ChangeLog at GitHub](https://github.com/BestImageViewer/geeqie/blob/master/ChangeLog)


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


### Code hackers:

If you plan on making any major changes to the code that will be offered for
inclusion to the main source, please contact us first - so that we can avoid
duplication of effort.
                                                         The Geeqie Team

### Known bugs:

See the Geeqie Bug Tracker at <https://github.com/BestImageViewer/geeqie/issues>
