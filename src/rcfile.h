/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#ifndef RCFILE_H
#define RCFILE_H

gchar *quoted_value(const gchar *text, const gchar **tail);
gchar *escquote_value(const gchar *text);

void save_options(ConfOptions *options);
void load_options(ConfOptions *options);


#endif
