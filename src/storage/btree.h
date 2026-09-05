#ifndef LOCALDB_BTREE_H
#define LOCALDB_BTREE_H

#include "localdb.h"
#include "pager.h"

/* B-tree node types */
#define BTREE_INTERNAL 0
#define BTREE_LEAF     1

/* B-tree cell: key-value pair */
typedef struct {
    char    *key;
    uint8_t *value;
    uint32_t key_len;
    uint32_t val_len;
    uint64_t child_page; /* for internal nodes */
} btree_cell;

/* B-tree page header (stored in first bytes of each page) */
typedef struct {
    uint8_t  node_type;     /* BTREE_INTERNAL or BTREE_LEAF */
    uint16_t cell_count;
    uint16_t free_offset;
    uint16_t free_size;
    uint64_t right_child;   /* for internal nodes */
} btree_header;

int  btree_create(localdb_pager *pager, uint64_t *root_page_out);
int  btree_insert(localdb_pager *pager, uint64_t root_page,
                  const char *key, const uint8_t *val, uint32_t val_len);
int  btree_search(localdb_pager *pager, uint64_t root_page,
                  const char *key, uint8_t **val_out, uint32_t *val_len_out);
int  btree_delete(localdb_pager *pager, uint64_t root_page, const char *key);
int  btree_count(localdb_pager *pager, uint64_t root_page, uint64_t *count_out);

#endif
