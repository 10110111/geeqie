# This script will aid compiling Geeqie on Debian style systems.
# You should edit this script to install only those libraries you need.

#!/bin/bash
zenity --question --title="Install Geeqie dependencies" --width=300 --text="This script will aid compiling Geeqie from sources on Debian style systems\n\nYou should first edit this script to install only those libraries you need." --ok-label="Cancel" --cancel-label="Run script"

if [ $? -eq 0 ]
then
  exit
fi

# For cloning the repository
sudo apt install git

# For compiling
sudo apt-get install build-essential
sudo apt-get install autoconf
sudo apt-get install libglib2.0
sudo apt-get install intltool
# For the GTK2 version of Geeqie
sudo apt-get install libgtk2.0-dev
# For the GTK3 version of Geeqie
sudo apt-get install libgtk-3-dev

# For Little CMS
sudo apt-get install liblcms2-2
sudo apt-get install liblcms2-dev

# For exiv2
sudo apt-get install libgexiv2-2
sudo apt-get install libgexiv2-dev

# For lua
sudo apt-get install lua5.1
sudo apt-get install liblua5.1-0
sudo apt-get install liblua5.1-dev

# For the mpeg thumbnails
sudo apt-get install libffmpegthumbnailer4v5
sudo apt-get install libffmpegthumbnailer-dev

# For the GPS map feature
sudo apt-get install libchamplain-gtk-0.12-0
sudo apt-get install libchamplain-gtk-0.12-dev
sudo apt-get install libchamplain-0.12-0
sudo apt-get install libchamplain-0.12-dev

# For the preview of pdf files
sudo apt-get install libpoppler-glib-dev

# For the display of .svg images
sudo apt-get install librsvg2-common

# For the display of .wmf images
sudo apt-get install libwmf0.2-7-gtk


# Other programs which help when using Geeqie

# For image rotation
sudo apt-get install exiftran
sudo apt-get install imagemagick

# For RAW file handling
sudo apt-get install ufraw

# For generating some documentation files
sudo apt-get install markdown


# To install, create a folder in which to compile Geeqie:
# mkdir <folder>
# cd <folder>
# git clone git://www.geeqie.org/geeqie.git
# cd geeqie
# For GTK3 (including GPS maps):
# ./autogen.sh
# For GTK2:
# ./autogen.sh --disable-gtk3
# make -j
# sudo make install

# Uninstall:
#sudo make uninstall

