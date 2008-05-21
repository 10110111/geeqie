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


#ifndef CACHE_MAINT_H
#define CACHE_MAINT_H


void cache_maintain_home(gint metadata, gint clear, GtkWidget *parent);

#if 0
gint cache_maintain_home_dir(const gchar *dir, gint recursive, gint clear);
gint cache_maintain_dir(const gchar *dir, gint recursive, gint clear);
#endif

void cache_maint_moved(FileData *fd);
void cache_maint_removed(FileData *fd);
void cache_maint_copied(FileData *fd);


void cache_manager_show(void);


#endif
