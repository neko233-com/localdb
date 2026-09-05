/**
 * LocalDB - Embedded database optimized for AI Agents
 *
 * A fast, lightweight embedded database designed as a drop-in SQLite
 * replacement for AI agent workloads: conversation history, memory
 * storage, knowledge bases, tool-call logs, and structured state.
 *
 * Key advantages over SQLite for agent use cases:
 *   - Append-optimized WAL for high-throughput conversation logging
 *   - Native JSON/document support (agents deal in structured data)
 *   - Async I/O friendly API (non-blocking reads/writes)
 *   - Minimal memory footprint with lazy loading
 *   - Built-in TTL/expiry for ephemeral agent state
 */

#ifndef LOCALDB_H
#define LOCALDB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────
 *  Version
 * ───────────────────────────────────────────────────────────── */
#define LOCALDB_VERSION_MAJOR 0
#define LOCALDB_VERSION_MINOR 1
#define LOCALDB_VERSION_PATCH 0
#define LOCALDB_VERSION_STRING "0.1.0"

/* ─────────────────────────────────────────────────────────────
 *  Return codes
 * ───────────────────────────────────────────────────────────── */
typedef enum {
    LOCALDB_OK            = 0,
    LOCALDB_ERROR         = -1,
    LOCALDB_ERROR_NOMEM   = -2,
    LOCALDB_ERROR_IO      = -3,
    LOCALDB_ERROR_CORRUPT = -4,
    LOCALDB_ERROR_NOTFOUND = -5,
    LOCALDB_ERROR_BUSY    = -6,
    LOCALDB_ERROR_LOCKED  = -7,
    LOCALDB_ERROR_SCHEMA  = -8,
    LOCALDB_ERROR_CONSTRAINT = -9,
    LOCALDB_ERROR_RANGE   = -10,
    LOCALDB_ERROR_TIMEOUT = -11,
    LOCALDB_DONE          = 1,
    LOCALDB_ROW           = 2,
} localdb_rc;

/* ─────────────────────────────────────────────────────────────
 *  Open flags
 * ───────────────────────────────────────────────────────────── */
#define LOCALDB_OPEN_READONLY   0x0001
#define LOCALDB_OPEN_READWRITE  0x0002
#define LOCALDB_OPEN_CREATE     0x0004
#define LOCALDB_OPEN_MEMORY     0x0008  /* in-memory only, no file */
#define LOCALDB_OPEN_NOLOG      0x0010  /* disable WAL for ephemeral */
#define LOCALDB_OPEN_SHARED     0x0020  /* allow multi-process */

/* ─────────────────────────────────────────────────────────────
 *  Value types for rows / documents
 * ───────────────────────────────────────────────────────────── */
typedef enum {
    LOCALDB_NULL    = 0,
    LOCALDB_INTEGER = 1,
    LOCALDB_FLOAT   = 2,
    LOCALDB_TEXT    = 3,
    LOCALDB_BLOB    = 4,
    LOCALDB_JSON    = 5,  /* stored as TEXT, validated on write */
    LOCALDB_BOOL    = 6,
} localdb_type;

/* ─────────────────────────────────────────────────────────────
 *  Opaque handles
 * ───────────────────────────────────────────────────────────── */
typedef struct localdb        localdb;        /* database connection */
typedef struct localdb_stmt   localdb_stmt;   /* prepared statement */
typedef struct localdb_doc    localdb_doc;    /* document (JSON-like row) */
typedef struct localdb_cursor localdb_cursor; /* iteration cursor */
typedef struct localdb_batch  localdb_batch;  /* batch write handle */

/* ─────────────────────────────────────────────────────────────
 *  Value container
 * ───────────────────────────────────────────────────────────── */
typedef struct {
    localdb_type type;
    union {
        int64_t  i64;
        double   f64;
        struct {
            const char *data;
            size_t      len;
        } str;   /* TEXT, JSON, BLOB */
        int      b;  /* BOOL */
    };
} localdb_value;

/* ─────────────────────────────────────────────────────────────
 *  Database lifecycle
 * ───────────────────────────────────────────────────────────── */

/** Open or create a database file. */
int localdb_open(const char *path, int flags, localdb **out_db);

/** Open an in-memory database (agent ephemeral state). */
int localdb_open_memory(localdb **out_db);

/** Close database, flush WAL, release resources. */
int localdb_close(localdb *db);

/** Enable/disable write-ahead log. */
int localdb_set_wal(localdb *db, int enabled);

/** Set page size (default 4096). Must be power of 2, before first write. */
int localdb_set_page_size(localdb *db, uint32_t size);

/** Get human-readable error message for last error. */
const char *localdb_errmsg(localdb *db);

/* ─────────────────────────────────────────────────────────────
 *  SQL execution (subset — agent-friendly)
 * ───────────────────────────────────────────────────────────── */

/** Execute one or more SQL statements (no result rows). */
int localdb_exec(localdb *db, const char *sql);

/** Execute SQL with callback for each result row. */
typedef int (*localdb_row_cb)(void *ctx, int ncols, char **values, char **names);
int localdb_exec_cb(localdb *db, const char *sql, localdb_row_cb cb, void *ctx);

/* ─────────────────────────────────────────────────────────────
 *  Prepared statements
 * ───────────────────────────────────────────────────────────── */

/** Compile SQL into a prepared statement. */
int localdb_prepare(localdb *db, const char *sql, size_t sql_len, localdb_stmt **out);

/** Step to next result row. Returns LOCALDB_ROW or LOCALDB_DONE. */
int localdb_step(localdb_stmt *stmt);

/** Finalize (destroy) a prepared statement. */
int localdb_finalize(localdb_stmt *stmt);

/** Reset a statement for re-execution. */
int localdb_reset(localdb_stmt *stmt);

/** Bind parameters (1-indexed). */
int localdb_bind_int(localdb_stmt *stmt, int idx, int64_t val);
int localdb_bind_float(localdb_stmt *stmt, int idx, double val);
int localdb_bind_text(localdb_stmt *stmt, int idx, const char *val, size_t len);
int localdb_bind_blob(localdb_stmt *stmt, int idx, const void *data, size_t len);
int localdb_bind_json(localdb_stmt *stmt, int idx, const char *json);
int localdb_bind_null(localdb_stmt *stmt, int idx);
int localdb_bind_bool(localdb_stmt *stmt, int idx, int val);

/** Named parameter binding. */
int localdb_bind_text_name(localdb_stmt *stmt, const char *name, const char *val, size_t len);
int localdb_bind_int_name(localdb_stmt *stmt, const char *name, int64_t val);
int localdb_bind_json_name(localdb_stmt *stmt, const char *name, const char *json);
int localdb_bind_null_name(localdb_stmt *stmt, const char *name);

/** Column getters (0-indexed). */
int64_t      localdb_column_int(localdb_stmt *stmt, int col);
double       localdb_column_float(localdb_stmt *stmt, int col);
const char  *localdb_column_text(localdb_stmt *stmt, int col);
const void  *localdb_column_blob(localdb_stmt *stmt, int col, size_t *out_len);
const char  *localdb_column_json(localdb_stmt *stmt, int col);
int          localdb_column_bool(localdb_stmt *stmt, int col);
localdb_type localdb_column_type(localdb_stmt *stmt, int col);
int          localdb_column_count(localdb_stmt *stmt);
const char  *localdb_column_name(localdb_stmt *stmt, int col);

/* ─────────────────────────────────────────────────────────────
 *  Document / Key-Value API (agent-optimized)
 *
 *  Agents commonly store structured JSON documents keyed by ID.
 *  This is a high-level convenience over the SQL layer.
 * ───────────────────────────────────────────────────────────── */

/** Create a collection (like a table optimized for JSON docs). */
int localdb_collection_create(localdb *db, const char *name);

/** Drop a collection. */
int localdb_collection_drop(localdb *db, const char *name);

/** Insert/upsert a JSON document with a string key. */
int localdb_doc_put(localdb *db, const char *collection,
                    const char *key, const char *json);

/** Get a document by key. Returns JSON string (caller must free via localdb_free). */
int localdb_doc_get(localdb *db, const char *collection,
                    const char *key, char **out_json);

/** Delete a document by key. */
int localdb_doc_del(localdb *db, const char *collection, const char *key);

/** Check if a document exists. */
int localdb_doc_exists(localdb *db, const char *collection, const char *key);

/** List all keys in a collection. Callback receives each key. */
int localdb_doc_keys(localdb *db, const char *collection,
                     localdb_row_cb cb, void *ctx);

/** Count documents in a collection. */
int64_t localdb_doc_count(localdb *db, const char *collection);

/** Query documents with a JSON path filter (subset of MongoDB-style).
 *  Example: localdb_doc_find(db, "memory", "type == 'conversation'", cb, ctx);
 */
int localdb_doc_find(localdb *db, const char *collection,
                     const char *filter, localdb_row_cb cb, void *ctx);

/* ─────────────────────────────────────────────────────────────
 *  TTL / Expiry (agent ephemeral state)
 * ───────────────────────────────────────────────────────────── */

/** Set TTL on a document (seconds from now). 0 = no expiry. */
int localdb_doc_set_ttl(localdb *db, const char *collection,
                        const char *key, uint32_t seconds);

/** Purge all expired documents from a collection. */
int localdb_doc_purge_expired(localdb *db, const char *collection);

/* ─────────────────────────────────────────────────────────────
 *  Batch operations (high-throughput writes)
 * ───────────────────────────────────────────────────────────── */

/** Begin a batch write. Multiple operations are buffered and flushed together. */
int localdb_batch_begin(localdb *db, localdb_batch **out);

/** Add a document put to the batch. */
int localdb_batch_put(localdb_batch *batch, const char *collection,
                      const char *key, const char *json);

/** Add a document delete to the batch. */
int localdb_batch_del(localdb_batch *batch, const char *collection,
                      const char *key);

/** Commit the batch (atomic). */
int localdb_batch_commit(localdb_batch *batch);

/** Abort the batch, discard buffered operations. */
int localdb_batch_abort(localdb_batch *batch);

/* ─────────────────────────────────────────────────────────────
 *  Transactions
 * ───────────────────────────────────────────────────────────── */

/** Begin a transaction. */
int localdb_begin(localdb *db);

/** Commit the current transaction. */
int localdb_commit(localdb *db);

/** Rollback the current transaction. */
int localdb_rollback(localdb *db);

/* ─────────────────────────────────────────────────────────────
 *  Utility
 * ───────────────────────────────────────────────────────────── */

/** Free memory allocated by LocalDB (e.g. doc_get output). */
void localdb_free(void *ptr);

/** Get the LocalDB library version string. */
const char *localdb_version(void);

/** Compact the database file (reclaim space from deleted docs). */
int localdb_vacuum(localdb *db);

/** Get database statistics. */
typedef struct {
    uint64_t page_count;
    uint64_t free_pages;
    uint64_t wal_size;
    uint64_t doc_count;     /* total across all collections */
    uint32_t page_size;
    uint32_t collection_count;
} localdb_stats;

int localdb_stats_get(localdb *db, localdb_stats *out);

#ifdef __cplusplus
}
#endif

#endif /* LOCALDB_H */
