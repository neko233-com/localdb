/**
 * LocalDB Example: Basic Usage
 */
#include "localdb.h"
#include <stdio.h>

int main(void) {
    printf("LocalDB Basic Example\n");
    printf("Version: %s\n\n", localdb_version());

    /* Open in-memory database */
    localdb *db = NULL;
    int rc = localdb_open_memory(&db);
    if (rc != LOCALDB_OK) {
        printf("Failed to open: %d\n", rc);
        return 1;
    }

    /* Execute SQL */
    localdb_exec(db, "CREATE COLLECTION users");

    /* Document API */
    localdb_doc_put(db, "users", "alice",
        "{\"name\":\"Alice\",\"age\":30,\"role\":\"engineer\"}");
    localdb_doc_put(db, "users", "bob",
        "{\"name\":\"Bob\",\"age\":25,\"role\":\"designer\"}");

    printf("Users count: %lld\n", (long long)localdb_doc_count(db, "users"));

    /* Get a document */
    char *json = NULL;
    if (localdb_doc_get(db, "users", "alice", &json) == LOCALDB_OK) {
        printf("Alice: %s\n", json);
        localdb_free(json);
    }

    /* Transaction */
    localdb_begin(db);
    localdb_doc_put(db, "users", "charlie",
        "{\"name\":\"Charlie\",\"age\":35}");
    localdb_commit(db);

    printf("Final count: %lld\n", (long long)localdb_doc_count(db, "users"));

    localdb_close(db);
    printf("Done.\n");
    return 0;
}
