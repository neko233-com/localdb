#ifndef LOCALDB_DATABASE_INTERNAL_H
#define LOCALDB_DATABASE_INTERNAL_H

#include "localdb.h"

/* Forward declarations for internal types */
typedef struct localdb_pager    localdb_pager;
typedef struct localdb_wal      localdb_wal;
typedef struct localdb_exec_ctx localdb_exec_ctx;
typedef struct localdb_collection localdb_collection;

/* Accessors for internal state (used by other modules) */
localdb_pager *localdb__pager(localdb *db);
localdb_wal   *localdb__wal(localdb *db);
int            localdb__flags(localdb *db);
int            localdb__in_txn(localdb *db);
void           localdb__set_in_txn(localdb *db, int val);

/* Collection management (internal) */
localdb_collection *localdb__find_collection(localdb *db, const char *name);
localdb_collection *localdb__create_collection(localdb *db, const char *name);

#endif
