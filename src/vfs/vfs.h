#ifndef LOCALDB_VFS_H
#define LOCALDB_VFS_H

#include "localdb.h"

/* Virtual File System abstraction.
 * Allows LocalDB to work with different storage backends:
 * - Standard file I/O (default)
 * - In-memory (for testing / ephemeral agent state)
 * - Memory-mapped files (for large databases)
 */

typedef struct localdb_vfs {
    int  (*open)(struct localdb_vfs *vfs, const char *path, int flags, void **out_fh);
    int  (*read)(void *fh, void *buf, size_t len, size_t offset);
    int  (*write)(void *fh, const void *buf, size_t len, size_t offset);
    int  (*close)(void *fh);
    int  (*size)(void *fh, size_t *out_size);
    void *user_data;
} localdb_vfs;

/* Get the default (file I/O) VFS */
localdb_vfs *localdb_vfs_default(void);

/* Get the in-memory VFS */
localdb_vfs *localdb_vfs_memory(void);

#endif
