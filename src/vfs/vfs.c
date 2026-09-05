#include "vfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Default VFS (standard file I/O) ──────────────────────── */

static int vfs_open(localdb_vfs *vfs, const char *path, int flags, void **out_fh) {
    (void)vfs;
    const char *mode = (flags & 0x0001) ? "rb" : "r+b";
    FILE *f = fopen(path, mode);
    if (!f && (flags & 0x0004)) f = fopen(path, "w+b");
    if (!f) return LOCALDB_ERROR_IO;
    *out_fh = f;
    return LOCALDB_OK;
}

static int vfs_read(void *fh, void *buf, size_t len, size_t offset) {
    FILE *f = (FILE *)fh;
    fseek(f, (long)offset, SEEK_SET);
    size_t n = fread(buf, 1, len, f);
    return (n == len) ? LOCALDB_OK : LOCALDB_ERROR_IO;
}

static int vfs_write(void *fh, const void *buf, size_t len, size_t offset) {
    FILE *f = (FILE *)fh;
    fseek(f, (long)offset, SEEK_SET);
    size_t n = fwrite(buf, 1, len, f);
    return (n == len) ? LOCALDB_OK : LOCALDB_ERROR_IO;
}

static int vfs_close(void *fh) {
    return fclose((FILE *)fh) == 0 ? LOCALDB_OK : LOCALDB_ERROR_IO;
}

static int vfs_size(void *fh, size_t *out_size) {
    FILE *f = (FILE *)fh;
    fseek(f, 0, SEEK_END);
    *out_size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    return LOCALDB_OK;
}

static localdb_vfs default_vfs = {
    .open = vfs_open,
    .read = vfs_read,
    .write = vfs_write,
    .close = vfs_close,
    .size = vfs_size,
    .user_data = NULL,
};

localdb_vfs *localdb_vfs_default(void) {
    return &default_vfs;
}

/* ── In-memory VFS ────────────────────────────────────────── */

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   capacity;
} mem_file;

static int mem_open(localdb_vfs *vfs, const char *path, int flags, void **out_fh) {
    (void)vfs; (void)path; (void)flags;
    mem_file *mf = (mem_file *)calloc(1, sizeof(mem_file));
    if (!mf) return LOCALDB_ERROR_NOMEM;
    mf->capacity = 4096;
    mf->data = (uint8_t *)calloc(1, mf->capacity);
    *out_fh = mf;
    return LOCALDB_OK;
}

static int mem_read(void *fh, void *buf, size_t len, size_t offset) {
    mem_file *mf = (mem_file *)fh;
    if (offset + len > mf->size) return LOCALDB_ERROR_IO;
    memcpy(buf, mf->data + offset, len);
    return LOCALDB_OK;
}

static int mem_write(void *fh, const void *buf, size_t len, size_t offset) {
    mem_file *mf = (mem_file *)fh;
    if (offset + len > mf->capacity) {
        size_t new_cap = mf->capacity;
        while (new_cap < offset + len) new_cap *= 2;
        mf->data = (uint8_t *)realloc(mf->data, new_cap);
        memset(mf->data + mf->capacity, 0, new_cap - mf->capacity);
        mf->capacity = new_cap;
    }
    memcpy(mf->data + offset, buf, len);
    if (offset + len > mf->size) mf->size = offset + len;
    return LOCALDB_OK;
}

static int mem_close(void *fh) {
    mem_file *mf = (mem_file *)fh;
    free(mf->data);
    free(mf);
    return LOCALDB_OK;
}

static int mem_size(void *fh, size_t *out_size) {
    *out_size = ((mem_file *)fh)->size;
    return LOCALDB_OK;
}

static localdb_vfs memory_vfs = {
    .open = mem_open,
    .read = mem_read,
    .write = mem_write,
    .close = mem_close,
    .size = mem_size,
    .user_data = NULL,
};

localdb_vfs *localdb_vfs_memory(void) {
    return &memory_vfs;
}
