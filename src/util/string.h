#ifndef LOCALDB_UTIL_STRING_H
#define LOCALDB_UTIL_STRING_H

#include <stddef.h>

/** Duplicate a string. Caller must free. */
char *localdb_strdup(const char *s);

/** Duplicate a string with max length. Caller must free. */
char *localdb_strndup(const char *s, size_t n);

#endif
