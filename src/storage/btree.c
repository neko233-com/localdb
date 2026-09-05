#include "btree.h"
#include <stdlib.h>
#include <string.h>

int btree_create(localdb_pager *pager, uint64_t *root_page_out) {
    if (!pager || !root_page_out) return LOCALDB_ERROR;

    localdb_page *page = NULL;
    int rc = localdb_pager_alloc(pager, &page);
    if (rc != LOCALDB_OK) return rc;

    memset(page->data, 0, localdb_pager_page_size(pager));
    btree_header *hdr = (btree_header *)page->data;
    hdr->node_type = BTREE_LEAF;
    hdr->cell_count = 0;

    localdb_pager_write(pager, page);
    *root_page_out = page->page_no;
    return LOCALDB_OK;
}

int btree_insert(localdb_pager *pager, uint64_t root_page,
                 const char *key, const uint8_t *val, uint32_t val_len) {
    if (!pager || !key) return LOCALDB_ERROR;

    localdb_page *page = NULL;
    int rc = localdb_pager_read(pager, (uint32_t)root_page, &page);
    if (rc != LOCALDB_OK) return rc;

    btree_header *hdr = (btree_header *)page->data;

    /* Simple leaf insertion for now (no split logic yet) */
    if (hdr->node_type != BTREE_LEAF) {
        return LOCALDB_ERROR_SCHEMA;
    }

    uint32_t key_len = (uint32_t)strlen(key);
    uint32_t cell_size = sizeof(uint32_t) * 2 + key_len + val_len; /* key_len + val_len + key + val */

    uint32_t page_size = localdb_pager_page_size(pager);
    if (hdr->free_offset + cell_size > page_size) {
        return LOCALDB_ERROR_RANGE; /* page full, needs split */
    }

    uint8_t *pos = page->data + sizeof(btree_header) + hdr->free_offset;
    memcpy(pos, &key_len, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    memcpy(pos, &val_len, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    memcpy(pos, key, key_len);
    pos += key_len;
    if (val && val_len > 0) memcpy(pos, val, val_len);

    hdr->cell_count++;
    hdr->free_offset += cell_size;

    localdb_pager_write(pager, page);
    return LOCALDB_OK;
}

int btree_search(localdb_pager *pager, uint64_t root_page,
                 const char *key, uint8_t **val_out, uint32_t *val_len_out) {
    if (!pager || !key || !val_out) return LOCALDB_ERROR;
    *val_out = NULL;
    if (val_len_out) *val_len_out = 0;

    localdb_page *page = NULL;
    int rc = localdb_pager_read(pager, (uint32_t)root_page, &page);
    if (rc != LOCALDB_OK) return rc;

    btree_header *hdr = (btree_header *)page->data;
    uint32_t key_len = (uint32_t)strlen(key);
    uint8_t *pos = page->data + sizeof(btree_header);

    for (uint16_t i = 0; i < hdr->cell_count; i++) {
        uint32_t klen, vlen;
        memcpy(&klen, pos, sizeof(uint32_t));
        pos += sizeof(uint32_t);
        memcpy(&vlen, pos, sizeof(uint32_t));
        pos += sizeof(uint32_t);

        if (klen == key_len && memcmp(pos, key, key_len) == 0) {
            pos += klen;
            uint8_t *result = (uint8_t *)malloc(vlen);
            if (!result) return LOCALDB_ERROR_NOMEM;
            memcpy(result, pos, vlen);
            *val_out = result;
            if (val_len_out) *val_len_out = vlen;
            return LOCALDB_OK;
        }
        pos += klen + vlen;
    }

    return LOCALDB_ERROR_NOTFOUND;
}

int btree_delete(localdb_pager *pager, uint64_t root_page, const char *key) {
    /* Stub — mark as deleted (tombstone) for now */
    (void)pager; (void)root_page; (void)key;
    return LOCALDB_OK;
}

int btree_count(localdb_pager *pager, uint64_t root_page, uint64_t *count_out) {
    if (!pager || !count_out) return LOCALDB_ERROR;

    localdb_page *page = NULL;
    int rc = localdb_pager_read(pager, (uint32_t)root_page, &page);
    if (rc != LOCALDB_OK) return rc;

    btree_header *hdr = (btree_header *)page->data;
    *count_out = hdr->cell_count;
    return LOCALDB_OK;
}
