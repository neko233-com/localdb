#ifndef LOCALDB_PARSER_H
#define LOCALDB_PARSER_H

#include "lexer.h"
#include "localdb.h"

typedef enum {
    STMT_CREATE_COLLECTION,
    STMT_DROP_COLLECTION,
    STMT_INSERT,
    STMT_SELECT,
    STMT_UPDATE,
    STMT_DELETE,
    STMT_BEGIN,
    STMT_COMMIT,
    STMT_ROLLBACK,
} stmt_type;

typedef struct sql_expr {
    enum { EXPR_COLUMN, EXPR_STRING, EXPR_NUMBER, EXPR_STAR } type;
    char  *value;
    double num_val;
} sql_expr;

typedef struct sql_where {
    char            *column;
    tok_type         op;      /* TOK_EQ, TOK_NE, etc. */
    char            *value;
    struct sql_where *next;   /* AND chain */
} sql_where;

typedef struct sql_stmt {
    stmt_type   type;
    char       *table;      /* collection name */
    /* INSERT */
    char      **columns;
    char      **values;
    int         col_count;
    /* SELECT */
    sql_expr   *select_exprs;
    int         select_count;
    sql_where  *where;
    int         limit;
    int         offset;
    /* CREATE */
    int         if_not_exists;
} sql_stmt;

int parse_sql(const char *sql, sql_stmt *stmt);
void sql_stmt_free(sql_stmt *stmt);

#endif
