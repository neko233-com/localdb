/**
 * Arena Allocator — Bump-pointer, zero-fragmentation
 *
 * Used for short-lived allocations (document reads, SQL parsing).
 * Bulk-free at end of operation instead of per-object free().
 * Typical allocation: ~5ns (single pointer bump + alignment).
 */
#ifndef LOCALDB_ARENA_H
#define LOCALDB_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct arena_block {
    struct arena_block *next;
    size_t  capacity;
    size_t  used;
    uint8_t data[];  /* flexible array member */
} arena_block;

typedef struct {
    arena_block *current;   /* active block */
    arena_block *blocks;    /* all blocks (for full reset) */
    size_t       block_size;
    uint64_t     total_allocs;
    uint64_t     total_bytes;
} arena;

/* Initialize arena with given block size (0 = default 64KB) */
void  arena_init(arena *a, size_t block_size);

/* Destroy arena, free all blocks */
void  arena_destroy(arena *a);

/* Allocate from arena (aligned to 8 bytes). Never returns NULL. */
void *arena_alloc(arena *a, size_t size);

/* Allocate zeroed memory from arena */
void *arena_alloc_zero(arena *a, size_t size);

/* Duplicate a string into arena */
char *arena_strdup(arena *a, const char *s);

/* Duplicate a buffer into arena */
void *arena_memdup(arena *a, const void *data, size_t len);

/* Reset arena (keep blocks, reuse from offset 0). ~O(blocks). */
void  arena_reset(arena *a);

#endif
