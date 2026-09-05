#ifndef LOCALDB_WAL_H
#define LOCALDB_WAL_H

#include "localdb.h"
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint32_t page_no;
    uint8_t *data;
    uint32_t data_len;
} wal_entry;

struct localdb_wal {
    FILE      *fd;
    char      *path;
    uint64_t   size;
    uint32_t   entry_count;
};

int  localdb_wal_open(const char *db_path, localdb_wal **out);
void localdb_wal_close(localdb_wal *wal);
int  localdb_wal_append(localdb_wal *wal, uint32_t page_no,
                        const uint8_t *data, uint32_t len);
int  localdb_wal_checkpoint(localdb_wal *wal); /* replay & clear */
int  localdb_wal_truncate(localdb_wal *wal);

#endif
