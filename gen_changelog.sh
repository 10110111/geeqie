#!/bin/bash

# Script to update ChangeLog file,
# it keeps "pre-svn" history and insert svn log at top,
# it uses C locale for date format.
# It has to be run where ChangeLog is.
# Old ChangeLog is saved as ChangeLog.bak

[ ! -e "ChangeLog" ] && exit 1

tail -6614 ChangeLog > ChangeLog.$$.old && \
LC_ALL=C git log --no-merges --no-notes --encoding=UTF-8 --no-follow 1b58572cf58e9d2d4a0305108395dab5c66d3a09..HEAD > ChangeLog.$$.new && \
cat ChangeLog.$$.old >> ChangeLog.$$.new && \
mv -f ChangeLog ChangeLog.bak && \
mv -f ChangeLog.$$.new ChangeLog

rm -f ChangeLog.$$.old
exit 0
