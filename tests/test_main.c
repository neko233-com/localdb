/**
 * LocalDB Ultra-High-Performance Test Suite
 */
#include "localdb.h"
#include <stdio.h>
#include <string.h>

static int pass = 0, fail = 0;

#define ASSERT(expr) do { \
    if (expr) { printf("  PASS: %s\n", #expr); pass++; } \
    else { printf("  FAIL: %s (line %d)\n", #expr, __LINE__); fail++; } \
} while(0)

static void test_open_close(void) {
    printf("[Open/Close]\n");
    localdb *db = NULL;
    ASSERT(localdb_open_memory(&db) == LOCALDB_OK);
    ASSERT(db != NULL);
    ASSERT(localdb_version() != NULL);
    ASSERT(strlen(localdb_version()) > 0);
    ASSERT(localdb_close(db) == LOCALDB_OK);
    ASSERT(localdb_close(NULL) == LOCALDB_ERROR);
}

static void test_document(void) {
    printf("[Document API]\n");
    localdb *db = NULL;
    localdb_open_memory(&db);

    ASSERT(localdb_collection_create(db, "memory") == LOCALDB_OK);
    ASSERT(localdb_doc_put(db, "memory", "k1", "{\"role\":\"user\",\"msg\":\"Hello\"}") == LOCALDB_OK);
    ASSERT(localdb_doc_put(db, "memory", "k2", "{\"role\":\"bot\",\"msg\":\"Hi!\"}") == LOCALDB_OK);
    ASSERT(localdb_doc_put(db, "memory", "k3", "{\"role\":\"user\",\"msg\":\"Bye\"}") == LOCALDB_OK);

    ASSERT(localdb_doc_count(db, "memory") == 3);

    char *json = NULL;
    ASSERT(localdb_doc_get(db, "memory", "k1", &json) == LOCALDB_OK);
    ASSERT(json != NULL);
    if (json) { ASSERT(strstr(json, "Hello") != NULL); localdb_free(json); }

    ASSERT(localdb_doc_exists(db, "memory", "k2") == LOCALDB_OK);
    ASSERT(localdb_doc_exists(db, "memory", "nonexistent") == LOCALDB_ERROR_NOTFOUND);

    ASSERT(localdb_doc_del(db, "memory", "k1") == LOCALDB_OK);
    ASSERT(localdb_doc_count(db, "memory") == 2);

    ASSERT(localdb_collection_drop(db, "memory") == LOCALDB_OK);
    localdb_close(db);
}

static void test_batch(void) {
    printf("[Batch API]\n");
    localdb *db = NULL;
    localdb_open_memory(&db);
    localdb_collection_create(db, "batch");

    localdb_batch *batch = NULL;
    ASSERT(localdb_batch_begin(db, &batch) == LOCALDB_OK);

    char buf[128];
    for (int i = 0; i < 100; i++) {
        snprintf(buf, sizeof(buf), "{\"i\":%d}", i);
        localdb_batch_put(batch, "batch", "key", buf);
    }
    ASSERT(localdb_batch_commit(batch) == LOCALDB_OK);
    ASSERT(localdb_doc_count(db, "batch") == 100);

    localdb_close(db);
}

static void test_sql(void) {
    printf("[SQL]\n");
    localdb *db = NULL;
    localdb_open_memory(&db);

    ASSERT(localdb_exec(db, "CREATE COLLECTION test") == LOCALDB_OK);
    ASSERT(localdb_exec(db, "CREATE COLLECTION IF NOT EXISTS test") == LOCALDB_OK);
    ASSERT(localdb_exec(db, "BEGIN") == LOCALDB_OK);
    ASSERT(localdb_exec(db, "COMMIT") == LOCALDB_OK);

    localdb_close(db);
}

static void test_stats(void) {
    printf("[Stats]\n");
    localdb *db = NULL;
    localdb_open_memory(&db);
    localdb_collection_create(db, "s");
    localdb_doc_put(db, "s", "k", "{\"v\":1}");

    localdb_stats stats;
    ASSERT(localdb_stats_get(db, &stats) == LOCALDB_OK);
    ASSERT(stats.collection_count == 1);
    ASSERT(stats.doc_count == 1);

    localdb_close(db);
}

int main(void) {
    printf("=== LocalDB Ultra-HP Test Suite ===\n");
    printf("Version: %s\n\n", localdb_version());

    test_open_close();
    test_document();
    test_batch();
    test_sql();
    test_stats();

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
