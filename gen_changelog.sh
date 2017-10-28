#!/bin/bash

# Script to update ChangeLog file,
# it keeps "pre-svn" history and inserts git log at top,
# it uses C locale for date format.
# It has to be run where ChangeLog is.
# Old ChangeLog is saved as ChangeLog.bak
#
# ChangeLog.html is also created

[ ! -e "ChangeLog" ] && exit 1
[ ! -x "$(command -v git)" ] && exit 0

tail -6614 ChangeLog > ChangeLog.$$.old && \
LC_ALL=C git log --no-merges --no-notes --encoding=UTF-8 --no-follow 1b58572cf58e9d2d4a0305108395dab5c66d3a09..HEAD > ChangeLog.$$.new && \
cat ChangeLog.$$.old >> ChangeLog.$$.new && \
mv -f ChangeLog ChangeLog.bak && \
mv -f ChangeLog.$$.new ChangeLog

rm -f ChangeLog.$$.old


echo "<textarea rows='6615' cols='100'" >ChangeLog.$$.old.html && \
tail -6618 ChangeLog >> ChangeLog.$$.old.html && \
echo "</textarea" >>ChangeLog.$$.old.html && \
LC_ALL=C git log --no-merges --no-notes --encoding=UTF-8 --date=format:'%Y-%m-%d' --no-follow  --pretty=format:"<li><a href=\"http://github.com/BestImageViewer/Geeqie/commit/%H\">view commit </a></li> <para>Author: %an<br>Date: %ad<br><textarea rows=4 cols=100>%s %n%n%b</textarea><br><br></para>" > ChangeLog.$$.new.html && \
cat ChangeLog.$$.old.html >> ChangeLog.$$.new.html
[ -e ChangeLog.html ] && mv -f ChangeLog.html ChangeLog.html.bak
mv -f ChangeLog.$$.new.html ChangeLog.html

rm -f ChangeLog.$$.old.html

exit 0
