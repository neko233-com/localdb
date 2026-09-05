#include "localdb.h"
#include <stdio.h>
#include <string.h>

extern void test_assert(int cond, const char *name);
#define ASSERT(expr) test_assert((expr), #expr)

void test_document(void) {
    localdb *db = NULL;
    localdb_open_memory(&db);

    /* Create collection */
    int rc = localdb_collection_create(db, "memory");
    ASSERT(rc == LOCALDB_OK);

    /* Put document */
    rc = localdb_doc_put(db, "memory", "conv_001",
        "{\"role\":\"user\",\"content\":\"Hello\"}");
    ASSERT(rc == LOCALDB_OK);

    /* Get document */
    char *json = NULL;
    rc = localdb_doc_get(db, "memory", "conv_001", &json);
    ASSERT(rc == LOCALDB_OK);
    ASSERT(json != NULL);
    if (json) {
        ASSERT(strstr(json, "Hello") != NULL);
        localdb_free(json);
    }

    /* Exists */
    rc = localdb_doc_exists(db, "memory", "conv_001");
    ASSERT(rc == LOCALDB_OK);

    /* Not found */
    rc = localdb_doc_exists(db, "memory", "nonexistent");
    ASSERT(rc == LOCALDB_ERROR_NOTFOUND);

    /* Count */
    localdb_doc_put(db, "memory", "conv_002",
        "{\"role\":\"assistant\",\"content\":\"Hi!\"}");
    int64_t count = localdb_doc_count(db, "memory");
    ASSERT(count == 2);

    /* Delete */
    rc = localdb_doc_del(db, "memory", "conv_001");
    ASSERT(rc == LOCALDB_OK);
    count = localdb_doc_count(db, "memory");
    ASSERT(count == 1);

    /* Drop collection */
    rc = localdb_collection_drop(db, "memory");
    ASSERT(rc == LOCALDB_OK);

    localdb_close(db);
}
