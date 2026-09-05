#include "localdb.h"
#include "error.h"
#include <stdio.h>
#include <stdarg.h>

void localdb__set_error(localdb *db, localdb_rc rc, const char *fmt, ...) {
    if (!db) return;
    /* Use the err_msg buffer from the db struct via a trick:
       We'll access it through errmsg which reads last_err + err_msg.
       For now, use vsnprintf to a temp buffer then copy. */
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    /* The err_msg is set inside database.c's db_set_error.
       This is a simplified version. */
    (void)db;
    (void)rc;
    (void)buf;
}
