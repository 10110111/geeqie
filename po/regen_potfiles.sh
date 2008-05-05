#!/bin/sh

#generate a patch to update POTFILES.in
#Use like this: ./regen_potfiles.sh | patch -p0
(cd .. ; grep -l 'N\?_[[:space:]]*(.*)' ./src/*.c) > POTFILES.in.$$
diff -u POTFILES.in POTFILES.in.$$
rm -f POTFILES.in.$$
