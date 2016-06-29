# Convert the original keymap_template.svg file to C program file
# Reformat to use data in the style of the gtk accelerator map routines
# awk -f keymap.awk keymap_template.svg > ../src/keymap_template.c

BEGIN {
	print "/*"
	print " * Copyright (C) 2004 John Ellis"
	print " * Copyright (C) 2008 - 2016 The Geeqie Team"
	print " *"
	print " * Author: John Ellis"
	print " *"
	print " * This program is free software; you can redistribute it and/or modify"
	print " * it under the terms of the GNU General Public License as published by"
	print " * the Free Software Foundation; either version 2 of the License, or"
	print " * (at your option) any later version."
	print " *"
	print " * This program is distributed in the hope that it will be useful,"
	print " * but WITHOUT ANY WARRANTY; without even the implied warranty of"
	print " * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the"
	print " * GNU General Public License for more details."
	print " *"
	print " * You should have received a copy of the GNU General Public License along"
	print " * with this program; if not, write to the Free Software Foundation, Inc.,"
	print " * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA."
	print " */"
	print ""
	print "static char *keymap_template [] = {"
}

{
	gsub(/\"/,"\\\"")
	gsub(/&lt;control&gt;/,"\\&lt;Primary\\&gt;")
	gsub(/&lt;meta&gt;/,"\\&lt;Alt\\&gt;")
	gsub(/&lt;shift&gt;/,"\\&lt;Shift\\&gt;")
	gsub(/&lt;super&gt;/,"\\&lt;Super\\&gt;")
	gsub(/&lt;Shift&gt;&lt;Primary&gt;/,"\\&lt;Primary\\&gt;\\&lt;Shift\\&gt;")
	gsub(/&lt;Alt&gt;&lt;Shift&gt;/,"\\&lt;Shift\\&gt;\\&lt;Alt\\&gt;")
	gsub(/&lt;Alt&gt;&lt;Primary&gt;/,"\\&lt;Primary\\&gt;\\&lt;Alt\\&gt;")

	gsub(/>\\</,">\\\\<")

	gsub(/^/,"\"")
	gsub(/$/,"\",")

	keycodes[0,0]="0"
	keycodes[0,1]="parenright"
	keycodes[1,0]="1"
	keycodes[1,1]="exclam"
	keycodes[2,0]="2"
	keycodes[2,1]="quotedbl"
	keycodes[3,0]="3"
	keycodes[3,1]="sterling"
	keycodes[4,0]="4"
	keycodes[4,1]="dollar"
	keycodes[5,0]="5"
	keycodes[5,1]="percent"
	keycodes[6,0]="6"
	keycodes[6,1]="asciicircum"
	keycodes[7,0]="7"
	keycodes[7,1]="ampersand"
	keycodes[8,0]="8"
	keycodes[8,1]="asterisk"
	keycodes[9,0]="9"
	keycodes[9,1]="parenleft"
	keycodes[10,0]="minus"
	keycodes[10,1]="underscore"
	keycodes[11,0]="equal"
	keycodes[11,1]="plus"
	keycodes[12,0]="bracketleft"
	keycodes[12,1]="braceleft"
	keycodes[13,0]="bracketright"
	keycodes[13,1]="braceright"
	keycodes[14,0]="minus"
	keycodes[14,1]="underscore"
	keycodes[15,0]="semicolon"
	keycodes[15,1]="colon"
	keycodes[16,0]="apostrophe"
	keycodes[16,1]="at"
	keycodes[17,0]="numbersign"
	keycodes[17,1]="asciitilde"
	keycodes[18,0]="comma"
	keycodes[18,1]="less"
	keycodes[19,0]="period"
	keycodes[19,1]="greater"
	keycodes[20,0]="slash"
	keycodes[20,1]="question"
	keycodes[21,0]="grave"
	keycodes[21,1]="notsign"
	keycodes[22,0]="backslash"
	keycodes[22,1]="bar"

	for (i=0; i<23; i++)
		{
		gsub("Shift&gt;"keycodes[i,0],"Shift\\&gt;"keycodes[i,1])
		gsub("Primary&gt;&lt;Shift&gt;"keycodes[i,0],"Primary\\&gt;\\&lt;Shift\\&gt;"keycodes[i,1])
		gsub("Shift&gt;&lt;Alt&gt;"keycodes[i,0],"Shift\\&gt;\\&lt;Alt\\&gt;"keycodes[i,1])
		}

	print
}

END {
	print "NULL,"
	print "};"

}
