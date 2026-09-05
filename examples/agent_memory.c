/**
 * LocalDB Example: Agent Memory Store
 *
 * Demonstrates using LocalDB as the memory backend for an AI agent:
 *   - Conversation history (append-only log)
 *   - Working memory (key-value with TTL)
 *   - Knowledge base (persistent documents)
 */
#include "localdb.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("LocalDB Agent Memory Example\n\n");

    localdb *db = NULL;
    localdb_open_memory(&db);

    /* ── 1. Conversation History ──────────────────────────── */
    printf("=== Conversation History ===\n");
    localdb_collection_create(db, "conversations");

    /* Agent appends messages as they happen */
    localdb_batch *batch = NULL;
    localdb_batch_begin(db, &batch);
    localdb_batch_put(batch, "conversations", "msg_001",
        "{\"role\":\"system\",\"content\":\"You are a helpful assistant.\"}");
    localdb_batch_put(batch, "conversations", "msg_002",
        "{\"role\":\"user\",\"content\":\"What is LocalDB?\"}");
    localdb_batch_put(batch, "conversations", "msg_003",
        "{\"role\":\"assistant\",\"content\":\"LocalDB is an embedded database optimized for AI agents.\"}");
    localdb_batch_commit(batch);

    printf("Messages stored: %lld\n",
        (long long)localdb_doc_count(db, "conversations"));

    /* ── 2. Working Memory (with TTL) ────────────────────── */
    printf("\n=== Working Memory ===\n");
    localdb_collection_create(db, "working_memory");

    /* Temporary context that expires */
    localdb_doc_put(db, "working_memory", "current_task",
        "{\"task\":\"summarize\",\"doc\":\"paper.pdf\",\"progress\":0.5}");
    localdb_doc_set_ttl(db, "working_memory", "current_task", 3600);

    localdb_doc_put(db, "working_memory", "user_preferences",
        "{\"language\":\"zh\",\"style\":\"concise\"}");
    /* No TTL — persists until explicitly deleted */

    char *task = NULL;
    if (localdb_doc_get(db, "working_memory", "current_task", &task) == LOCALDB_OK) {
        printf("Current task: %s\n", task);
        localdb_free(task);
    }

    /* ── 3. Knowledge Base ───────────────────────────────── */
    printf("\n=== Knowledge Base ===\n");
    localdb_collection_create(db, "knowledge");

    localdb_doc_put(db, "knowledge", "fact_001",
        "{\"subject\":\"LocalDB\",\"predicate\":\"is_a\",\"object\":\"embedded database\"}");
    localdb_doc_put(db, "knowledge", "fact_002",
        "{\"subject\":\"LocalDB\",\"predicate\":\"optimizes_for\",\"object\":\"AI agents\"}");
    localdb_doc_put(db, "knowledge", "fact_003",
        "{\"subject\":\"LocalDB\",\"predicate\":\"alternative_to\",\"object\":\"SQLite\"}");

    printf("Knowledge entries: %lld\n",
        (long long)localdb_doc_count(db, "knowledge"));

    /* Query by key prefix pattern */
    char *fact = NULL;
    if (localdb_doc_get(db, "knowledge", "fact_001", &fact) == LOCALDB_OK) {
        printf("Fact: %s\n", fact);
        localdb_free(fact);
    }

    /* ── 4. Cleanup ──────────────────────────────────────── */
    /* Purge expired working memory */
    localdb_doc_purge_expired(db, "working_memory");

    /* Get stats */
    localdb_stats stats;
    if (localdb_stats_get(db, &stats) == LOCALDB_OK) {
        printf("\n=== Database Stats ===\n");
        printf("Collections: %u\n", stats.collection_count);
        printf("Total docs:  %llu\n", (unsigned long long)stats.doc_count);
        printf("Page size:   %u\n", stats.page_size);
    }

    localdb_close(db);
    printf("\nDone.\n");
    return 0;
}
