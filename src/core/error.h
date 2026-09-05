#ifndef LOCALDB_ERROR_INTERNAL_H
#define LOCALDB_ERROR_INTERNAL_H

#include "localdb.h"

void localdb__set_error(localdb *db, localdb_rc rc, const char *fmt, ...);

#endif
