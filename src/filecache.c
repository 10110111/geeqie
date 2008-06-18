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


#include "main.h"
#include "filecache.h"

/* this implements a simple LRU algorithm */

struct _FileCacheData {
	FileCacheReleaseFunc release;
	GList *list;
	gulong max_size;
	gulong size;
	};


FileCacheData *file_cache_new(FileCacheReleaseFunc release, gulong max_size)
{
	FileCacheData *fc = g_new(FileCacheData, 1);
	fc->release = release;
	fc->list = NULL;
	fc->max_size = max_size;
	fc->size = 0;
	return fc;
}

gint file_cache_get(FileCacheData *fc, FileData *fd)
{
	GList *work;
	if ((work = g_list_find(fc->list, fd)))
		{
		fc->list = g_list_remove_link(fc->list, work);
		fc->list = g_list_concat(work, fc->list);
		DEBUG_1("cache hit: %s", fd->path);
		return TRUE;
		}
	DEBUG_1("cache miss: %s", fd->path);
	return FALSE;
}

void file_cache_put(FileCacheData *fc, FileData *fd, gulong size)
{
	GList *work;
	FileData *last_fd;
	if ((work = g_list_find(fc->list, fd)))
		{ 
		/* entry already exists, move it to the beginning */
		fc->list = g_list_remove_link(fc->list, work);
		fc->list = g_list_concat(work, fc->list);
		return;
		}
	
	DEBUG_1("cache add: %s", fd->path);
	file_data_ref(fd);
	fc->list = g_list_prepend(fc->list, fd);
	fc->size++; /* FIXME: use size */
	
	if (fc->size < fc->max_size) return;
	
	fc->size--;
	work = g_list_last(fc->list);
	last_fd = work->data;
	fc->list = g_list_delete_link(fc->list, work);
	DEBUG_1("cache remove: %s", last_fd->path);
	fc->release(last_fd);
	file_data_unref(last_fd);
}
