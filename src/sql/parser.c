#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Helper: duplicate a token's string */
static char *tok_dup(token t) {
    char *s = (char *)malloc(t.len + 1);
    if (s) {
        memcpy(s, t.start, t.len);
        s[t.len] = '\0';
    }
    return s;
}

/* Helper: check if token matches string (case-insensitive) */
static int tok_eq(token t, const char *s) {
    if ((int)strlen(s) != t.len) return 0;
    for (int i = 0; i < t.len; i++) {
        if (toupper((unsigned char)t.start[i]) != toupper((unsigned char)s[i])) return 0;
    }
    return 1;
}

int parse_sql(const char *sql, sql_stmt *stmt) {
    if (!sql || !stmt) return LOCALDB_ERROR;
    memset(stmt, 0, sizeof(sql_stmt));

    lexer l;
    lexer_init(&l, sql);
    token t = lexer_next(&l);

    /* ── BEGIN / COMMIT / ROLLBACK ── */
    if (t.type == TOK_BEGIN) {
        stmt->type = STMT_BEGIN;
        return LOCALDB_OK;
    }
    if (t.type == TOK_COMMIT) {
        stmt->type = STMT_COMMIT;
        return LOCALDB_OK;
    }
    if (t.type == TOK_ROLLBACK) {
        stmt->type = STMT_ROLLBACK;
        return LOCALDB_OK;
    }

    /* ── CREATE COLLECTION ── */
    if (t.type == TOK_CREATE) {
        t = lexer_next(&l);
        if (t.type == TOK_COLLECTION) {
            stmt->type = STMT_CREATE_COLLECTION;
            t = lexer_next(&l);
            if (t.type == TOK_IF) {
                t = lexer_next(&l);
                if (t.type == TOK_NOT) {
                    t = lexer_next(&l);
                    if (t.type == TOK_EXISTS) {
                        stmt->if_not_exists = 1;
                        t = lexer_next(&l);
                    }
                }
            }
            if (t.type == TOK_IDENT) {
                stmt->table = tok_dup(t);
            }
            return LOCALDB_OK;
        }
        /* CREATE TABLE → treat as CREATE COLLECTION */
        if (t.type == TOK_TABLE) {
            stmt->type = STMT_CREATE_COLLECTION;
            t = lexer_next(&l);
            if (t.type == TOK_IF) {
                t = lexer_next(&l);
                if (t.type == TOK_NOT) {
                    t = lexer_next(&l);
                    if (t.type == TOK_EXISTS) {
                        stmt->if_not_exists = 1;
                        t = lexer_next(&l);
                    }
                }
            }
            if (t.type == TOK_IDENT) {
                stmt->table = tok_dup(t);
            }
            return LOCALDB_OK;
        }
        return LOCALDB_ERROR_SCHEMA;
    }

    /* ── DROP COLLECTION ── */
    if (t.type == TOK_DROP) {
        t = lexer_next(&l);
        if (t.type == TOK_COLLECTION || t.type == TOK_TABLE) {
            stmt->type = STMT_DROP_COLLECTION;
            t = lexer_next(&l);
            if (t.type == TOK_IF) {
                t = lexer_next(&l);
                if (t.type == TOK_EXISTS) {
                    t = lexer_next(&l);
                }
            }
            if (t.type == TOK_IDENT) {
                stmt->table = tok_dup(t);
            }
            return LOCALDB_OK;
        }
        return LOCALDB_ERROR_SCHEMA;
    }

    /* ── INSERT INTO ── */
    if (t.type == TOK_INSERT) {
        stmt->type = STMT_INSERT;
        t = lexer_next(&l);
        if (t.type != TOK_INTO) return LOCALDB_ERROR_SCHEMA;
        t = lexer_next(&l);
        if (t.type != TOK_IDENT) return LOCALDB_ERROR_SCHEMA;
        stmt->table = tok_dup(t);

        /* Parse column list */
        t = lexer_next(&l);
        if (t.type == TOK_LPAREN) {
            /* Count columns */
            lexer saved = l;
            int count = 0;
            while ((t = lexer_next(&l)).type != TOK_RPAREN && t.type != TOK_EOF) {
                if (t.type == TOK_COMMA) continue;
                count++;
            }
            l = saved;

            stmt->columns = (char **)calloc(count, sizeof(char *));
            stmt->values = (char **)calloc(count, sizeof(char *));
            stmt->col_count = count;

            for (int i = 0; i < count; i++) {
                t = lexer_next(&l);
                if (t.type == TOK_COMMA) t = lexer_next(&l);
                stmt->columns[i] = tok_dup(t);
            }
            t = lexer_next(&l); /* ) */
        }

        if (t.type != TOK_VALUES) return LOCALDB_ERROR_SCHEMA;
        t = lexer_next(&l);
        if (t.type != TOK_LPAREN) return LOCALDB_ERROR_SCHEMA;

        /* Parse values */
        int vi = 0;
        while ((t = lexer_next(&l)).type != TOK_RPAREN && t.type != TOK_EOF) {
            if (t.type == TOK_COMMA) continue;
            if (vi < stmt->col_count) {
                stmt->values[vi++] = tok_dup(t);
            }
        }
        return LOCALDB_OK;
    }

    /* ── SELECT ── */
    if (t.type == TOK_SELECT) {
        stmt->type = STMT_SELECT;

        /* Parse select expressions */
        int expr_cap = 4;
        stmt->select_exprs = (sql_expr *)calloc(expr_cap, sizeof(sql_expr));
        stmt->select_count = 0;

        while (1) {
            t = lexer_next(&l);
            if (t.type == TOK_STAR) {
                stmt->select_exprs[stmt->select_count].type = EXPR_STAR;
                stmt->select_count++;
            } else if (t.type == TOK_IDENT) {
                stmt->select_exprs[stmt->select_count].type = EXPR_COLUMN;
                stmt->select_exprs[stmt->select_count].value = tok_dup(t);
                stmt->select_count++;
            }
            t = lexer_next(&l);
            if (t.type != TOK_COMMA) break;
            if (stmt->select_count >= expr_cap) {
                expr_cap *= 2;
                stmt->select_exprs = (sql_expr *)realloc(stmt->select_exprs,
                    expr_cap * sizeof(sql_expr));
            }
        }

        /* FROM */
        if (t.type == TOK_FROM) {
            t = lexer_next(&l);
            if (t.type == TOK_IDENT) {
                stmt->table = tok_dup(t);
            }
            t = lexer_next(&l);
        }

        /* WHERE */
        if (t.type == TOK_WHERE) {
            sql_where **tail = &stmt->where;
            while (1) {
                sql_where *w = (sql_where *)calloc(1, sizeof(sql_where));
                t = lexer_next(&l);
                w->column = tok_dup(t);
                t = lexer_next(&l);
                w->op = t.type;
                t = lexer_next(&l);
                w->value = tok_dup(t);
                *tail = w;
                tail = &w->next;
                t = lexer_next(&l);
                if (t.type != TOK_AND) break;
            }
        }

        /* LIMIT */
        if (t.type == TOK_LIMIT) {
            t = lexer_next(&l);
            if (t.type == TOK_INTEGER_LIT) {
                stmt->limit = atoi(t.start);
            }
            t = lexer_next(&l);
        }

        return LOCALDB_OK;
    }

    /* ── DELETE FROM ── */
    if (t.type == TOK_DELETE) {
        stmt->type = STMT_DELETE;
        t = lexer_next(&l);
        if (t.type == TOK_FROM) t = lexer_next(&l);
        if (t.type == TOK_IDENT) stmt->table = tok_dup(t);

        t = lexer_next(&l);
        if (t.type == TOK_WHERE) {
            stmt->where = (sql_where *)calloc(1, sizeof(sql_where));
            t = lexer_next(&l);
            stmt->where->column = tok_dup(t);
            t = lexer_next(&l);
            stmt->where->op = t.type;
            t = lexer_next(&l);
            stmt->where->value = tok_dup(t);
        }
        return LOCALDB_OK;
    }

    /* ── UPDATE ── */
    if (t.type == TOK_UPDATE) {
        stmt->type = STMT_UPDATE;
        t = lexer_next(&l);
        if (t.type == TOK_IDENT) stmt->table = tok_dup(t);
        /* TODO: parse SET clause */
        return LOCALDB_OK;
    }

    return LOCALDB_ERROR_SCHEMA;
}

void sql_stmt_free(sql_stmt *stmt) {
    if (!stmt) return;
    free(stmt->table);
    for (int i = 0; i < stmt->col_count; i++) {
        free(stmt->columns[i]);
        free(stmt->values[i]);
    }
    free(stmt->columns);
    free(stmt->values);
    for (int i = 0; i < stmt->select_count; i++) {
        free(stmt->select_exprs[i].value);
    }
    free(stmt->select_exprs);
    sql_where *w = stmt->where;
    while (w) {
        sql_where *next = w->next;
        free(w->column);
        free(w->value);
        free(w);
        w = next;
    }
}
