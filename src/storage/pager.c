#include "pager.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PAGE_SIZE  4096
#define DEFAULT_CACHE_SIZE 64

int localdb_pager_open(const char *path, int flags, localdb_pager **out) {
    if (!out) return LOCALDB_ERROR;
    *out = NULL;

    localdb_pager *p = (localdb_pager *)calloc(1, sizeof(localdb_pager));
    if (!p) return LOCALDB_ERROR_NOMEM;

    p->page_size = DEFAULT_PAGE_SIZE;
    p->cache_size = DEFAULT_CACHE_SIZE;
    p->flags = flags;

    if (!(flags & LOCALDB_OPEN_MEMORY)) {
        const char *mode = (flags & LOCALDB_OPEN_READONLY) ? "rb" : "r+b";
        p->fd = fopen(path, mode);
        if (!p->fd && (flags & LOCALDB_OPEN_CREATE)) {
            p->fd = fopen(path, "w+b");
        }
        if (!p->fd) {
            free(p);
            return LOCALDB_ERROR_IO;
        }
        p->path = strdup(path);

        /* Get file size to compute page count */
        fseek(p->fd, 0, SEEK_END);
        long size = ftell(p->fd);
        fseek(p->fd, 0, SEEK_SET);
        p->page_count = (size > 0) ? (uint32_t)(size / p->page_size) : 0;
        if (p->page_count == 0) p->page_count = 1; /* at least header page */
    }

    *out = p;
    return LOCALDB_OK;
}

void localdb_pager_close(localdb_pager *pager) {
    if (!pager) return;
    localdb_pager_flush(pager);

    /* Free all cached pages */
    localdb_page *p = pager->cache_head;
    while (p) {
        localdb_page *next = p->next;
        free(p->data);
        free(p);
        p = next;
    }

    if (pager->fd) fclose(pager->fd);
    free(pager->path);
    free(pager);
}

int localdb_pager_read(localdb_pager *pager, uint32_t page_no, localdb_page **out) {
    if (!pager || !out) return LOCALDB_ERROR;
    *out = NULL;

    /* Check cache first */
    for (localdb_page *p = pager->cache_head; p; p = p->next) {
        if (p->page_no == page_no) {
            /* Move to head (MRU) */
            if (p != pager->cache_head) {
                if (p->prev) p->prev->next = p->next;
                if (p->next) p->next->prev = p->prev;
                if (p == pager->cache_tail) pager->cache_tail = p->prev;
                p->prev = NULL;
                p->next = pager->cache_head;
                if (pager->cache_head) pager->cache_head->prev = p;
                pager->cache_head = p;
            }
            *out = p;
            return LOCALDB_OK;
        }
    }

    /* Cache miss — read from file */
    localdb_page *page = (localdb_page *)calloc(1, sizeof(localdb_page));
    if (!page) return LOCALDB_ERROR_NOMEM;

    page->data = (uint8_t *)calloc(1, pager->page_size);
    if (!page->data) { free(page); return LOCALDB_ERROR_NOMEM; }

    page->page_no = page_no;

    if (pager->fd) {
        fseek(pager->fd, (long)page_no * pager->page_size, SEEK_SET);
        size_t read = fread(page->data, 1, pager->page_size, pager->fd);
        (void)read;
    }

    /* Evict LRU if cache is full */
    if (pager->cache_used >= pager->cache_size && pager->cache_tail) {
        localdb_page *evict = pager->cache_tail;
        if (evict->dirty && pager->fd) {
            fseek(pager->fd, (long)evict->page_no * pager->page_size, SEEK_SET);
            fwrite(evict->data, 1, pager->page_size, pager->fd);
        }
        pager->cache_tail = evict->prev;
        if (pager->cache_tail) pager->cache_tail->next = NULL;
        if (evict == pager->cache_head) pager->cache_head = NULL;
        free(evict->data);
        free(evict);
        pager->cache_used--;
    }

    /* Insert at head */
    page->next = pager->cache_head;
    page->prev = NULL;
    if (pager->cache_head) pager->cache_head->prev = page;
    pager->cache_head = page;
    if (!pager->cache_tail) pager->cache_tail = page;
    pager->cache_used++;

    *out = page;
    return LOCALDB_OK;
}

int localdb_pager_write(localdb_pager *pager, localdb_page *page) {
    if (!pager || !page) return LOCALDB_ERROR;
    page->dirty = 1;
    return LOCALDB_OK;
}

int localdb_pager_alloc(localdb_pager *pager, localdb_page **out) {
    if (!pager || !out) return LOCALDB_ERROR;
    pager->page_count++;
    return localdb_pager_read(pager, pager->page_count - 1, out);
}

int localdb_pager_flush(localdb_pager *pager) {
    if (!pager || !pager->fd) return LOCALDB_OK;
    for (localdb_page *p = pager->cache_head; p; p = p->next) {
        if (p->dirty) {
            fseek(pager->fd, (long)p->page_no * pager->page_size, SEEK_SET);
            fwrite(p->data, 1, pager->page_size, pager->fd);
            p->dirty = 0;
        }
    }
    fflush(pager->fd);
    return LOCALDB_OK;
}

uint32_t localdb_pager_page_size(localdb_pager *pager) {
    return pager ? pager->page_size : 0;
}

uint32_t localdb_pager_page_count(localdb_pager *pager) {
    return pager ? pager->page_count : 0;
}
