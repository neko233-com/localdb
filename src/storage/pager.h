#ifndef LOCALDB_PAGER_H
#define LOCALDB_PAGER_H

#include "localdb.h"
#include <stdio.h>

typedef struct localdb_page {
    uint32_t page_no;
    uint8_t *data;
    int      dirty;
    int      pinned;
    struct localdb_page *next; /* LRU chain */
    struct localdb_page *prev;
} localdb_page;

struct localdb_pager {
    FILE     *fd;
    char     *path;
    uint32_t  page_size;    /* default 4096 */
    uint32_t  page_count;
    uint32_t  cache_size;   /* max cached pages */
    uint32_t  cache_used;
    localdb_page *cache_head; /* LRU list head (most recent) */
    localdb_page *cache_tail; /* LRU list tail (least recent) */
    int       flags;
};

int  localdb_pager_open(const char *path, int flags, localdb_pager **out);
void localdb_pager_close(localdb_pager *pager);
int  localdb_pager_read(localdb_pager *pager, uint32_t page_no, localdb_page **out);
int  localdb_pager_write(localdb_pager *pager, localdb_page *page);
int  localdb_pager_alloc(localdb_pager *pager, localdb_page **out);
int  localdb_pager_flush(localdb_pager *pager);
uint32_t localdb_pager_page_size(localdb_pager *pager);
uint32_t localdb_pager_page_count(localdb_pager *pager);

#endif
