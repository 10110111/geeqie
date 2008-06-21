/*
 * Geeqie
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: Vladimir Nadvornik
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifndef FILECACHE_H
#define FILECACHE_H

#include "main.h"
#include "filedata.h"

typedef struct _FileCacheData FileCacheData;
typedef void (*FileCacheReleaseFunc)(FileData *fd);


FileCacheData *file_cache_new(FileCacheReleaseFunc release, gulong max_size);
gint file_cache_get(FileCacheData *fc, FileData *fd);
void file_cache_put(FileCacheData *fc, FileData *fd, gulong size);
void file_cache_dump(FileCacheData *fc);

#endif
