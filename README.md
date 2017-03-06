      ###################################################################
      ##                          Geeqie x.x                           ##
      ##                                                               ##
      ##              Copyright (C) 2008 - 2016 The Geeqie Team        ##
      ##              Copyright (C) 1999 - 2006 John Ellis.            ##
      ##                                                               ##
      ##                      Use at your own risk!                    ##
      ##                                                               ##
      ##  This software released under the GNU General Public License. ##
      ##       Please read the COPYING file for more information.      ##
      ###################################################################

This is Geeqie, a successor of GQview.

Geeqie has been forked from GQview project, because it was not possible to
contact GQview author and the only maintainer. Geeqie projects goal is to move
the development forward and also integrate the existing patches.

Geeqie is currently considered stable. Please report any crash or odd behavior
to the [mailing list](https://lists.sourceforge.net/lists/listinfo/geeqie-devel)
and/or to [Github](https://github.com/BestImageViewer/geeqie/issues)

For more info see: http://www.geeqie.org/

Please send any question or suggestions to geeqie-devel@lists.sourceforge.net or
open an issue on Github (https://github.com/BestImageViewer/geeqie/issues)

# README contents:

* Requirements
* Notes and changes for this release
* Installation
* Description / Features
* Documentation (keyboard shortcuts)
* Editor command macros
* Additional comments
* Translation status
* Credits

## Requirements

  Required libraries:
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

  Optional libraries:
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

## Notes and changes for this release            [section:release_notes]

See NEWS file.

  Code hackers:

    If you plan on making any major changes to the code that will be offered for
    inclusion to the main source, please contact us first - so that we can avoid
    duplication of effort.
                                                         The Geeqie Team

  Known bugs:

    See the Geeqie Bug Tracker at https://github.com/BestImageViewer/geeqie/issues

## Installation

  Compilation: ./autogen.sh ; make
  Show compile options: ./autogen.sh --help
  General install: make install
  Removal: make uninstall

## Description / Features

  Geeqie is a graphics file viewer. Basic features:

- Single click image viewing / navigation.
- Zoom functions.
- Thumbnails, with optional caching and .xvpics support.
- Multiple file selection for move, copy, delete, rename, drag and drop.
- Drag and drop.
- Slideshow.
- Full screen.
- Ability to open images in external editors (configurable).
- Collections.
- Comparison of images to find duplicates by name, size, date,
  dimensions, or image content similarity.
  - Rotation invariant detection
  - EXIF support.
  - support for stereoscopic images
    - input: side-by-side (JPS) and MPO format
    - output: single image, anaglyph, SBS, mirror, SBS half size (3DTV)

## Credits                                             [section:credits]

  Translations:

     Grzegorz Kowal <g_kowal@poczta.onet.pl>
     Zbigniew Chyla <cyba@gnome.pl>
     Emil Nowak <emil5@go2.pl>
     Wit Wilinski <wit.wilinski@gmail.com>
     Tomasz Golinski <tomaszg@math.uwb.edu.pl>
         for Polish translation

     Christopher R. Gabriel <cgabriel@pluto.linux.it>
     Di Maggio Salvatore <Salvatore.Dimaggio@bologna.marelli.it>
     Costantino <inverness1ATvirgilio.it>
         for Italian translation

     Sandokan <cortex@nextra.sk>
     Ivan Priesol <priesol@iris-sk.sk>
     Mgr. Peter Tuharsky <tuharsky@misbb.sk>
         for Slovak translation

     Rodrigo Sancho Senosiain <ruy_ikari@bigfoot.com>
     Ariel Fermani <the_end@bbs.frc.utn.edu.ar>
         for Spanish translation

     Laurent Monin <i18n@norz.org>
     Eric Lassauge <lassauge@users.sf.net>
     Jean-Pierre Pedron <jppedron@club-internet.fr>
     Pascal Bleser <pascal.bleser@atosorigin.com>
     Nicolas Boos <nicolas.boos@wanadoo.fr>
     Nicolas Bonifas <nicolas_bonifas@users.sf.net>
         for French translation

     Fâtih Demir <kabalak@gmx.net>
         for Turkish translation

     Kam Tik <kamtik@hongkong.com>
     Abel Cheung <deaddog@deaddog.ws>
     S.J. Luo <crystal@mickey.ee.nctu.edu.tw>
     Levin <zjlevin@hotmail.com>
         for Traditional Chinese (Big5) translation

     Junichi Uekawa <dancer@debian.org>
     Oleg Andrjushenko <oandr@itec.cn.ua>
     Michael Bravo <mbravo@tag-ltd.spb.ru>
     Sergey Pinaev <dfo@antex.ru>
         for Russian translation

     Guilherme M. Schroeder <slump@ieg.com.br>
     Vitor Fernandes <vitor_fernandes@SoftHome.net>
     Herval Ribeiro de Azevêdo <heraze@gmail.com>
         for Brazilian Portuguese translation

     Shingo Akagaki <akagaki@ece.numazu-ct.ac.jp>
     Yuuki Ninomiya <gm@debian.or.jp>
     Sato Satoru <ss@gnome.gr.jp>
     Takeshi AIHANA <aihana@gnome.gr.jp>
         for Japanese translation

     Matthias Warkus <mawarkus@t-online.de>
     Thomas Klausner <wiz@danbala.ifoer.tuwien.ac.at>
     Danny Milosavljevic <danny_milo@yahoo.com>
     Ronny Steiner <Post@SIRSteiner.de>
     Klaus Ethgen <Klaus@Ethgen.de>
         for German translation

     Matej Erman <matej.erman@guest.arnes.si>
         for Slovene translation

     MÃtyÃs Tibor <templar@tempi.scene.hu>
     Koblinger Egmont <egmont@uhulinux.hu>
     Sári Gábor <saga@externet.hu>
         for Hungarian translation

     Wu Yulun <migr@operamail.com>
     Charles Wang <charlesw1234cn@yahoo.com.cn>
         for simplified Chinese translation

     H.J.Visser <H.J.Visser@harrie.mine.nu>
     Tino Meinen <a.t.meinen@chello.nl>
         for Dutch translation

     Lauri Nurmi <lanurmi@iki.fi>
         for Finnish translation

     Ilmar Kerm <ikerm@hot.ee>
         for Estonian translation

     Volodymyr M. Lisivka <lvm@mystery.lviv.net>
         for Ukrainian translation

     Birger Langkjer <birger.langkjer@image.dk>
         for Danish translation

     Torgeir Ness Sundli <torgeir@mp3bil.no>
         for Norwegian translation

     Jan Raska <jan.raska@tiscali.cz>
     Michal Bukovjan <bukm@centrum.cz>
         for Czech translation

     Phanumas Thanyaboon <maskung@hotmail.com>
         for Thai translation

     Harald Ersch <hersch@romatsa.ro>
         for Romanian translation

     pclouds <pclouds@vnlinux.org>
         for Vietnamese translation

     Tedi Heriyanto <tedi_h@gmx.net>
         for Indonesian translation

     Vladimir Petrov <vladux@mail.bg>
         for Bulgarian translation

     Hans Öfverbeck <hans.ofverbeck@home.se>
         for Swedish translation

     Youssef Assad <youssef@devcabal.org>
         for Arabic translation

     catux.org <mecatxis@ya.com>
         for Catalan translation

     Hyun-Jin Moon <moonhyunjin@gmail.com
         for Korean translation

     Pavel Piatruk <berserker@neolocation.com>
         for Belarusian translation

     Piarres Beobide <pi@beobide.net>
         for Basque translation

     Antonio C. Codazzi <f_sophia@libero.it>
         for Esperanto translation

     Nikos Papadopoulos
	 for Greek translation

 Fixes, additions, and patches:

     Eric Swalens
     Quy Tonthat
         for initial Exif support.

     Daniel M. German <dmgerman at uvic.ca>
         for Canon raw image support.

     Lars Ellenberg
         for Fujifilm raw image support.

     Diederen Damien <D.Diederen@student.ulg.ac.be>
         for .xvpics thumbnail reading support.

     Nick Rusnov <nick@grawk.net>
     Ryan Murray <rmurray@debian.org>
         for man page.

     Martin Pool <mbp@samba.org>
         for sort by number, misc. improvements.

     Jörg Mensmann <joerg.mensmann@gmx.de>
         for Xinerama support patch.

     Vadim Shliakhov
         for miscellaneous fixes.

     Uwe Ohse
         for Exif enhancements, histogram and other patches

     Timo Horstschäfer
         for customizable fullscreen overlay

     Michael Mokeev
         for print related enhancements

     Carles Pina i Estany
         for copy path to clipboard patch

     Kjell Morgenstern
         for random slide show speedup patch

     And...
         Thanks to the users for finding Geeqie useful.
