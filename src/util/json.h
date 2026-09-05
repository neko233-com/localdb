#ifndef LOCALDB_UTIL_JSON_H
#define LOCALDB_UTIL_JSON_H

#include "localdb.h"

/* Minimal JSON utilities for document storage.
 * Not a full parser — just validation and path extraction. */

/** Validate JSON string. Returns 1 if valid, 0 if not. */
int localdb_json_validate(const char *json);

/** Extract a value by simple path (e.g. "name" or "user.age").
 *  Returns a newly allocated string, caller must free with localdb_free(). */
int localdb_json_get(const char *json, const char *path, char **out);

/** Get JSON string length (for storage sizing). */
int localdb_json_len(const char *json);

#endif
