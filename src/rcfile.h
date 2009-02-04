/*
 * Geeqie
 * (C) 2004 John Ellis
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#ifndef RCFILE_H
#define RCFILE_H

gboolean save_options_to(const gchar *utf8_path, ConfOptions *options);
gboolean load_options_from(const gchar *utf8_path, ConfOptions *options);

#endif
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
