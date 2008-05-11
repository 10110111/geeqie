#!/bin/bash

# This script prints translations statistics for .po files
# existing in the current directory
export LC_ALL=C

echo "Translations statistics"
echo "Date: "$(date -R)
echo

echo "Note: completion % in the chart below may not be quite correct"
echo "      when fuzzy translations exist but do not appear in the source."
echo "      For exact results, run make update-po with up to date POTFILES.in."
echo "      comp % = trans / (trans + fuzzy + untrans)"
echo

(echo "Language  Comp(%) Trans Fuzzy Untrans Total"; \
for i in *.po; do
	msgfmt --statistics -o /dev/null $i 2>&1 \
	| sed 's/^\([0-9]\+ \)[^0-9]*\([0-9]\+ \)\?[^0-9]*\([0-9]\+ \)\?[^0-9]*$/\1\2\3/g' \
	| awk '{ \
		tot = $1 + $2 + $3; \
		if (tot != 0) \
			printf "%8.0f|%s|%7.2f|%5d|%5d|%7d|%5d\n",\
			($1*100/tot)*100, "'"${i%%.po}"'", $1*100/tot, tot-($2+$3), $2, $3, tot}' ;
done | sort -t '|' -b -k1,1nr -k2,2 | sed 's/^ *[0-9]*//' | tr ' |' '| '
) | column -t -c 80 | tr '|' ' '
echo

