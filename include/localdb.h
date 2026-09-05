/**
 * LocalDB - Ultra-High-Performance Embedded Database for AI Agents
 *
 * Performance design:
 *   - mmap-backed storage: zero-copy reads, no read() syscalls
 *   - Arena allocator: bump-pointer allocation, zero fragmentation
 *   - Pre-allocated page pool: no malloc during write path
 *   - Batch write coalescing: merge small writes into large sequential I/O
 *   - Lock-free reads for single-writer scenarios
 *   - Cache-line aligned structures to avoid false sharing
 *   - Hash-indexed collections for O(1) lookup
 *   - SIMD-friendly data layout
 */

#ifndef LOCALDB_H
#define LOCALDB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ──────────────────────────────────────────────── */
#define LOCALDB_VERSION_MAJOR 0
#define LOCALDB_VERSION_MINOR 2
#define LOCALDB_VERSION_PATCH 0
#define LOCALDB_VERSION_STRING "0.2.0"

/* ── Return codes ─────────────────────────────────────────── */
typedef enum {
    LOCALDB_OK             =  0,
    LOCALDB_ERROR          = -1,
    LOCALDB_ERROR_NOMEM    = -2,
    LOCALDB_ERROR_IO       = -3,
    LOCALDB_ERROR_CORRUPT  = -4,
    LOCALDB_ERROR_NOTFOUND = -5,
    LOCALDB_ERROR_BUSY     = -6,
    LOCALDB_ERROR_LOCKED   = -7,
    LOCALDB_ERROR_SCHEMA   = -8,
    LOCALDB_ERROR_CONSTRAINT = -9,
    LOCALDB_ERROR_RANGE    = -10,
    LOCALDB_ERROR_TIMEOUT  = -11,
    LOCALDB_DONE           =  1,
    LOCALDB_ROW            =  2,
} localdb_rc;

/* ── Open flags ───────────────────────────────────────────── */
#define LOCALDB_OPEN_READONLY   0x0001
#define LOCALDB_OPEN_READWRITE  0x0002
#define LOCALDB_OPEN_CREATE     0x0004
#define LOCALDB_OPEN_MEMORY     0x0008
#define LOCALDB_OPEN_NOLOG      0x0010
#define LOCALDB_OPEN_SHARED     0x0020
#define LOCALDB_OPEN_MMAP       0x0040  /* enable mmap (default for files) */
#define LOCALDB_OPEN_HUGETLB    0x0080  /* use huge pages for mmap */

/* ── Performance tuning flags ─────────────────────────────── */
#define LOCALDB_TUNE_BATCH_SIZE     0x01  /* WAL batch size (pages) */
#define LOCALDB_TUNE_CACHE_SIZE     0x02  /* page cache size (pages) */
#define LOCALDB_TUNE_ARENA_SIZE     0x03  /* arena block size (bytes) */
#define LOCALDB_TUNE_MADVISE        0x04  /* madvise hint (willneed/sequential/random) */
#define LOCALDB_TUNE_SYNC_MODE      0x05  /* 0=off, 1=normal, 2=full */
#define LOCALDB_TUNE_WAL_AUTOCHECK  0x06  /* auto-checkpoint threshold (pages) */

/* ── Value types ──────────────────────────────────────────── */
typedef enum {
    LOCALDB_NULL    = 0,
    LOCALDB_INTEGER = 1,
    LOCALDB_FLOAT   = 2,
    LOCALDB_TEXT    = 3,
    LOCALDB_BLOB    = 4,
    LOCALDB_JSON    = 5,
    LOCALDB_BOOL    = 6,
} localdb_type;

/* ── Opaque handles ───────────────────────────────────────── */
typedef struct localdb        localdb;
typedef struct localdb_stmt   localdb_stmt;
typedef struct localdb_batch  localdb_batch;

/* ── Callbacks ────────────────────────────────────────────── */
typedef int (*localdb_row_cb)(void *ctx, int ncols, char **values, char **names);

/* ── Performance stats ────────────────────────────────────── */
typedef struct {
    uint64_t page_count;
    uint64_t free_pages;
    uint64_t wal_size;
    uint64_t doc_count;
    uint32_t page_size;
    uint32_t collection_count;
    /* Performance counters */
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t mmap_bytes;       /* bytes served from mmap (zero-copy) */
    uint64_t arena_allocs;     /* arena bump-pointer allocations */
    uint64_t arena_bytes;      /* total bytes allocated via arena */
    uint64_t batch_writes;     /* number of batch write operations */
    uint64_t wal_entries;      /* total WAL entries written */
    double   avg_write_latency_us; /* average write latency (microseconds) */
    double   avg_read_latency_us;  /* average read latency (microseconds) */
} localdb_stats;

/* ────────────────────────────────────────────────────────────
 *  Database lifecycle
 * ──────────────────────────────────────────────────────────── */
int         localdb_open(const char *path, int flags, localdb **out_db);
int         localdb_open_memory(localdb **out_db);
int         localdb_close(localdb *db);
const char *localdb_errmsg(localdb *db);
const char *localdb_version(void);

/* ────────────────────────────────────────────────────────────
 *  Performance tuning
 * ──────────────────────────────────────────────────────────── */
int localdb_tune(localdb *db, int param, int64_t value);
int localdb_set_page_size(localdb *db, uint32_t size);
int localdb_set_wal(localdb *db, int enabled);

/* ────────────────────────────────────────────────────────────
 *  SQL execution
 * ──────────────────────────────────────────────────────────── */
int localdb_exec(localdb *db, const char *sql);
int localdb_exec_cb(localdb *db, const char *sql, localdb_row_cb cb, void *ctx);

/* ────────────────────────────────────────────────────────────
 *  Prepared statements
 * ──────────────────────────────────────────────────────────── */
int         localdb_prepare(localdb *db, const char *sql, size_t sql_len, localdb_stmt **out);
int         localdb_step(localdb_stmt *stmt);
int         localdb_finalize(localdb_stmt *stmt);
int         localdb_reset(localdb_stmt *stmt);
int         localdb_bind_int(localdb_stmt *stmt, int idx, int64_t val);
int         localdb_bind_float(localdb_stmt *stmt, int idx, double val);
int         localdb_bind_text(localdb_stmt *stmt, int idx, const char *val, size_t len);
int         localdb_bind_blob(localdb_stmt *stmt, int idx, const void *data, size_t len);
int         localdb_bind_json(localdb_stmt *stmt, int idx, const char *json);
int         localdb_bind_null(localdb_stmt *stmt, int idx);
int         localdb_bind_bool(localdb_stmt *stmt, int idx, int val);
int64_t     localdb_column_int(localdb_stmt *stmt, int col);
double      localdb_column_float(localdb_stmt *stmt, int col);
const char *localdb_column_text(localdb_stmt *stmt, int col);
const void *localdb_column_blob(localdb_stmt *stmt, int col, size_t *out_len);
int         localdb_column_bool(localdb_stmt *stmt, int col);
localdb_type localdb_column_type(localdb_stmt *stmt, int col);
int         localdb_column_count(localdb_stmt *stmt);
const char *localdb_column_name(localdb_stmt *stmt, int col);

/* ────────────────────────────────────────────────────────────
 *  Document / Key-Value API (zero-copy reads where possible)
 * ──────────────────────────────────────────────────────────── */
int   localdb_collection_create(localdb *db, const char *name);
int   localdb_collection_drop(localdb *db, const char *name);
int   localdb_doc_put(localdb *db, const char *collection, const char *key, const char *json);
int   localdb_doc_get(localdb *db, const char *collection, const char *key, char **out_json);
int   localdb_doc_del(localdb *db, const char *collection, const char *key);
int   localdb_doc_exists(localdb *db, const char *collection, const char *key);
int   localdb_doc_keys(localdb *db, const char *collection, localdb_row_cb cb, void *ctx);
int64_t localdb_doc_count(localdb *db, const char *collection);
int   localdb_doc_find(localdb *db, const char *collection, const char *filter, localdb_row_cb cb, void *ctx);

/* Zero-copy read: returns pointer into mmap'd page (valid until next write) */
int   localdb_doc_get_ref(localdb *db, const char *collection, const char *key,
                          const char **out_json, size_t *out_len);

/* ────────────────────────────────────────────────────────────
 *  TTL / Expiry
 * ──────────────────────────────────────────────────────────── */
int localdb_doc_set_ttl(localdb *db, const char *collection, const char *key, uint32_t seconds);
int localdb_doc_purge_expired(localdb *db, const char *collection);

/* ────────────────────────────────────────────────────────────
 *  Batch operations (high-throughput, coalesced writes)
 * ──────────────────────────────────────────────────────────── */
int localdb_batch_begin(localdb *db, localdb_batch **out);
int localdb_batch_put(localdb_batch *batch, const char *collection, const char *key, const char *json);
int localdb_batch_del(localdb_batch *batch, const char *collection, const char *key);
int localdb_batch_commit(localdb_batch *batch);
int localdb_batch_abort(localdb_batch *batch);

/* ────────────────────────────────────────────────────────────
 *  Transactions
 * ──────────────────────────────────────────────────────────── */
int localdb_begin(localdb *db);
int localdb_commit(localdb *db);
int localdb_rollback(localdb *db);

/* ────────────────────────────────────────────────────────────
 *  Utility
 * ──────────────────────────────────────────────────────────── */
void localdb_free(void *ptr);
int  localdb_vacuum(localdb *db);
int  localdb_stats_get(localdb *db, localdb_stats *out);

#ifdef __cplusplus
}
#endif

#endif /* LOCALDB_H */
