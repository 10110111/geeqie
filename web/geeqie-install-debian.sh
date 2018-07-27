#!/bin/bash
version="2018-07-26"
description=$'
Geeqie is an image viewer.
This script will download, compile, and install Geeqie on Debian-based systems.
If run from a folder that already contains the Geeqie sources, the source
code will be updated from the repository.
Dialogs allow the user to install additional features, including
additional pixbuf loaders.

Command line options are:
-v --version The version of this file
-h --help Output this text
-c --commit Checkout and compile commit ident
-t --tag Checkout and compile tag (e.g. v1.4 or v1.3)
'

# Essential for compiling
essential_array=(
"git"
"build-essential"
"autoconf"
"libglib2.0-0"
"intltool"
)

# Optional for both GTK2 and GTK3
optional_array=(
"LCMS (for color management)"
"liblcms2-dev"
"exiv2 (for exif handling)"
"libgexiv2-dev"
"lua (for --remote commands)"
"liblua5.1-0-dev"
"libffmpegthumbnailer (for mpeg thumbnails)"
"libffmpegthumbnailer-dev"
"libtiff (for tiff support)"
"libtiff-dev"
"libjpeg (for jpeg support"
"libjpeg-dev"
"librsvg2 (for viewing .svg images"
"librsvg2-common"
"libwmf (for viewing .wmf images)"
"libwmf0.2-7-gtk"
"exiftran (for image rotation)"
"exiftran"
"imagemagick (for image rotation)"
"imagemagick"
"ufraw (for RAW file handling)"
"ufraw"
"markdown (for generating README help file)"
"markdown"
)

# Optional for GTK3 only
optional_gtk3_array=(
"libchamplain gtk (for GPS maps)"
"libchamplain-gtk-0.12-dev"
"libchamplain (for GPS maps)"
"libchamplain-0.12-dev"
"libpoppler (for pdf file preview)"
"libpoppler-glib-dev"
)

# Optional pixbuf loaders
optional_loaders_array=(
".webp WebP images"
"webp"
".psd Photoshop images"
"psd"
".xcf Gimp files"
"xcf"
)

install_essential()
{
arraylength=${#essential_array[@]}
for (( i=0; i<${arraylength}; i=i+1 ));
do
	res=$(dpkg-query --show --showformat='${Status}' ${essential_array[$i]} 2>&1)
	if [[ $res != "install ok installed"* ]]
	then
		sudo apt-get --assume-yes install ${essential_array[$i]}
	fi
done

if [[ $1 == "GTK3" ]]
then
	res=$(dpkg-query --show --showformat='${Status}' "libgtk-3-dev" 2>&1)
	if [[ $res != "install ok installed"* ]]
	then
		sudo apt-get --assume-yes install libgtk-3-dev
	fi
else
	res=$(dpkg-query --show --showformat='${Status}' "libgtk2.0-dev" 2>&1)
	if [[ $res != "install ok installed"* ]]
	then
		sudo apt-get --assume-yes install libgtk2.0-dev
	fi
fi
}

install_options()
{
if [ -n "$options" ]
then
	OLDIFS=$IFS
	IFS='|'
	set $options
	while [ $# -gt 0 ];
	do
		sudo apt-get --assume-yes install $1
		shift
	done
	IFS=$OLDIFS
fi
return
}

install_webp()
{
rm -rf webp-pixbuf-loader-master
sudo apt-get --assume-yes install libglib2.0-dev libgdk-pixbuf2.0-dev libwebp-dev
wget https://github.com/aruiz/webp-pixbuf-loader/archive/master.zip
unzip master.zip
cd webp-pixbuf-loader-master
./waf configure
./waf build
sudo ./waf install
sudo gdk-pixbuf-query-loaders --update-cache
cd -
rm -rf webp-pixbuf-loader-master
rm master.zip
}

install_psd()
{
rm -rf gdk-pixbuf-psd
git clone https://github.com/and-rom/gdk-pixbuf-psd.git
cd gdk-pixbuf-psd
./autogen.sh
make
sudo make install
sudo gdk-pixbuf-query-loaders --update-cache
cd -
rm -rf gdk-pixbuf-psd
}

install_xcf()
{
rm -rf xcf-pixbuf-loader
git clone https://github.com/StephaneDelcroix/xcf-pixbuf-loader.git
cd xcf-pixbuf-loader
./autogen.sh
make

# There must be a better way...
loader_locn=$(gdk-pixbuf-query-loaders | grep "LoaderDir"  | tr -d '#[:space:]')

OLDIFS=$IFS
IFS='='
set $loader_locn
OLDIFS=$IFS

if [ -d $2 ]
then
	sudo cp .libs/libioxcf.so $2
	sudo gdk-pixbuf-query-loaders --update-cache
fi
cd -
rm -rf  xcf-pixbuf-loader
}

install_extra_loaders()
{
if [ -n "$extra_loaders" ]
then
	OLDIFS=$IFS
	IFS='|'
	set $extra_loaders
	while [ $# -gt 0 ];
	do
		case $1 in
		"webp" )
			install_webp
		;;
		"psd" )
			install_psd
		;;
		"xcf" )
			install_xcf
		;;
		esac

		shift
	done
	IFS=$OLDIFS
fi
return
}

uninstall()
{
current_dir=$(basename $PWD)
if [[ $current_dir == "geeqie" ]]
then
	sudo make uninstall
	zenity --title="Uninstall Geeqie" --width=370 --text="WARNING.\nThis will delete folder:\n\n$PWD\n\nand all sub-folders!" --question --ok-label="Cancel" --cancel-label="OK"  2>/dev/null

	if [[ $? == 1 ]]
	then
		cd ..
		sudo rm -rf geeqie
	fi
else
	zenity --title="Uninstall Geeqie" --width=370 --text="This is not a geeqie installation folder!\n\n$PWD" --warning  2>/dev/null
fi
exit
}


# Entry point
# Parse the comand line
OPTS=$(getopt -o vhc:t: --long version,help,commit:,tag: -- "$@")
eval set -- "$OPTS"

while true;
do
	case "$1" in
	-v | --version )
		echo "$version"
		exit
		;;
	-h | --help )
		echo "$description"
		exit
		;;
	-c | --commit )
		COMMIT="$2"
		shift
		shift
		;;
	-t | --tag )
		TAG="$2"
		shift;
		shift
		;;
	* ) break
		;;
	esac
done

# If a Geeqie folder already exists here, warn the user
if [ -d "geeqie" ]
then
	zenity --info --title="Install Geeqie and dependencies" --width=370 --text="This script is for use on Ubuntu and other\nDebian-based installations.\nIt will download, compile, and install Geeqie source\ncode and its dependencies.\n\nA sub-folder named \"geeqie\" will be created in the\nfolder this script is run from, and the source code\nwill be downloaded to that sub-folder.\n\nA sub-folder of that name already exists.\nPlease try another folder." 2>/dev/null

	exit
fi

# If it looks like a Geeqie download folder, assume an update
if [ -d ".git" ] && [ -d "src" ] && [ -f "geeqie.1" ]
then
	mode="update"
else
	# If it looks like something else is already installed here, warn the user
	if [ -d ".git" ] || [ -d "src" ]
	then
		zenity --info --title="Install Geeqie and dependencies" --width=370 --text="This script is for use on Ubuntu and other\nDebian-based installations.\nIt will download, compile, and install Geeqie source\ncode and its dependencies.\n\nIt looks like you are running this script from a folder which already has software installed.\n\nPlease try another folder." 2>/dev/null

		exit
	else
		mode="install"
	fi
fi

if [[ $mode == "install" ]]
then
	message="This script is for use on Ubuntu and other\nDebian-based installations.\nIt will download, compile, and install Geeqie source\ncode and its dependencies.\n\nA sub-folder named \"geeqie\" will be created in the\nfolder this script is run from, and the source code\nwill be downloaded to that sub-folder.\n\nIn this dialog you must select whether to compile\nfor GTK2 or GTK3.\nIf you want to use GPS maps or pdf preview,\nyou must choose GTK3.\nThe GTK2 version has a slightly different\nlook-and-feel compared to the GTK3 version,\nbut otherwise has the same features.\nYou may easily switch between the two after\ninstallation.\n\nIn subsequent dialogs you may choose which\noptional features to install."

	title="Install Geeqie and dependencies"
else
	message="This script is for use on Ubuntu and other\nDebian-based installations.\nIt will update the Geeqie source code and its\ndependencies, and will compile and install Geeqie.\n\nYou may also switch the installed version from\nGTK2 to GTK3 and vice versa.\n\nIn this dialog you must select whether to compile\nfor GTK2 or GTK3.\nIf you want to use GPS maps or pdf preview,\nyou must choose GTK3.\nThe GTK2 version has a slightly different\nlook-and-feel compared to the GTK3 version,\nbut otherwise has the same features.\n\nIn subsequent dialogs you may choose which\noptional features to install."

	title="Update Geeqie and re-install"
fi

# Ask whether to install GTK2 or GTK3 or uninstall

gtk_version=$(zenity --title="$title" --width=370 --text="$message" --list --radiolist --column "" --column "" TRUE "GTK3 (required for GPS maps and pdf preview)" FALSE "GTK2" FALSE "Uninstall" --cancel-label="Cancel" --ok-label="OK" --hide-header 2>/dev/null)

if [[ $? == 1 ]]
then
	exit
fi

if [[ $gtk_version == "Uninstall" ]]
then
	uninstall
	exit
fi

sleep 100 | zenity --title="$title" --text="Checking for installed files" --width=370 --progress --pulsate 2>/dev/null &
zen_pid=$!

# Get the standard options that are not yet installed
arraylength=${#optional_array[@]}
for (( i=0; i<${arraylength}; i=i+2 ));
do
	res=$(dpkg-query --show --showformat='${Status}' ${optional_array[$i+1]}  2>&1)
	if [[ $res != "install ok installed"* ]]
	then
		if [ -z "$option_string" ]
		then
			option_string=$'TRUE\n'"${optional_array[$i]}"$'\n'"${optional_array[$i+1]}"
		else
			option_string="$option_string"$'\nTRUE\n'"${optional_array[$i]}"$'\n'"${optional_array[$i+1]}"
		fi
	fi
done

# If GTK3 required, get the GTK3 options not yet installed
if [[ "$gtk_version" == "GTK3"* ]]
then
	arraylength=${#optional_gtk3_array[@]}
	for (( i=0; i<${arraylength}; i=i+2 ));
	do
		res=$(dpkg-query --show --showformat='${Status}' ${optional_gtk3_array[$i+1]}  2>&1)
		if [[ $res != "install ok installed"* ]]
		then
			if [ -z "$option_string" ]
			then
				option_string=$'TRUE\n'"${optional_gtk3_array[$i]}"$'\n'"${optional_gtk3_array[$i+1]}"
			else
				option_string="$option_string"$'\nTRUE\n'"${optional_gtk3_array[$i]}"$'\n'"${optional_gtk3_array[$i+1]}"
			fi
		fi
	done
fi

# Get the optional loaders not yet installed
((i=0))
gdk-pixbuf-query-loaders | grep WebP >/dev/null
if [[ $? == 1 ]]
then
	if [ -z "$loaders_string" ]
	then
		loaders_string=$'nFALSE\n'"${optional_loaders_array[$i]}"$'\n'"${optional_loaders_array[$i+1]}"
	else
		loaders_string="$loaders_string"$'\nFALSE\n'"${optional_loaders_array[$i]}"$'\n'"${optional_loaders_array[$i+1]}"
	fi
fi

((i=i+2))
gdk-pixbuf-query-loaders | grep psd >/dev/null
if [[ $? == 1 ]]
then
	if [ -z "$loaders_string" ]
	then
		loaders_string=$'FALSE\n'"${optional_loaders_array[$i]}"$'\n'"${optional_loaders_array[$i+1]}"
	else
		loaders_string="$loaders_string"$'\nFALSE\n'"${optional_loaders_array[$i]}"$'\n'"${optional_loaders_array[$i+1]}"
	fi
fi

((i=i+2))
gdk-pixbuf-query-loaders | grep xcf >/dev/null
if [[ $? == 1 ]]
then
	if [ -z "$loaders_string" ]
	then
		loaders_string=$'FALSE\n'"${optional_loaders_array[$i]}"$'\n'"${optional_loaders_array[$i+1]}"
	else
		loaders_string="$loaders_string"$'\nFALSE\n'"${optional_loaders_array[$i]}"$'\n'"${optional_loaders_array[$i+1]}"
	fi
fi

kill $zen_pid 2>/dev/null

# Ask the user which options to install
if [ -n "$option_string" ]
then
	options=$(echo "$option_string" | zenity --title="$title" --width=370 --height=400 --list --checklist --text 'Select which library files to install:' --column='Select' --column='Library files' --column='Library' --hide-column=3 --print-column=3 2>/dev/null)

	if [[ $? == 1 ]]
	then
		exit
	fi
fi

# Ask the user which extra loaders to install
if [ -n "$loaders_string" ]
then
	extra_loaders=$(echo "$loaders_string" | zenity --title="$title" --width=370 --height=400 --list --checklist --text 'These loaders are not part of the main repository,\nbut are known to work to some extent.' --column='Select' --column='Library files' --column='Library' --hide-column=3 --print-column=3 2>/dev/null)

	if [[ $? == 1 ]]
	then
		exit
	fi
fi

install_essential $gtk_version
install_options
install_extra_loaders

if [[ $mode == "install" ]]
then
	ret=$(git clone git://www.geeqie.org/geeqie.git)
else
	git checkout master
	ret=$(git pull)
fi

if [[ $? != 0 ]]
then
	echo "$ret"
	exit
fi

if [[ $mode == "install" ]]
then
	cd geeqie
else
	sudo make uninstall
	sudo make maintainer-clean
fi

if [[ "$COMMIT" ]]
then
	git checkout "$COMMIT"
fi
if [[ "TAG" ]]
then
	git checkout "$TAG"
fi

if [[ $gtk_version == "GTK3"* ]]
then
	./autogen.sh
else
	./autogen.sh --disable-gtk3
fi

make -j
sudo make install

exit


