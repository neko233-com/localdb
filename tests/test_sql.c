#include "localdb.h"
#include <stdio.h>
#include <string.h>

extern void test_assert(int cond, const char *name);
#define ASSERT(expr) test_assert((expr), #expr)

void test_sql(void) {
    localdb *db = NULL;
    localdb_open_memory(&db);

    /* CREATE COLLECTION */
    int rc = localdb_exec(db, "CREATE COLLECTION test");
    ASSERT(rc == LOCALDB_OK);

    /* INSERT */
    rc = localdb_exec(db, "INSERT INTO test (key, value) VALUES ('k1', '{\"x\":1}')");
    ASSERT(rc == LOCALDB_OK);

    /* Transaction */
    rc = localdb_exec(db, "BEGIN");
    ASSERT(rc == LOCALDB_OK);
    rc = localdb_exec(db, "COMMIT");
    ASSERT(rc == LOCALDB_OK);

    /* CREATE IF NOT EXISTS */
    rc = localdb_exec(db, "CREATE COLLECTION IF NOT EXISTS test");
    ASSERT(rc == LOCALDB_OK);

    localdb_close(db);
}
