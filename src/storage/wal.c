#include "wal.h"
#include <stdlib.h>
#include <string.h>

int localdb_wal_open(const char *db_path, localdb_wal **out) {
    if (!out) return LOCALDB_ERROR;
    *out = NULL;

    localdb_wal *wal = (localdb_wal *)calloc(1, sizeof(localdb_wal));
    if (!wal) return LOCALDB_ERROR_NOMEM;

    /* WAL file is db_path + ".wal" */
    size_t len = strlen(db_path);
    wal->path = (char *)malloc(len + 5);
    if (!wal->path) { free(wal); return LOCALDB_ERROR_NOMEM; }
    sprintf(wal->path, "%s.wal", db_path);

    wal->fd = fopen(wal->path, "a+b");
    if (!wal->fd) {
        free(wal->path);
        free(wal);
        return LOCALDB_ERROR_IO;
    }

    *out = wal;
    return LOCALDB_OK;
}

void localdb_wal_close(localdb_wal *wal) {
    if (!wal) return;
    if (wal->fd) fclose(wal->fd);
    free(wal->path);
    free(wal);
}

int localdb_wal_append(localdb_wal *wal, uint32_t page_no,
                       const uint8_t *data, uint32_t len) {
    if (!wal || !wal->fd || !data) return LOCALDB_ERROR;

    /* Format: [page_no:4][len:4][data:len] */
    fwrite(&page_no, sizeof(uint32_t), 1, wal->fd);
    fwrite(&len, sizeof(uint32_t), 1, wal->fd);
    fwrite(data, 1, len, wal->fd);
    fflush(wal->fd);

    wal->entry_count++;
    wal->size += sizeof(uint32_t) * 2 + len;
    return LOCALDB_OK;
}

int localdb_wal_checkpoint(localdb_wal *wal) {
    if (!wal) return LOCALDB_ERROR;
    /* Stub: replay entries to main file, then truncate */
    return localdb_wal_truncate(wal);
}

int localdb_wal_truncate(localdb_wal *wal) {
    if (!wal || !wal->fd) return LOCALDB_ERROR;
    freopen(wal->path, "wb", wal->fd);
    if (!wal->fd) {
        wal->fd = fopen(wal->path, "a+b");
    }
    wal->entry_count = 0;
    wal->size = 0;
    return LOCALDB_OK;
}
