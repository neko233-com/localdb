#ifndef LOCALDB_EXECUTOR_H
#define LOCALDB_EXECUTOR_H

#include "localdb.h"
#include "parser.h"

typedef struct {
    localdb *db;
    /* execution state */
} localdb_exec_ctx;

void localdb_exec_init(localdb_exec_ctx *ctx, localdb *db);
void localdb_exec_destroy(localdb_exec_ctx *ctx);

/* Execute a parsed statement */
int localdb_exec_stmt(localdb_exec_ctx *ctx, sql_stmt *stmt,
                      localdb_row_cb cb, void *cb_ctx);

#endif
