#include "localdb.h"
#include <stdio.h>
#include <string.h>

extern void test_assert(int cond, const char *name);
#define ASSERT(expr) test_assert((expr), #expr)

void test_open_close(void) {
    /* Test: open and close in-memory database */
    localdb *db = NULL;
    int rc = localdb_open_memory(&db);
    ASSERT(rc == LOCALDB_OK);
    ASSERT(db != NULL);

    /* Test: version string */
    const char *ver = localdb_version();
    ASSERT(ver != NULL);
    ASSERT(strlen(ver) > 0);

    /* Test: close */
    rc = localdb_close(db);
    ASSERT(rc == LOCALDB_OK);

    /* Test: NULL db close doesn't crash */
    rc = localdb_close(NULL);
    ASSERT(rc == LOCALDB_ERROR);
}
