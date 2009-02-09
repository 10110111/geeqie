#!/bin/bash

# Script to update ChangeLog file,
# it keeps "pre-svn" history and insert svn log at top,
# it uses C locale for date format.
# It has to be run where ChangeLog is.
# Old ChangeLog is saved as ChangeLog.bak

[ ! -e "ChangeLog" ] && exit 1

tail -6614 ChangeLog > ChangeLog.$$.old && \
LC_ALL=C svn log -rHEAD:220 > ChangeLog.$$.new && \
cat ChangeLog.$$.old >> ChangeLog.$$.new && \
mv -f ChangeLog ChangeLog.bak && \
mv -f ChangeLog.$$.new ChangeLog

rm -f ChangeLog.$$.old
exit 0
