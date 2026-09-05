#include "arena.h"
#include <stdlib.h>
#include <string.h>

#define ARENA_DEFAULT_BLOCK_SIZE (64 * 1024)  /* 64KB */
#define ARENA_ALIGN 8

static arena_block *arena_new_block(size_t min_size) {
    size_t cap = ARENA_DEFAULT_BLOCK_SIZE;
    while (cap < min_size + sizeof(arena_block)) cap *= 2;
    arena_block *b = (arena_block *)malloc(cap);
    if (!b) return NULL;
    b->next = NULL;
    b->capacity = cap - sizeof(arena_block);
    b->used = 0;
    return b;
}

void arena_init(arena *a, size_t block_size) {
    a->current = NULL;
    a->blocks = NULL;
    a->block_size = block_size > 0 ? block_size : ARENA_DEFAULT_BLOCK_SIZE;
    a->total_allocs = 0;
    a->total_bytes = 0;
}

void arena_destroy(arena *a) {
    arena_block *b = a->blocks;
    while (b) {
        arena_block *next = b->next;
        free(b);
        b = next;
    }
    a->current = NULL;
    a->blocks = NULL;
}

void *arena_alloc(arena *a, size_t size) {
    if (size == 0) return NULL;

    /* Align to ARENA_ALIGN */
    size = (size + ARENA_ALIGN - 1) & ~(ARENA_ALIGN - 1);

    /* Try current block */
    arena_block *b = a->current;
    if (b && b->capacity - b->used >= size) {
        void *ptr = b->data + b->used;
        b->used += size;
        a->total_allocs++;
        a->total_bytes += size;
        return ptr;
    }

    /* Need a new block */
    size_t new_cap = a->block_size;
    if (size > new_cap) new_cap = size;
    arena_block *nb = arena_new_block(new_cap);
    if (!nb) return NULL;

    nb->next = a->blocks;
    a->blocks = nb;
    a->current = nb;

    void *ptr = nb->data;
    nb->used = size;
    a->total_allocs++;
    a->total_bytes += size;
    return ptr;
}

void *arena_alloc_zero(arena *a, size_t size) {
    void *ptr = arena_alloc(a, size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

char *arena_strdup(arena *a, const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = (char *)arena_alloc(a, len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

void *arena_memdup(arena *a, const void *data, size_t len) {
    if (!data || len == 0) return NULL;
    void *dup = arena_alloc(a, len);
    if (dup) memcpy(dup, data, len);
    return dup;
}

void arena_reset(arena *a) {
    /* Reset all blocks to used=0 */
    for (arena_block *b = a->blocks; b; b = b->next) {
        b->used = 0;
    }
    /* Set current to the first (largest) block */
    a->current = a->blocks;
}
