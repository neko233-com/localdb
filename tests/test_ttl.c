#include "localdb.h"
#include <stdio.h>

extern void test_assert(int cond, const char *name);
#define ASSERT(expr) test_assert((expr), #expr)

void test_ttl(void) {
    localdb *db = NULL;
    localdb_open_memory(&db);
    localdb_collection_create(db, "ttl_test");

    /* Put with TTL */
    localdb_doc_put(db, "ttl_test", "ephemeral", "{\"data\":\"temp\"}");
    int rc = localdb_doc_set_ttl(db, "ttl_test", "ephemeral", 3600);
    ASSERT(rc == LOCALDB_OK);

    /* Zero TTL = no expiry */
    localdb_doc_put(db, "ttl_test", "permanent", "{\"data\":\"forever\"}");
    rc = localdb_doc_set_ttl(db, "ttl_test", "permanent", 0);
    ASSERT(rc == LOCALDB_OK);

    /* Purge (nothing should be purged yet) */
    rc = localdb_doc_purge_expired(db, "ttl_test");
    ASSERT(rc == LOCALDB_OK);

    localdb_close(db);
}
