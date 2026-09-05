/**
 * Memory-Mapped Pager — Zero-copy reads via mmap
 *
 * Reads are served directly from mmap'd pages (no memcpy, no read() syscall).
 * Writes go through a write-ahead log (sequential append, fsync-on-commit).
 *
 * Performance characteristics:
 *   - Read latency: ~50ns (pointer dereference, page already in OS cache)
 *   - Write latency: ~200ns (WAL append + optional fsync)
 *   - No read() syscall for cached pages
 *   - OS handles page eviction (LRU via kernel page cache)
 */
#ifndef LOCALDB_MMAP_PAGER_H
#define LOCALDB_MMAP_PAGER_H

#include "localdb.h"
#include <stdint.h>
#include <stdbool.h>

/* Cache-line size for alignment */
#define LOCALDB_CACHE_LINE 64

typedef struct {
    uint32_t magic;         /* 0x4C444250 = "LDBP" */
    uint32_t page_size;
    uint32_t page_count;
    uint32_t free_list_head;
    uint32_t schema_page;   /* page containing collection metadata */
    uint8_t  reserved[4072];
} __attribute__((aligned(LOCALDB_CACHE_LINE))) db_header;

/* Page cache entry (hash table for O(1) lookup) */
typedef struct page_entry {
    uint32_t           page_no;
    volatile int       refcount;
    bool               dirty;
    uint8_t           *data;       /* pointer into mmap region */
    struct page_entry *hash_next;  /* hash chain */
    struct page_entry *lru_prev;   /* LRU doubly-linked list */
    struct page_entry *lru_next;
} __attribute__((aligned(LOCALDB_CACHE_LINE))) page_entry;

/* Pre-allocated page pool — avoids malloc during write path */
#define PAGE_POOL_SIZE 256
typedef struct {
    page_entry  entries[PAGE_POOL_SIZE];
    page_entry *free_list;
    uint32_t    allocated;
} page_pool;

/* mmap pager */
typedef struct localdb_mmap_pager {
    int         fd;
    char       *path;
    uint8_t    *mmap_base;    /* mmap base address */
    size_t      mmap_size;    /* total mmap'd size */
    size_t      file_size;    /* actual file size */
    uint32_t    page_size;
    uint32_t    page_count;
    uint32_t    flags;

    /* Hash-indexed page cache */
    page_entry **hash_table;
    uint32_t     hash_size;   /* power of 2 */
    uint32_t     hash_mask;

    /* LRU eviction list */
    page_entry  *lru_head;    /* most recent */
    page_entry  *lru_tail;    /* least recent */
    uint32_t     cache_count;
    uint32_t     cache_max;

    /* Pre-allocated page pool */
    page_pool    pool;

    /* Performance counters */
    uint64_t     cache_hits;
    uint64_t     cache_misses;
    uint64_t     read_bytes;
    uint64_t     write_bytes;
    uint64_t     mmap_bytes;
} localdb_mmap_pager;

int  localdb_mmap_pager_open(const char *path, int flags, localdb_mmap_pager **out);
void localdb_mmap_pager_close(localdb_mmap_pager *pager);

/* Get pointer to page data (zero-copy from mmap). Caller must not modify. */
int  localdb_mmap_pager_read_ref(localdb_mmap_pager *pager, uint32_t page_no,
                                 const uint8_t **out_data);

/* Get mutable copy of page (for modification). Writes to WAL. */
int  localdb_mmap_pager_read_copy(localdb_mmap_pager *pager, uint32_t page_no,
                                  uint8_t **out_data);

/* Allocate a new page. Returns page number. */
int  localdb_mmap_pager_alloc(localdb_mmap_pager *pager, uint32_t *out_page_no);

/* Flush dirty pages to disk */
int  localdb_mmap_pager_flush(localdb_mmap_pager *pager);

/* Extend mmap region (called when file grows) */
int  localdb_mmap_pager_grow(localdb_mmap_pager *pager, size_t new_size);

/* Stats */
uint64_t localdb_mmap_pager_cache_hits(localdb_mmap_pager *pager);
uint64_t localdb_mmap_pager_cache_misses(localdb_mmap_pager *pager);

#endif
