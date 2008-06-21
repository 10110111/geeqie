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

typedef struct _FileCacheEntry FileCacheEntry;
struct _FileCacheEntry {
	FileData *fd;
	gulong size;
	};

static gint file_cache_entry_compare_cb(gconstpointer a, gconstpointer b)
{
	const FileCacheEntry *fca = a;
	const FileData *fd = b;
	if (fca->fd == fd) return 0;
	return 1;
}


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
	if ((work = g_list_find_custom(fc->list, fd, file_cache_entry_compare_cb)))
		{
		fc->list = g_list_remove_link(fc->list, work);
		fc->list = g_list_concat(work, fc->list);
		DEBUG_1("cache hit: %s", fd->path);
		return TRUE;
		}
	DEBUG_1("cache miss: %s", fd->path);
	return FALSE;
}

void file_cache_set_size(FileCacheData *fc, gulong size)
{
	GList *work;
	FileCacheEntry *last_fe;
	work = g_list_last(fc->list);
	while (fc->size > size && work)
		{
		GList *prev;
		last_fe = work->data;
		prev = work->prev; 
		fc->list = g_list_delete_link(fc->list, work);
		work = prev;
		
		DEBUG_1("cache remove: %s", last_fe->fd->path);
		fc->size -= last_fe->size;
		fc->release(last_fe->fd);
		file_data_unref(last_fe->fd);
		g_free(last_fe);
		}
}

void file_cache_put(FileCacheData *fc, FileData *fd, gulong size)
{
	GList *work;
	FileCacheEntry *fe;
	if ((work = g_list_find_custom(fc->list, fd, file_cache_entry_compare_cb)))
		{ 
		/* entry already exists, move it to the beginning */
		fc->list = g_list_remove_link(fc->list, work);
		fc->list = g_list_concat(work, fc->list);
		return;
		}
	
	DEBUG_1("cache add: %s", fd->path);
	fe = g_new(FileCacheEntry, 1);
	fe->fd = file_data_ref(fd);
	fe->size = size;
	fc->list = g_list_prepend(fc->list, fe);
	fc->size += size;
	
	file_cache_set_size(fc, fc->max_size);
}

gulong file_cache_get_max_size(FileCacheData *fc)
{
	return fc->max_size;
}

gulong file_cache_get_size(FileCacheData *fc)
{
	return fc->size;
}

void file_cache_set_max_size(FileCacheData *fc, gulong size)
{
	fc->max_size = size;
	file_cache_set_size(fc, fc->max_size);
}

void file_cache_dump(FileCacheData *fc)
{
	GList *work;
	work = fc->list;
	
	DEBUG_1("cache dump: max size:%ld size:%ld", fc->max_size, fc->size);
		
	while(work)
		{
		FileCacheEntry *fe = work->data;
		work = work->next;
		DEBUG_1("cache entry: %s %ld", fe->fd->path, fe->size);
		}
}
