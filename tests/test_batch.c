#include "localdb.h"
#include <stdio.h>

extern void test_assert(int cond, const char *name);
#define ASSERT(expr) test_assert((expr), #expr)

void test_batch(void) {
    localdb *db = NULL;
    localdb_open_memory(&db);
    localdb_collection_create(db, "batch_test");

    /* Batch write */
    localdb_batch *batch = NULL;
    int rc = localdb_batch_begin(db, &batch);
    ASSERT(rc == LOCALDB_OK);
    ASSERT(batch != NULL);

    rc = localdb_batch_put(batch, "batch_test", "k1", "{\"v\":1}");
    ASSERT(rc == LOCALDB_OK);
    rc = localdb_batch_put(batch, "batch_test", "k2", "{\"v\":2}");
    ASSERT(rc == LOCALDB_OK);
    rc = localdb_batch_put(batch, "batch_test", "k3", "{\"v\":3}");
    ASSERT(rc == LOCALDB_OK);

    rc = localdb_batch_commit(batch);
    ASSERT(rc == LOCALDB_OK);

    int64_t count = localdb_doc_count(db, "batch_test");
    ASSERT(count == 3);

    /* Batch abort */
    localdb_batch_begin(db, &batch);
    localdb_batch_put(batch, "batch_test", "k4", "{\"v\":4}");
    localdb_batch_abort(batch);

    count = localdb_doc_count(db, "batch_test");
    ASSERT(count == 3); /* k4 should not be added */

    localdb_close(db);
}
