#!/bin/sh

unset LANG
PAGES=`curl "http://sourceforge.net/apps/trac/geeqie/wiki/TitleIndex" | \
     sed -e "s|>|>\n|g" |grep 'href=.*/geeqie/wiki/Guide'|sed -e 's|.*/wiki/Guide\([a-zA-Z0-9]*\).*|Guide\1|'`

mkdir wiki

for p in $PAGES ; do
  curl "http://sourceforge.net/apps/trac/geeqie/wiki/$p?format=txt" > wiki/$p
done

     
     