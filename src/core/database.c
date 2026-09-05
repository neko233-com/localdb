/**
 * LocalDB Core - Database connection and lifecycle
 */
#include "localdb.h"
#include "database.h"
#include "../storage/pager.h"
#include "../storage/wal.h"
#include "../sql/executor.h"
#include "../util/string.h"
#include "../storage/btree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct localdb {
    localdb_pager   *pager;
    localdb_wal     *wal;
    localdb_exec_ctx sql_ctx;
    int              flags;
    int              in_txn;
    localdb_rc       last_err;
    char             err_msg[256];
    /* Collections metadata (simple linked list for now) */
    localdb_collection *collections;
};

struct localdb_collection {
    char               *name;
    uint64_t            root_page;
    uint64_t            doc_count;
    localdb_collection *next;
};

/* ── Internal helpers ─────────────────────────────────────── */

static void db_set_error(localdb *db, localdb_rc rc, const char *msg) {
    db->last_err = rc;
    if (msg) {
        strncpy(db->err_msg, msg, sizeof(db->err_msg) - 1);
        db->err_msg[sizeof(db->err_msg) - 1] = '\0';
    }
}

/* ── Public API ───────────────────────────────────────────── */

int localdb_open(const char *path, int flags, localdb **out_db) {
    if (!out_db) return LOCALDB_ERROR;
    *out_db = NULL;

    localdb *db = (localdb *)calloc(1, sizeof(localdb));
    if (!db) return LOCALDB_ERROR_NOMEM;

    db->flags = flags;

    /* Initialize pager */
    int rc = localdb_pager_open(path, flags, &db->pager);
    if (rc != LOCALDB_OK) {
        db_set_error(db, rc, "failed to open pager");
        free(db);
        return rc;
    }

    /* Initialize WAL */
    if (!(flags & LOCALDB_OPEN_NOLOG)) {
        rc = localdb_wal_open(path, &db->wal);
        if (rc != LOCALDB_OK) {
            db_set_error(db, rc, "failed to open WAL");
            localdb_pager_close(db->pager);
            free(db);
            return rc;
        }
    }

    /* Initialize SQL executor */
    localdb_exec_init(&db->sql_ctx, db);

    *out_db = db;
    return LOCALDB_OK;
}

int localdb_open_memory(localdb **out_db) {
    return localdb_open(":memory:", LOCALDB_OPEN_MEMORY | LOCALDB_OPEN_CREATE, out_db);
}

int localdb_close(localdb *db) {
    if (!db) return LOCALDB_ERROR;

    /* Free collections */
    localdb_collection *c = db->collections;
    while (c) {
        localdb_collection *next = c->next;
        free(c->name);
        free(c);
        c = next;
    }

    /* Close WAL and pager */
    if (db->wal) localdb_wal_close(db->wal);
    if (db->pager) localdb_pager_close(db->pager);

    localdb_exec_destroy(&db->sql_ctx);
    free(db);
    return LOCALDB_OK;
}

const char *localdb_errmsg(localdb *db) {
    if (!db) return "null database handle";
    return db->err_msg[0] ? db->err_msg : "ok";
}

const char *localdb_version(void) {
    return LOCALDB_VERSION_STRING;
}

void localdb_free(void *ptr) {
    free(ptr);
}

/* ── Collection API ───────────────────────────────────────── */

int localdb_collection_create(localdb *db, const char *name) {
    if (!db || !name) return LOCALDB_ERROR;
    if (localdb__find_collection(db, name)) return LOCALDB_OK; /* already exists */
    localdb__create_collection(db, name);
    return db->collections ? LOCALDB_OK : LOCALDB_ERROR_NOMEM;
}

int localdb_collection_drop(localdb *db, const char *name) {
    if (!db || !name) return LOCALDB_ERROR;
    localdb_collection **pp = &db->collections;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            localdb_collection *c = *pp;
            *pp = c->next;
            free(c->name);
            free(c);
            return LOCALDB_OK;
        }
        pp = &(*pp)->next;
    }
    return LOCALDB_ERROR_NOTFOUND;
}

/* ── Document API ─────────────────────────────────────────── */

int localdb_doc_put(localdb *db, const char *collection,
                    const char *key, const char *json) {
    if (!db || !collection || !key || !json) return LOCALDB_ERROR;
    localdb_collection *c = localdb__find_collection(db, collection);
    if (!c) c = localdb__create_collection(db, collection);
    if (!c) return LOCALDB_ERROR_NOMEM;
    int rc = btree_insert(db->pager, c->root_page, key,
                          (const uint8_t *)json, (uint32_t)strlen(json));
    if (rc == LOCALDB_OK) c->doc_count++;
    return rc;
}

int localdb_doc_get(localdb *db, const char *collection,
                    const char *key, char **out_json) {
    if (!db || !collection || !key || !out_json) return LOCALDB_ERROR;
    *out_json = NULL;
    localdb_collection *c = localdb__find_collection(db, collection);
    if (!c) return LOCALDB_ERROR_NOTFOUND;
    uint8_t *val = NULL;
    uint32_t val_len = 0;
    int rc = btree_search(db->pager, c->root_page, key, &val, &val_len);
    if (rc != LOCALDB_OK) return rc;
    *out_json = (char *)malloc(val_len + 1);
    if (!*out_json) { free(val); return LOCALDB_ERROR_NOMEM; }
    memcpy(*out_json, val, val_len);
    (*out_json)[val_len] = '\0';
    free(val);
    return LOCALDB_OK;
}

int localdb_doc_del(localdb *db, const char *collection, const char *key) {
    if (!db || !collection || !key) return LOCALDB_ERROR;
    localdb_collection *c = localdb__find_collection(db, collection);
    if (!c) return LOCALDB_ERROR_NOTFOUND;
    int rc = btree_delete(db->pager, c->root_page, key);
    if (rc == LOCALDB_OK && c->doc_count > 0) c->doc_count--;
    return rc;
}

int localdb_doc_exists(localdb *db, const char *collection, const char *key) {
    char *json = NULL;
    int rc = localdb_doc_get(db, collection, key, &json);
    if (rc == LOCALDB_OK) localdb_free(json);
    return rc;
}

int64_t localdb_doc_count(localdb *db, const char *collection) {
    if (!db || !collection) return 0;
    localdb_collection *c = localdb__find_collection(db, collection);
    if (!c) return 0;
    return (int64_t)c->doc_count;
}

int localdb_doc_keys(localdb *db, const char *collection,
                     localdb_row_cb cb, void *ctx) {
    /* Stub — would iterate B-tree leaf pages */
    (void)db; (void)collection; (void)cb; (void)ctx;
    return LOCALDB_OK;
}

int localdb_doc_find(localdb *db, const char *collection,
                     const char *filter, localdb_row_cb cb, void *ctx) {
    /* Stub — would scan collection and filter by JSON path */
    (void)db; (void)collection; (void)filter; (void)cb; (void)ctx;
    return LOCALDB_OK;
}

/* ── TTL API ──────────────────────────────────────────────── */

int localdb_doc_set_ttl(localdb *db, const char *collection,
                        const char *key, uint32_t seconds) {
    /* Stub — would store expiry timestamp in metadata */
    (void)db; (void)collection; (void)key; (void)seconds;
    return LOCALDB_OK;
}

int localdb_doc_purge_expired(localdb *db, const char *collection) {
    /* Stub — would scan for expired docs and delete them */
    (void)db; (void)collection;
    return LOCALDB_OK;
}

/* ── Batch API ────────────────────────────────────────────── */

struct localdb_batch {
    localdb *db;
    /* Simple array of pending operations */
    struct {
        enum { BATCH_PUT, BATCH_DEL } type;
        char *collection;
        char *key;
        char *json;
    } *ops;
    int count;
    int capacity;
};

int localdb_batch_begin(localdb *db, localdb_batch **out) {
    if (!db || !out) return LOCALDB_ERROR;
    localdb_batch *b = (localdb_batch *)calloc(1, sizeof(localdb_batch));
    if (!b) return LOCALDB_ERROR_NOMEM;
    b->db = db;
    b->capacity = 64;
    b->ops = calloc(b->capacity, sizeof(*b->ops));
    if (!b->ops) { free(b); return LOCALDB_ERROR_NOMEM; }
    *out = b;
    return LOCALDB_OK;
}

int localdb_batch_put(localdb_batch *batch, const char *collection,
                      const char *key, const char *json) {
    if (!batch || !collection || !key || !json) return LOCALDB_ERROR;
    if (batch->count >= batch->capacity) {
        batch->capacity *= 2;
        batch->ops = realloc(batch->ops, batch->capacity * sizeof(*batch->ops));
    }
    int i = batch->count++;
    batch->ops[i].type = BATCH_PUT;
    batch->ops[i].collection = strdup(collection);
    batch->ops[i].key = strdup(key);
    batch->ops[i].json = strdup(json);
    return LOCALDB_OK;
}

int localdb_batch_del(localdb_batch *batch, const char *collection,
                      const char *key) {
    if (!batch || !collection || !key) return LOCALDB_ERROR;
    if (batch->count >= batch->capacity) {
        batch->capacity *= 2;
        batch->ops = realloc(batch->ops, batch->capacity * sizeof(*batch->ops));
    }
    int i = batch->count++;
    batch->ops[i].type = BATCH_DEL;
    batch->ops[i].collection = strdup(collection);
    batch->ops[i].key = strdup(key);
    batch->ops[i].json = NULL;
    return LOCALDB_OK;
}

int localdb_batch_commit(localdb_batch *batch) {
    if (!batch) return LOCALDB_ERROR;
    localdb *db = batch->db;
    for (int i = 0; i < batch->count; i++) {
        if (batch->ops[i].type == BATCH_PUT) {
            localdb_doc_put(db, batch->ops[i].collection,
                           batch->ops[i].key, batch->ops[i].json);
        } else {
            localdb_doc_del(db, batch->ops[i].collection,
                           batch->ops[i].key);
        }
        free(batch->ops[i].collection);
        free(batch->ops[i].key);
        free(batch->ops[i].json);
    }
    free(batch->ops);
    free(batch);
    return LOCALDB_OK;
}

int localdb_batch_abort(localdb_batch *batch) {
    if (!batch) return LOCALDB_ERROR;
    for (int i = 0; i < batch->count; i++) {
        free(batch->ops[i].collection);
        free(batch->ops[i].key);
        free(batch->ops[i].json);
    }
    free(batch->ops);
    free(batch);
    return LOCALDB_OK;
}

/* ── Stats ────────────────────────────────────────────────── */

int localdb_stats_get(localdb *db, localdb_stats *out) {
    if (!db || !out) return LOCALDB_ERROR;
    memset(out, 0, sizeof(localdb_stats));
    out->page_size = localdb_pager_page_size(db->pager);
    out->page_count = localdb_pager_page_count(db->pager);

    uint64_t total_docs = 0;
    uint32_t col_count = 0;
    for (localdb_collection *c = db->collections; c; c = c->next) {
        total_docs += c->doc_count;
        col_count++;
    }
    out->doc_count = total_docs;
    out->collection_count = col_count;
    return LOCALDB_OK;
}

int localdb_vacuum(localdb *db) {
    /* Stub — would compact the file */
    (void)db;
    return LOCALDB_OK;
}

/* ── WAL config ───────────────────────────────────────────── */

int localdb_set_wal(localdb *db, int enabled) {
    if (!db) return LOCALDB_ERROR;
    if (!enabled && db->wal) {
        localdb_wal_close(db->wal);
        db->wal = NULL;
    }
    return LOCALDB_OK;
}

int localdb_set_page_size(localdb *db, uint32_t size) {
    if (!db || size < 512 || size > 65536 || (size & (size - 1)) != 0)
        return LOCALDB_ERROR;
    /* Must be set before first write */
    (void)db; (void)size;
    return LOCALDB_OK;
}
