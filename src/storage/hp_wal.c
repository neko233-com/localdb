#include "hp_wal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

int localdb_hp_wal_open(const char *db_path, localdb_hp_wal **out) {
    if (!out) return LOCALDB_ERROR;
    *out = NULL;

    localdb_hp_wal *wal = (localdb_hp_wal *)calloc(1, sizeof(localdb_hp_wal));
    if (!wal) return LOCALDB_ERROR_NOMEM;

    size_t len = strlen(db_path);
    wal->path = (char *)malloc(len + 5);
    if (!wal->path) { free(wal); return LOCALDB_ERROR_NOMEM; }
    sprintf(wal->path, "%s.wal", db_path);

#ifdef _WIN32
    wal->fd = _open(wal->path, _O_RDWR | _O_BINARY | _O_CREAT, 0644);
#else
    wal->fd = open(wal->path, O_RDWR | O_CREAT | O_APPEND, 0644);
#endif
    if (wal->fd < 0) {
        free(wal->path);
        free(wal);
        return LOCALDB_ERROR_IO;
    }

    /* Allocate write buffer */
    wal->batch.buffer = (uint8_t *)malloc(WAL_BUFFER_SIZE);
    if (!wal->batch.buffer) {
#ifdef _WIN32
        _close(wal->fd);
#else
        close(wal->fd);
#endif
        free(wal->path);
        free(wal);
        return LOCALDB_ERROR_NOMEM;
    }
    wal->batch.buffer_size = WAL_BUFFER_SIZE;
    wal->batch.buffer_used = 0;
    wal->batch.entry_count = 0;

    *out = wal;
    return LOCALDB_OK;
}

void localdb_hp_wal_close(localdb_hp_wal *wal) {
    if (!wal) return;
    /* Flush any remaining entries */
    localdb_hp_wal_flush(wal);
    if (wal->fd >= 0) {
#ifdef _WIN32
        _close(wal->fd);
#else
        close(wal->fd);
#endif
    }
    free(wal->batch.buffer);
    free(wal->path);
    free(wal);
}

int localdb_hp_wal_append(localdb_hp_wal *wal, uint32_t page_no,
                          const uint8_t *data, uint32_t len) {
    if (!wal || !data) return LOCALDB_ERROR;

    size_t entry_size = WAL_ENTRY_HDR + len;

    /* If buffer would overflow, flush first */
    if (wal->batch.buffer_used + entry_size > wal->batch.buffer_size) {
        int rc = localdb_hp_wal_flush(wal);
        if (rc != LOCALDB_OK) return rc;
    }

    /* If entry is too large for buffer, write directly */
    if (entry_size > wal->batch.buffer_size) {
        wal_entry_hdr hdr = { .page_no = page_no, .data_len = len };
#ifdef _WIN32
        _write(wal->fd, &hdr, WAL_ENTRY_HDR);
        _write(wal->fd, data, len);
#else
        write(wal->fd, &hdr, WAL_ENTRY_HDR);
        write(wal->fd, data, len);
#endif
        wal->entry_count++;
        wal->total_entries++;
        wal->total_bytes += entry_size;
        return LOCALDB_OK;
    }

    /* Append to buffer */
    uint8_t *pos = wal->batch.buffer + wal->batch.buffer_used;
    wal_entry_hdr hdr = { .page_no = page_no, .data_len = len };
    memcpy(pos, &hdr, WAL_ENTRY_HDR);
    memcpy(pos + WAL_ENTRY_HDR, data, len);
    wal->batch.buffer_used += entry_size;
    wal->batch.entry_count++;

    return LOCALDB_OK;
}

int localdb_hp_wal_flush(localdb_hp_wal *wal) {
    if (!wal || wal->batch.buffer_used == 0) return LOCALDB_OK;

    /* One large sequential write */
#ifdef _WIN32
    int written = _write(wal->fd, wal->batch.buffer, (unsigned)wal->batch.buffer_used);
#else
    ssize_t written = write(wal->fd, wal->batch.buffer, wal->batch.buffer_used);
#endif

    if ((size_t)written != wal->batch.buffer_used) {
        return LOCALDB_ERROR_IO;
    }

    wal->entry_count += wal->batch.entry_count;
    wal->total_entries += wal->batch.entry_count;
    wal->total_bytes += wal->batch.buffer_used;
    wal->batch.buffer_used = 0;
    wal->batch.entry_count = 0;
    wal->batch_flushes++;

    return LOCALDB_OK;
}

int localdb_hp_wal_checkpoint(localdb_hp_wal *wal) {
    if (!wal) return LOCALDB_ERROR;
    /* Flush remaining entries */
    int rc = localdb_hp_wal_flush(wal);
    if (rc != LOCALDB_OK) return rc;
    /* TODO: replay entries to main file */
    return localdb_hp_wal_truncate(wal);
}

int localdb_hp_wal_truncate(localdb_hp_wal *wal) {
    if (!wal) return LOCALDB_ERROR;
#ifdef _WIN32
    _chsize(wal->fd, 0);
    _lseek(wal->fd, 0, SEEK_SET);
#else
    ftruncate(wal->fd, 0);
    lseek(wal->fd, 0, SEEK_SET);
#endif
    wal->entry_count = 0;
    wal->size = 0;
    return LOCALDB_OK;
}

uint64_t localdb_hp_wal_total_entries(localdb_hp_wal *wal) { return wal ? wal->total_entries : 0; }
uint64_t localdb_hp_wal_total_bytes(localdb_hp_wal *wal) { return wal ? wal->total_bytes : 0; }
