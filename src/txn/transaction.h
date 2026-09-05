#ifndef LOCALDB_TRANSACTION_H
#define LOCALDB_TRANSACTION_H

#include "localdb.h"

int localdb_begin(localdb *db);
int localdb_commit(localdb *db);
int localdb_rollback(localdb *db);

#endif
