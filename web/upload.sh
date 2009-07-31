#!/bin/sh

if [ "${PWD##*/}" != "web" ] ; then
  echo $0 must be called from directory \"web\"
  exit 1;
fi

# later we can include user manual 
# ln -sf ../doc

echo -n "SF username: "
read username

rsync -avP -C --copy-links --exclude "*.sh" --exclude "Makefile*" -e ssh ./ $username,geeqie@web.sourceforge.net:htdocs/
