#include "executor.h"
#include "../core/database.h"
#include "../storage/btree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void localdb_exec_init(localdb_exec_ctx *ctx, localdb *db) {
    ctx->db = db;
}

void localdb_exec_destroy(localdb_exec_ctx *ctx) {
    ctx->db = NULL;
}

/* Execute SQL string (high-level entry) */
int localdb_exec(localdb *db, const char *sql) {
    return localdb_exec_cb(db, sql, NULL, NULL);
}

int localdb_exec_cb(localdb *db, const char *sql, localdb_row_cb cb, void *ctx) {
    if (!db || !sql) return LOCALDB_ERROR;

    sql_stmt stmt;
    int rc = parse_sql(sql, &stmt);
    if (rc != LOCALDB_OK) return rc;

    rc = localdb_exec_stmt(&db->sql_ctx, &stmt, cb, ctx);
    sql_stmt_free(&stmt);
    return rc;
}

/* Collection management (accessed via database internal API) */
localdb_collection *localdb__find_collection(localdb *db, const char *name) {
    for (localdb_collection *c = db->collections; c; c = c->next) {
        if (strcmp(c->name, name) == 0) return c;
    }
    return NULL;
}

localdb_collection *localdb__create_collection(localdb *db, const char *name) {
    localdb_collection *c = (localdb_collection *)calloc(1, sizeof(localdb_collection));
    if (!c) return NULL;
    c->name = strdup(name);

    /* Create B-tree root page */
    if (btree_create(db->pager, &c->root_page) != LOCALDB_OK) {
        free(c->name);
        free(c);
        return NULL;
    }

    c->next = db->collections;
    db->collections = c;
    return c;
}

int localdb_exec_stmt(localdb_exec_ctx *ctx, sql_stmt *stmt,
                      localdb_row_cb cb, void *cb_ctx) {
    localdb *db = ctx->db;

    switch (stmt->type) {
    case STMT_BEGIN:
        return localdb_begin(db);
    case STMT_COMMIT:
        return localdb_commit(db);
    case STMT_ROLLBACK:
        return localdb_rollback(db);

    case STMT_CREATE_COLLECTION: {
        if (stmt->if_not_exists && localdb__find_collection(db, stmt->table)) {
            return LOCALDB_OK;
        }
        localdb_collection *c = localdb__create_collection(db, stmt->table);
        return c ? LOCALDB_OK : LOCALDB_ERROR_NOMEM;
    }

    case STMT_DROP_COLLECTION: {
        /* Stub: mark collection as dropped */
        return LOCALDB_OK;
    }

    case STMT_INSERT: {
        if (!stmt->table || stmt->col_count < 2) return LOCALDB_ERROR_SCHEMA;

        /* Find key and value columns */
        const char *key = NULL;
        const char *json = NULL;
        for (int i = 0; i < stmt->col_count; i++) {
            if (strcmp(stmt->columns[i], "key") == 0) key = stmt->values[i];
            else if (strcmp(stmt->columns[i], "value") == 0 ||
                     strcmp(stmt->columns[i], "data") == 0 ||
                     strcmp(stmt->columns[i], "json") == 0) json = stmt->values[i];
        }
        if (!key) {
            key = stmt->values[0];
            json = stmt->values[1];
        }
        if (!key || !json) return LOCALDB_ERROR_SCHEMA;

        localdb_collection *c = localdb__find_collection(db, stmt->table);
        if (!c) c = localdb__create_collection(db, stmt->table);
        if (!c) return LOCALDB_ERROR_NOMEM;

        return btree_insert(db->pager, c->root_page, key,
                            (const uint8_t *)json, (uint32_t)strlen(json));
    }

    case STMT_SELECT: {
        /* Simple SELECT * FROM collection */
        return LOCALDB_OK;
    }

    case STMT_DELETE:
    case STMT_UPDATE:
        /* Stub */
        return LOCALDB_OK;
    }

    return LOCALDB_ERROR_SCHEMA;
}
