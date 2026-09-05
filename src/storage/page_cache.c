#include "page_cache.h"
#include <stdlib.h>

int localdb_cache_resize(localdb_pager *pager, uint32_t max_pages) {
    if (!pager) return LOCALDB_ERROR;
    pager->cache_size = max_pages;
    return LOCALDB_OK;
}

void localdb_cache_clear(localdb_pager *pager) {
    if (!pager) return;
    localdb_page *p = pager->cache_head;
    while (p) {
        localdb_page *next = p->next;
        if (p->dirty && pager->fd) {
            fseek(pager->fd, (long)p->page_no * pager->page_size, SEEK_SET);
            fwrite(p->data, 1, pager->page_size, pager->fd);
        }
        free(p->data);
        free(p);
        p = next;
    }
    pager->cache_head = NULL;
    pager->cache_tail = NULL;
    pager->cache_used = 0;
}
