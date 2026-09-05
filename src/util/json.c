#include "json.h"
#include <stdlib.h>
#include <string.h>

/* Simple JSON validation — checks balanced braces/brackets and basic structure */
int localdb_json_validate(const char *json) {
    if (!json) return 0;
    if (*json != '{' && *json != '[') return 0;

    int depth = 0;
    int in_string = 0;
    const char *p = json;

    while (*p) {
        if (*p == '"' && (p == json || *(p - 1) != '\\')) {
            in_string = !in_string;
        } else if (!in_string) {
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']') {
                depth--;
                if (depth < 0) return 0;
            }
        }
        p++;
    }
    return depth == 0 && !in_string;
}

int localdb_json_get(const char *json, const char *path, char **out) {
    if (!json || !path || !out) return LOCALDB_ERROR;
    *out = NULL;

    /* Simple top-level key lookup: find "path": in the JSON */
    size_t path_len = strlen(path);
    /* Build search pattern: "path" */
    char *pattern = (char *)malloc(path_len + 4);
    if (!pattern) return LOCALDB_ERROR_NOMEM;
    pattern[0] = '"';
    memcpy(pattern + 1, path, path_len);
    pattern[path_len + 1] = '"';
    pattern[path_len + 2] = ':';
    pattern[path_len + 3] = '\0';

    const char *found = strstr(json, pattern);
    free(pattern);
    if (!found) return LOCALDB_ERROR_NOTFOUND;

    found += path_len + 3; /* skip "key": */
    while (*found == ' ' || *found == '\t') found++;

    /* Extract value */
    const char *start = found;
    const char *end;
    if (*found == '"') {
        /* String value */
        start = found + 1;
        end = start;
        while (*end && !(*end == '"' && *(end - 1) != '\\')) end++;
    } else if (*found == '{' || *found == '[') {
        /* Object/array — find matching close */
        int depth = 0;
        end = found;
        do {
            if (*end == '{' || *end == '[') depth++;
            else if (*end == '}' || *end == ']') depth--;
            end++;
        } while (*end && depth > 0);
    } else {
        /* Number, bool, null — until comma or close */
        end = start;
        while (*end && *end != ',' && *end != '}' && *end != ']') end++;
        while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t')) end--;
    }

    size_t len = (size_t)(end - start);
    *out = (char *)malloc(len + 1);
    if (!*out) return LOCALDB_ERROR_NOMEM;
    memcpy(*out, start, len);
    (*out)[len] = '\0';
    return LOCALDB_OK;
}

int localdb_json_len(const char *json) {
    return json ? (int)strlen(json) : 0;
}
