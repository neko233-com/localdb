/**
 * Ultra-High-Performance WAL — Batch coalescing + sequential I/O
 *
 * Performance:
 *   - Batch coalescing: merge N page writes into one large sequential write
 *   - O_DIRECT bypass (Linux) for zero kernel buffering overhead
 *   - Pre-allocated write buffer: no malloc during write path
 *   - fsync-on-commit semantics for durability
 */
#ifndef LOCALDB_HP_WAL_H
#define LOCALDB_HP_WAL_H

#include "localdb.h"
#include <stdint.h>
#include <stdbool.h>

#define WAL_MAGIC       0x4C444257  /* "LDBW" */
#define WAL_ENTRY_HDR   8           /* page_no(4) + len(4) */
#define WAL_BUFFER_SIZE (1024 * 1024)  /* 1MB write buffer */

/* WAL entry header (on-disk) */
typedef struct {
    uint32_t page_no;
    uint32_t data_len;
    /* followed by data_len bytes of page data */
} __attribute__((packed)) wal_entry_hdr;

/* Batch write buffer */
typedef struct {
    uint8_t  *buffer;
    size_t    buffer_size;
    size_t    buffer_used;
    uint32_t  entry_count;
} wal_batch;

typedef struct localdb_hp_wal {
    int         fd;
    char       *path;
    uint64_t    size;
    uint32_t    entry_count;

    /* Write buffer for batch coalescing */
    wal_batch   batch;

    /* Stats */
    uint64_t    total_entries;
    uint64_t    total_bytes;
    uint64_t    batch_flushes;
} localdb_hp_wal;

int  localdb_hp_wal_open(const char *db_path, localdb_hp_wal **out);
void localdb_hp_wal_close(localdb_hp_wal *wal);

/* Buffer a page write (does not flush to disk) */
int  localdb_hp_wal_append(localdb_hp_wal *wal, uint32_t page_no,
                           const uint8_t *data, uint32_t len);

/* Flush all buffered entries to disk (one large sequential write + fsync) */
int  localdb_hp_wal_flush(localdb_hp_wal *wal);

/* Checkpoint: replay WAL into main file, then truncate */
int  localdb_hp_wal_checkpoint(localdb_hp_wal *wal);

/* Truncate WAL to empty */
int  localdb_hp_wal_truncate(localdb_hp_wal *wal);

/* Stats */
uint64_t localdb_hp_wal_total_entries(localdb_hp_wal *wal);
uint64_t localdb_hp_wal_total_bytes(localdb_hp_wal *wal);

#endif
