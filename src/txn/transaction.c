#include "transaction.h"
#include "../core/database.h"
#include "../storage/pager.h"
#include "../storage/wal.h"

int localdb_begin(localdb *db) {
    if (!db) return LOCALDB_ERROR;
    if (db->in_txn) return LOCALDB_ERROR_BUSY;
    db->in_txn = 1;
    return LOCALDB_OK;
}

int localdb_commit(localdb *db) {
    if (!db) return LOCALDB_ERROR;
    if (!db->in_txn) return LOCALDB_ERROR;
    /* Flush dirty pages */
    localdb_pager_flush(db->pager);
    db->in_txn = 0;
    return LOCALDB_OK;
}

int localdb_rollback(localdb *db) {
    if (!db) return LOCALDB_ERROR;
    if (!db->in_txn) return LOCALDB_ERROR;
    /* TODO: restore from WAL or snapshot */
    db->in_txn = 0;
    return LOCALDB_OK;
}
