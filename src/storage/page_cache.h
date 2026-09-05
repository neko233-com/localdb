#ifndef LOCALDB_PAGE_CACHE_H
#define LOCALDB_PAGE_CACHE_H

#include "pager.h"

/* Page cache is integrated into the pager (LRU cache).
 * This header provides additional cache management functions. */

int  localdb_cache_resize(localdb_pager *pager, uint32_t max_pages);
void localdb_cache_clear(localdb_pager *pager);
int  localdb_cache_hit_count(localdb_pager *pager);
int  localdb_cache_miss_count(localdb_pager *pager);

#endif
