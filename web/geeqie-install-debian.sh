#!/bin/bash
version="2018-08-02"
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
-c --commit=ID Checkout and compile commit ID
-t --tag=TAG Checkout and compile TAG (e.g. v1.4 or v1.3)
-b --back=N Checkout commit -N (e.g. "-b 1" for last-but-one commit)
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

####################################################################
# Get System Info
# Derived from: https://github.com/coto/server-easy-install (GPL)
####################################################################
lowercase()
{
	echo "$1" | sed "y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/"
}

systemProfile()
{
	OS=`lowercase \`uname\``
	KERNEL=`uname -r`
	MACH=`uname -m`

	if [ "${OS}" == "windowsnt" ]
	then
		OS=windows
	elif [ "${OS}" == "darwin" ]
	then
		OS=mac
	else
		OS=`uname`
		if [ "${OS}" = "SunOS" ]
		then
			OS=Solaris
			ARCH=`uname -p`
			OSSTR="${OS} ${REV}(${ARCH} `uname -v`)"
		elif [ "${OS}" = "AIX" ]
		then
			OSSTR="${OS} `oslevel` (`oslevel -r`)"
		elif [ "${OS}" = "Linux" ]
		then
			if [ -f /etc/redhat-release ]
			then
				DistroBasedOn='RedHat'
				DIST=`cat /etc/redhat-release |sed s/\ release.*//`
				PSUEDONAME=`cat /etc/redhat-release | sed s/.*\(// | sed s/\)//`
				REV=`cat /etc/redhat-release | sed s/.*release\ // | sed s/\ .*//`
			elif [ -f /etc/SuSE-release ]
			then
				DistroBasedOn='SuSe'
				PSUEDONAME=`cat /etc/SuSE-release | tr "\n" ' '| sed s/VERSION.*//`
				REV=`cat /etc/SuSE-release | tr "\n" ' ' | sed s/.*=\ //`
			elif [ -f /etc/mandrake-release ]
			then
				DistroBasedOn='Mandrake'
				PSUEDONAME=`cat /etc/mandrake-release | sed s/.*\(// | sed s/\)//`
				REV=`cat /etc/mandrake-release | sed s/.*release\ // | sed s/\ .*//`
			elif [ -f /etc/debian_version ]
			then
				DistroBasedOn='Debian'
				if [ -f /etc/lsb-release ]
				then
					DIST=`cat /etc/lsb-release | grep '^DISTRIB_ID' | awk -F=  '{ print $2 }'`
					PSUEDONAME=`cat /etc/lsb-release | grep '^DISTRIB_CODENAME' | awk -F=  '{ print $2 }'`
					REV=`cat /etc/lsb-release | grep '^DISTRIB_RELEASE' | awk -F=  '{ print $2 }'`
				fi
			fi
			if [ -f /etc/UnitedLinux-release ]
			then
				DIST="${DIST}[`cat /etc/UnitedLinux-release | tr "\n" ' ' | sed s/VERSION.*//`]"
			fi
			OS=`lowercase $OS`
			DistroBasedOn=`lowercase $DistroBasedOn`
			readonly OS
			readonly DIST
			readonly DistroBasedOn
			readonly PSUEDONAME
			readonly REV
			readonly KERNEL
			readonly MACH
		fi
	fi
}

install_essential()
{
arraylength=${#essential_array[@]}
for (( i=0; i<${arraylength}; i=i+1 ));
do
	package_query ${essential_array[$i]}
	if [ $? != 0 ]
	then
		package_install ${essential_array[$i]}
	fi
done

if [[ $1 == "GTK3" ]]
then
	package_query "libgtk-3-dev"
	if [ $? != 0 ]
	then
		package_install libgtk-3-dev
	fi
else
	package_query "libgtk2.0-dev"
	if [ $? != 0 ]
	then
		package_install libgtk2.0-dev
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
		package_install $1
		shift
	done
	IFS=$OLDIFS
fi
}

install_webp()
{
rm -rf webp-pixbuf-loader-master
package_install libglib2.0-dev libgdk-pixbuf2.0-dev libwebp-dev python-minimal
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
package_install libbz2-dev
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

package_query()
{
if [[ $DistroBasedOn == "debian" ]]
then
	res=$(dpkg-query --show --showformat='${Status}' $1  2>&1)
	if [[ "$res" == "install ok installed"* ]]
	then
		status=0
	else
		status=1
	fi
fi
return $status
}

package_install()
{
if [[ $DistroBasedOn == "debian" ]]
then
	sudo apt-get --assume-yes install $@
fi
}

# Entry point

# Check system type
systemProfile
if [[ $DistroBasedOn != "debian" ]]
then
	zenity --error --title="Install Geeqie and dependencies" --width=370 --text="Unknown operating system:\n
Operating System: $OS
Distribution: $DIST
Psuedoname: $PSUEDONAME
Revision: $REV
DistroBasedOn: $DistroBasedOn
Kernel: $KERNEL
Machine: $MACH" 2>/dev/null

	exit
fi

# Parse the comand line
OPTS=$(getopt -o vhc:t:b: --long version,help,commit:,tag:,back: -- "$@")
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
	-b | --back )
		BACK="$2"
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

# Use GTK3 as default
gk2_installed=FALSE
gtk3_installed=TRUE

if [[ $mode == "install" ]]
then
	message="This script is for use on Ubuntu and other\nDebian-based installations.\nIt will download, compile, and install Geeqie source\ncode and its dependencies.\n\nA sub-folder named \"geeqie\" will be created in the\nfolder this script is run from, and the source code\nwill be downloaded to that sub-folder.\n\nIn this dialog you must select whether to compile\nfor GTK2 or GTK3.\nIf you want to use GPS maps or pdf preview,\nyou must choose GTK3.\nThe GTK2 version has a slightly different\nlook-and-feel compared to the GTK3 version,\nbut otherwise has the same features.\nYou may easily switch between the two after\ninstallation.\n\nIn subsequent dialogs you may choose which\noptional features to install."

	title="Install Geeqie and dependencies"
	install_option=TRUE
else
	message="This script is for use on Ubuntu and other\nDebian-based installations.\nIt will update the Geeqie source code and its\ndependencies, and will compile and install Geeqie.\n\nYou may also switch the installed version from\nGTK2 to GTK3 and vice versa.\n\nIn this dialog you must select whether to compile\nfor GTK2 or GTK3.\nIf you want to use GPS maps or pdf preview,\nyou must choose GTK3.\nThe GTK2 version has a slightly different\nlook-and-feel compared to the GTK3 version,\nbut otherwise has the same features.\n\nIn subsequent dialogs you may choose which\noptional features to install."

	title="Update Geeqie and re-install"
	install_option=FALSE

	# When updating, use previous installation as default
	if [[ -f config.log ]]
	then
		grep gtk-2.0 config.log >/dev/null
		if [[ $? != 0 ]]
		then
			gtk2_installed=FALSE
			gtk3_installed=TRUE
		else
			gtk2_installed=TRUE
			gtk3_installed=FALSE
		fi
	fi
fi

# Ask whether to install GTK2 or GTK3 or uninstall

gtk_version=$(zenity --title="$title" --width=370 --text="$message" --list --radiolist --column "" --column "" "$gtk3_installed" "GTK3 (required for GPS maps and pdf preview)" "$gtk2_installed" "GTK2" FALSE "Uninstall" --cancel-label="Cancel" --ok-label="OK" --hide-header 2>/dev/null)

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
	package_query ${optional_array[$i+1]}
	if [ $? != 0 ]
	then
		if [ -z "$option_string" ]
		then
			option_string="$install_option"$'\n'"${optional_array[$i]}"$'\n'"${optional_array[$i+1]}"
		else
			option_string="$option_string"$'\n'"$install_option"$'\n'"${optional_array[$i]}"$'\n'"${optional_array[$i+1]}"
		fi
	fi
done

# If GTK3 required, get the GTK3 options not yet installed
if [[ "$gtk_version" == "GTK3"* ]]
then
	arraylength=${#optional_gtk3_array[@]}
	for (( i=0; i<${arraylength}; i=i+2 ));
	do
		package_query ${optional_gtk3_array[$i+1]}
		if [ $? != 0 ]
		then
			if [ -z "$option_string" ]
			then
				option_string="$install_option"$'\n'"${optional_gtk3_array[$i]}"$'\n'"${optional_gtk3_array[$i+1]}"
			else
				option_string="$option_string"$'\n'"$install_option"$'\n'"${optional_gtk3_array[$i]}"$'\n'"${optional_gtk3_array[$i+1]}"
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
		loaders_string=$'FALSE\n'"${optional_loaders_array[$i]}"$'\n'"${optional_loaders_array[$i+1]}"
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
	options=$(echo "$option_string" | zenity --title="$title" --width=400 --height=500 --list --checklist --text 'Select which library files to install:' --column='Select' --column='Library files' --column='Library' --hide-column=3 --print-column=3 2>/dev/null)

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
	ret=$(git clone git://www.geeqie.org/geeqie.git  2>&1 >/dev/null)
else
	git checkout master
	if [[ $? != 0 ]]
	then
		zenity --title="$title" --width=370 --height=400 --error --text="Git checkout master error"  2>/dev/null
		exit
	fi
	ret=$(git pull 2>&1 >/dev/null)
fi

if [[ $? != 0 ]]
then
	zenity --title="$title" --width=370 --height=400 --error --text="Git error:\n\n $ret" 2>/dev/null
	exit
fi

if [[ $mode == "install" ]]
then
	cd geeqie
else
	sudo make uninstall
	sudo make maintainer-clean
fi

if [[ "$BACK" ]]
then
	ret=$(git checkout master~"$BACK" 2>&1 >/dev/null)
	if [[ $1 != 0 ]]
	then
		zenity --title="$title" --width=370 --height=400 --error --text="Git error:\n\n $ret" 2>/dev/null
		exit
	fi
elif [[ "$COMMIT" ]]
then
	ret=$(git checkout "$COMMIT" 2>&1 >/dev/null)
	if [[ $1 != 0 ]]
	then
		zenity --title="$title" --width=370 --height=400 --error --text="Git error:\n\n $ret" 2>/dev/null
		exit
	fi
elif [[ "$TAG" ]]
then
	ret=$(git checkout "$TAG" 2>&1 >/dev/null)
	if [[ $1 != 0 ]]
	then
		zenity --title="$title" --width=370 --height=400 --error --text="Git error:\n\n $ret" 2>/dev/null
		exit
	fi
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
