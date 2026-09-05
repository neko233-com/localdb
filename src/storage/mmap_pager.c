/**
 * mmap Pager Implementation — Ultra-fast zero-copy reads
 */
#include "mmap_pager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#define DEFAULT_PAGE_SIZE  4096
#define DEFAULT_CACHE_MAX  4096  /* 16MB worth of pages at 4KB */
#define HASH_INITIAL_SIZE  1024
#define GROWTH_FACTOR      2
#define MMAP_GROW_STEP     (16 * 1024 * 1024)  /* grow by 16MB */

/* ── Platform mmap helpers ────────────────────────────────── */

static uint8_t *platform_mmap(int fd, size_t size, bool read_only) {
#ifdef _WIN32
    HANDLE fh = (HANDLE)_get_osfhandle(fd);
    if (fh == INVALID_HANDLE_VALUE) return NULL;
    DWORD protect = read_only ? PAGE_READONLY : PAGE_READWRITE;
    HANDLE mapping = CreateFileMapping(fh, NULL, protect, 0, (DWORD)size, NULL);
    if (!mapping) return NULL;
    DWORD access = read_only ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
    uint8_t *ptr = (uint8_t *)MapViewOfFile(mapping, access, 0, 0, size);
    CloseHandle(mapping);
    return ptr;
#else
    int prot = PROT_READ;
    if (!read_only) prot |= PROT_WRITE;
    return (uint8_t *)mmap(NULL, size, prot, MAP_SHARED, fd, 0);
#endif
}

static void platform_munmap(uint8_t *ptr, size_t size) {
#ifdef _WIN32
    (void)size;
    UnmapViewOfFile(ptr);
#else
    munmap(ptr, size);
#endif
}

static void platform_madvise_random(uint8_t *ptr, size_t size) {
#ifdef _WIN32
    (void)ptr; (void)size;
#else
    madvise(ptr, size, MADV_RANDOM);
#endif
}

static void platform_madvise_sequential(uint8_t *ptr, size_t size) {
#ifdef _WIN32
    (void)ptr; (void)size;
#else
    madvise(ptr, size, MADV_SEQUENTIAL);
#endif
}

/* ── Page pool ────────────────────────────────────────────── */

static void page_pool_init(page_pool *pool) {
    pool->free_list = NULL;
    pool->allocated = 0;
    for (int i = PAGE_POOL_SIZE - 1; i >= 0; i--) {
        pool->entries[i].hash_next = NULL;
        pool->entries[i].lru_prev = NULL;
        pool->entries[i].lru_next = NULL;
        pool->entries[i].refcount = 0;
        pool->entries[i].dirty = false;
        pool->entries[i].data = NULL;
        pool->entries[i].page_no = 0;
        /* Link into free list */
        pool->entries[i].hash_next = (struct page_entry *)pool->free_list;
        pool->free_list = &pool->entries[i];
    }
}

static page_entry *page_pool_alloc(page_pool *pool) {
    if (!pool->free_list) return NULL;
    page_entry *e = pool->free_list;
    pool->free_list = (page_entry *)e->hash_next;
    e->hash_next = NULL;
    e->refcount = 1;
    e->dirty = false;
    pool->allocated++;
    return e;
}

static void page_pool_free(page_pool *pool, page_entry *e) {
    e->refcount = 0;
    e->dirty = false;
    e->hash_next = (struct page_entry *)pool->free_list;
    pool->free_list = e;
    pool->allocated--;
}

/* ── Hash table ───────────────────────────────────────────── */

static inline uint32_t hash_page(uint32_t page_no) {
    /* Fibonacci hashing */
    return (page_no * 2654435761u);
}

static page_entry *hash_lookup(localdb_mmap_pager *p, uint32_t page_no) {
    uint32_t idx = hash_page(page_no) & p->hash_mask;
    page_entry *e = p->hash_table[idx];
    while (e) {
        if (e->page_no == page_no) return e;
        e = (page_entry *)e->hash_next;
    }
    return NULL;
}

static void hash_insert(localdb_mmap_pager *p, page_entry *e) {
    uint32_t idx = hash_page(e->page_no) & p->hash_mask;
    e->hash_next = (struct page_entry *)p->hash_table[idx];
    p->hash_table[idx] = e;
}

static void hash_remove(localdb_mmap_pager *p, uint32_t page_no) {
    uint32_t idx = hash_page(page_no) & p->hash_mask;
    page_entry **pp = &p->hash_table[idx];
    while (*pp) {
        if ((*pp)->page_no == page_no) {
            *pp = (page_entry *)(*pp)->hash_next;
            return;
        }
        pp = (page_entry **)&(*pp)->hash_next;
    }
}

/* ── LRU management ───────────────────────────────────────── */

static void lru_touch(localdb_mmap_pager *p, page_entry *e) {
    if (e == p->lru_head) return; /* already MRU */

    /* Remove from current position */
    if (e->lru_prev) e->lru_prev->lru_next = e->lru_next;
    if (e->lru_next) e->lru_next->lru_prev = e->lru_prev;
    if (e == p->lru_tail) p->lru_tail = e->lru_prev;

    /* Insert at head */
    e->lru_prev = NULL;
    e->lru_next = (struct page_entry *)p->lru_head;
    if (p->lru_head) p->lru_head->lru_prev = (struct page_entry *)e;
    p->lru_head = e;
    if (!p->lru_tail) p->lru_tail = e;
}

static page_entry *lru_evict(localdb_mmap_pager *p) {
    page_entry *victim = p->lru_tail;
    if (!victim) return NULL;

    /* Remove from LRU */
    p->lru_tail = victim->lru_prev;
    if (p->lru_tail) p->lru_tail->lru_next = NULL;
    if (victim == p->lru_head) p->lru_head = NULL;

    /* Remove from hash */
    hash_remove(p, victim->page_no);

    p->cache_count--;
    return victim;
}

/* ── Pager implementation ─────────────────────────────────── */

int localdb_mmap_pager_open(const char *path, int flags, localdb_mmap_pager **out) {
    if (!out) return LOCALDB_ERROR;
    *out = NULL;

    localdb_mmap_pager *p = (localdb_mmap_pager *)calloc(1, sizeof(localdb_mmap_pager));
    if (!p) return LOCALDB_ERROR_NOMEM;

    p->page_size = DEFAULT_PAGE_SIZE;
    p->cache_max = DEFAULT_CACHE_MAX;
    p->flags = flags;

    /* Initialize page pool */
    page_pool_init(&p->pool);

    /* Initialize hash table */
    p->hash_size = HASH_INITIAL_SIZE;
    p->hash_mask = p->hash_size - 1;
    p->hash_table = (page_entry **)calloc(p->hash_size, sizeof(page_entry *));
    if (!p->hash_table) { free(p); return LOCALDB_ERROR_NOMEM; }

    if (flags & LOCALDB_OPEN_MEMORY) {
        /* In-memory mode: allocate initial buffer */
        p->mmap_size = MMAP_GROW_STEP;
        p->mmap_base = (uint8_t *)calloc(1, p->mmap_size);
        if (!p->mmap_base) {
            free(p->hash_table);
            free(p);
            return LOCALDB_ERROR_NOMEM;
        }
        p->page_count = 1; /* header page */
        p->file_size = p->page_size;
    } else {
        /* File mode: open and mmap */
        const char *mode = (flags & LOCALDB_OPEN_READONLY) ? "rb" : "r+b";
        p->fd = -1;
#ifdef _WIN32
        p->fd = _open(path, (flags & LOCALDB_OPEN_READONLY) ? _O_RDONLY | _O_BINARY : _O_RDWR | _O_BINARY);
        if (p->fd < 0 && (flags & LOCALDB_OPEN_CREATE)) {
            p->fd = _open(path, _O_RDWR | _O_BINARY | _O_CREAT, 0644);
        }
#else
        p->fd = open(path, (flags & LOCALDB_OPEN_READONLY) ? O_RDONLY : O_RDWR);
        if (p->fd < 0 && (flags & LOCALDB_OPEN_CREATE)) {
            p->fd = open(path, O_RDWR | O_CREAT, 0644);
        }
#endif
        if (p->fd < 0) {
            free(p->hash_table);
            free(p);
            return LOCALDB_ERROR_IO;
        }

        /* Get file size */
#ifdef _WIN32
        struct _stat64 st;
        _fstat64(p->fd, &st);
        p->file_size = (size_t)st.st_size;
#else
        struct stat st;
        fstat(p->fd, &st);
        p->file_size = (size_t)st.st_size;
#endif

        if (p->file_size == 0 && (flags & LOCALDB_OPEN_CREATE)) {
            /* New file: write header */
            p->file_size = p->page_size;
#ifdef _WIN32
            _chsize(p->fd, (long)p->file_size);
#else
            ftruncate(p->fd, (off_t)p->file_size);
#endif
        }

        if (p->file_size == 0) {
#ifdef _WIN32
            _close(p->fd);
#else
            close(p->fd);
#endif
            free(p->hash_table);
            free(p);
            return LOCALDB_ERROR_IO;
        }

        /* mmap the file */
        p->mmap_size = p->file_size;
        p->mmap_base = platform_mmap(p->fd, p->mmap_size,
                                     (flags & LOCALDB_OPEN_READONLY) != 0);
        if (!p->mmap_base) {
            /* Fallback: mmap failed, use calloc for in-memory */
            p->mmap_size = p->file_size;
            p->mmap_base = (uint8_t *)calloc(1, p->mmap_size);
            if (!p->mmap_base) {
#ifdef _WIN32
                _close(p->fd);
#else
                close(p->fd);
#endif
                free(p->hash_table);
                free(p);
                return LOCALDB_ERROR_NOMEM;
            }
            /* Read file into memory */
#ifdef _WIN32
            _lseek(p->fd, 0, SEEK_SET);
            _read(p->fd, p->mmap_base, (unsigned)p->mmap_size);
#else
            lseek(p->fd, 0, SEEK_SET);
            read(p->fd, p->mmap_base, p->mmap_size);
#endif
        } else {
            /* Advise kernel: random access pattern for DB workload */
            platform_madvise_random(p->mmap_base, p->mmap_size);
        }

        p->page_count = (uint32_t)(p->file_size / p->page_size);
        if (p->page_count == 0) p->page_count = 1;

        p->path = strdup(path);
    }

    *out = p;
    return LOCALDB_OK;
}

void localdb_mmap_pager_close(localdb_mmap_pager *pager) {
    if (!pager) return;

    /* Flush dirty pages */
    localdb_mmap_pager_flush(pager);

    /* Unmap */
    if (pager->mmap_base) {
        if (pager->fd >= 0) {
            platform_munmap(pager->mmap_base, pager->mmap_size);
        } else {
            free(pager->mmap_base);
        }
    }

    /* Close fd */
    if (pager->fd >= 0) {
#ifdef _WIN32
        _close(pager->fd);
#else
        close(pager->fd);
#endif
    }

    free(pager->hash_table);
    free(pager->path);
    free(pager);
}

int localdb_mmap_pager_read_ref(localdb_mmap_pager *pager, uint32_t page_no,
                                const uint8_t **out_data) {
    if (!pager || !out_data) return LOCALDB_ERROR;
    *out_data = NULL;

    if (page_no >= pager->page_count) return LOCALDB_ERROR_RANGE;

    /* Check cache first (O(1) hash lookup) */
    page_entry *e = hash_lookup(pager, page_no);
    if (e) {
        lru_touch(pager, e);
        pager->cache_hits++;
        *out_data = e->data ? e->data : pager->mmap_base + (size_t)page_no * pager->page_size;
        return LOCALDB_OK;
    }

    /* Cache miss: return pointer directly into mmap (zero-copy) */
    *out_data = pager->mmap_base + (size_t)page_no * pager->page_size;
    pager->cache_misses++;
    pager->mmap_bytes += pager->page_size;

    /* Insert into cache for future lookups */
    e = page_pool_alloc(&pager->pool);
    if (!e) {
        /* Pool exhausted, evict LRU */
        e = lru_evict(pager);
        if (e) {
            hash_remove(pager, e->page_no);
        }
    }
    if (e) {
        e->page_no = page_no;
        e->data = NULL;  /* using mmap pointer directly */
        e->dirty = false;
        hash_insert(pager, e);
        lru_touch(pager, e);
        pager->cache_count++;
    }

    return LOCALDB_OK;
}

int localdb_mmap_pager_read_copy(localdb_mmap_pager *pager, uint32_t page_no,
                                 uint8_t **out_data) {
    if (!pager || !out_data) return LOCALDB_ERROR;
    *out_data = NULL;

    const uint8_t *ref = NULL;
    int rc = localdb_mmap_pager_read_ref(pager, page_no, &ref);
    if (rc != LOCALDB_OK) return rc;

    /* Copy to writable buffer */
    uint8_t *copy = (uint8_t *)malloc(pager->page_size);
    if (!copy) return LOCALDB_ERROR_NOMEM;
    memcpy(copy, ref, pager->page_size);
    *out_data = copy;
    return LOCALDB_OK;
}

int localdb_mmap_pager_alloc(localdb_mmap_pager *pager, uint32_t *out_page_no) {
    if (!pager || !out_page_no) return LOCALDB_ERROR;

    uint32_t new_page = pager->page_count;
    pager->page_count++;

    /* Grow file/mmap if needed */
    size_t new_size = (size_t)pager->page_count * pager->page_size;
    if (new_size > pager->mmap_size) {
        int rc = localdb_mmap_pager_grow(pager, new_size + MMAP_GROW_STEP);
        if (rc != LOCALDB_OK) return rc;
    }

    /* Zero the new page */
    uint8_t *page_data = pager->mmap_base + (size_t)new_page * pager->page_size;
    memset(page_data, 0, pager->page_size);

    *out_page_no = new_page;
    return LOCALDB_OK;
}

int localdb_mmap_pager_flush(localdb_mmap_pager *pager) {
    if (!pager) return LOCALDB_OK;

    if (pager->fd >= 0 && pager->mmap_base) {
#ifdef _WIN32
        FlushViewOfFile(pager->mmap_base, pager->mmap_size);
#else
        msync(pager->mmap_base, pager->mmap_size, MS_SYNC);
#endif
    }
    return LOCALDB_OK;
}

int localdb_mmap_pager_grow(localdb_mmap_pager *pager, size_t new_size) {
    if (!pager) return LOCALDB_ERROR;
    if (new_size <= pager->mmap_size) return LOCALDB_OK;

    /* Round up to page boundary */
    new_size = (new_size + pager->page_size - 1) & ~((size_t)pager->page_size - 1);

    if (pager->fd >= 0) {
        /* Extend file */
#ifdef _WIN32
        _chsize(pager->fd, (long)new_size);
#else
        ftruncate(pager->fd, (off_t)new_size);
#endif

        /* Remap */
        platform_munmap(pager->mmap_base, pager->mmap_size);
        pager->mmap_size = new_size;
        pager->mmap_base = platform_mmap(pager->fd, pager->mmap_size,
                                         (pager->flags & LOCALDB_OPEN_READONLY) != 0);
        if (!pager->mmap_base) return LOCALDB_ERROR_IO;
    } else {
        /* In-memory: realloc */
        pager->mmap_base = (uint8_t *)realloc(pager->mmap_base, new_size);
        if (!pager->mmap_base) return LOCALDB_ERROR_NOMEM;
        memset(pager->mmap_base + pager->mmap_size, 0, new_size - pager->mmap_size);
        pager->mmap_size = new_size;
    }

    return LOCALDB_OK;
}

uint64_t localdb_mmap_pager_cache_hits(localdb_mmap_pager *p) { return p ? p->cache_hits : 0; }
uint64_t localdb_mmap_pager_cache_misses(localdb_mmap_pager *p) { return p ? p->cache_misses : 0; }
