#include "lexer.h"
#include <ctype.h>
#include <string.h>

typedef struct { const char *word; tok_type type; } keyword;
static const keyword keywords[] = {
    {"CREATE", TOK_CREATE}, {"TABLE", TOK_TABLE},
    {"INSERT", TOK_INSERT}, {"INTO", TOK_INTO}, {"VALUES", TOK_VALUES},
    {"SELECT", TOK_SELECT}, {"FROM", TOK_FROM}, {"WHERE", TOK_WHERE},
    {"AND", TOK_AND}, {"OR", TOK_OR},
    {"UPDATE", TOK_UPDATE}, {"SET", TOK_SET},
    {"DELETE", TOK_DELETE}, {"DROP", TOK_DROP},
    {"BEGIN", TOK_BEGIN}, {"COMMIT", TOK_COMMIT}, {"ROLLBACK", TOK_ROLLBACK},
    {"NULL", TOK_NULL}, {"NOT", TOK_NOT},
    {"PRIMARY", TOK_PRIMARY}, {"KEY", TOK_KEY},
    {"INTEGER", TOK_INTEGER}, {"TEXT", TOK_TEXT},
    {"REAL", TOK_REAL}, {"BLOB", TOK_BLOB},
    {"ORDER", TOK_ORDER}, {"BY", TOK_BY},
    {"ASC", TOK_ASC}, {"DESC", TOK_DESC},
    {"LIMIT", TOK_LIMIT}, {"OFFSET", TOK_OFFSET},
    {"IF", TOK_IF}, {"EXISTS", TOK_EXISTS},
    {"COLLECTION", TOK_COLLECTION},
    {NULL, TOK_EOF}
};

void lexer_init(lexer *l, const char *sql) {
    l->input = sql;
    l->pos = sql;
    l->line = 1;
    l->col = 1;
}

static void skip_whitespace(lexer *l) {
    while (*l->pos && (*l->pos == ' ' || *l->pos == '\t' || *l->pos == '\n' || *l->pos == '\r')) {
        if (*l->pos == '\n') { l->line++; l->col = 1; }
        else l->col++;
        l->pos++;
    }
}

static token make_token(lexer *l, tok_type type, const char *start, int len) {
    token t;
    t.type = type;
    t.start = start;
    t.len = len;
    t.line = l->line;
    t.col = l->col;
    return t;
}

token lexer_next(lexer *l) {
    skip_whitespace(l);

    if (*l->pos == '\0') return make_token(l, TOK_EOF, l->pos, 0);

    const char *start = l->pos;

    /* Single-char tokens */
    switch (*l->pos) {
        case '(': l->pos++; l->col++; return make_token(l, TOK_LPAREN, start, 1);
        case ')': l->pos++; l->col++; return make_token(l, TOK_RPAREN, start, 1);
        case ',': l->pos++; l->col++; return make_token(l, TOK_COMMA, start, 1);
        case ';': l->pos++; l->col++; return make_token(l, TOK_SEMICOLON, start, 1);
        case '.': l->pos++; l->col++; return make_token(l, TOK_DOT, start, 1);
        case '*': l->pos++; l->col++; return make_token(l, TOK_STAR, start, 1);
        case '=':
            l->pos++; l->col++;
            return make_token(l, TOK_EQ, start, 1);
        case '!':
            if (l->pos[1] == '=') {
                l->pos += 2; l->col += 2;
                return make_token(l, TOK_NE, start, 2);
            }
            break;
        case '<':
            l->pos++; l->col++;
            if (*l->pos == '=') { l->pos++; l->col++; return make_token(l, TOK_LE, start, 2); }
            return make_token(l, TOK_LT, start, 1);
        case '>':
            l->pos++; l->col++;
            if (*l->pos == '=') { l->pos++; l->col++; return make_token(l, TOK_GE, start, 2); }
            return make_token(l, TOK_GT, start, 1);
    }

    /* String literal */
    if (*l->pos == '\'' || *l->pos == '"') {
        char quote = *l->pos;
        l->pos++; l->col++;
        while (*l->pos && *l->pos != quote) {
            if (*l->pos == '\\') { l->pos++; l->col++; }
            l->pos++; l->col++;
        }
        if (*l->pos == quote) { l->pos++; l->col++; }
        return make_token(l, TOK_STRING_LIT, start + 1, (int)(l->pos - start - 2));
    }

    /* Number */
    if (isdigit(*l->pos)) {
        int is_float = 0;
        while (isdigit(*l->pos) || *l->pos == '.') {
            if (*l->pos == '.') is_float++;
            l->pos++; l->col++;
        }
        return make_token(l, is_float ? TOK_FLOAT_LIT : TOK_INTEGER_LIT,
                          start, (int)(l->pos - start));
    }

    /* Identifier or keyword */
    if (isalpha(*l->pos) || *l->pos == '_') {
        while (isalnum(*l->pos) || *l->pos == '_') {
            l->pos++; l->col++;
        }
        int len = (int)(l->pos - start);

        /* Check if keyword (case-insensitive) */
        for (const keyword *kw = keywords; kw->word; kw++) {
            if ((int)strlen(kw->word) == len) {
                int match = 1;
                for (int i = 0; i < len; i++) {
                    if (toupper((unsigned char)start[i]) != toupper((unsigned char)kw->word[i])) {
                        match = 0; break;
                    }
                }
                if (match) return make_token(l, kw->type, start, len);
            }
        }
        return make_token(l, TOK_IDENT, start, len);
    }

    /* Unknown char — skip */
    l->pos++; l->col++;
    return lexer_next(l);
}

token lexer_peek(lexer *l) {
    lexer saved = *l;
    token t = lexer_next(l);
    *l = saved;
    return t;
}

const char *tok_type_str(tok_type t) {
    switch (t) {
        case TOK_EOF: return "EOF";
        case TOK_CREATE: return "CREATE";
        case TOK_SELECT: return "SELECT";
        case TOK_INSERT: return "INSERT";
        case TOK_IDENT: return "IDENT";
        case TOK_STRING_LIT: return "STRING";
        case TOK_INTEGER_LIT: return "INTEGER_LIT";
        default: return "TOKEN";
    }
}
