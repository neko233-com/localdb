#ifndef LOCALDB_LEXER_H
#define LOCALDB_LEXER_H

typedef enum {
    TOK_EOF = 0,
    /* Keywords */
    TOK_CREATE, TOK_TABLE, TOK_INSERT, TOK_INTO, TOK_VALUES,
    TOK_SELECT, TOK_FROM, TOK_WHERE, TOK_AND, TOK_OR,
    TOK_UPDATE, TOK_SET, TOK_DELETE, TOK_DROP,
    TOK_BEGIN, TOK_COMMIT, TOK_ROLLBACK,
    TOK_NULL, TOK_NOT, TOK_PRIMARY, TOK_KEY,
    TOK_INTEGER, TOK_TEXT, TOK_REAL, TOK_BLOB,
    TOK_ORDER, TOK_BY, TOK_ASC, TOK_DESC,
    TOK_LIMIT, TOK_OFFSET,
    TOK_IF, TOK_EXISTS, TOK_COLLECTION,
    /* Literals */
    TOK_INTEGER_LIT, TOK_FLOAT_LIT, TOK_STRING_LIT,
    /* Identifiers */
    TOK_IDENT,
    /* Symbols */
    TOK_LPAREN, TOK_RPAREN, TOK_COMMA, TOK_SEMICOLON,
    TOK_DOT, TOK_STAR, TOK_EQ, TOK_NE, TOK_LT, TOK_GT,
    TOK_LE, TOK_GE, TOK_ASSIGN,
} tok_type;

typedef struct {
    tok_type    type;
    const char *start;
    int         len;
    int         line;
    int         col;
} token;

typedef struct {
    const char *input;
    const char *pos;
    int         line;
    int         col;
} lexer;

void   lexer_init(lexer *l, const char *sql);
token  lexer_next(lexer *l);
token  lexer_peek(lexer *l);
const char *tok_type_str(tok_type t);

#endif
