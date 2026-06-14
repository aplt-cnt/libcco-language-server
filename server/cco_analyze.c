#include "cco_analyze.h"
#include <cnt/cco.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ---- Helpers ----------------------------------------------------------- */

static int is_ident_char(int c) {
    return isalnum(c) || c == '_' || c == '$' || c == '.';
}

char *cco_get_word_at(const text_document_t *doc, lsp_position_t pos) {
    size_t offset = text_document_offset(doc, (size_t)pos.line, (size_t)pos.character);
    const char *text = doc->text;
    size_t len = strlen(text);

    /* Expand backward */
    size_t start = offset;
    while (start > 0 && is_ident_char((unsigned char)text[start - 1])) start--;

    /* Expand forward */
    size_t end = offset;
    while (end < len && is_ident_char((unsigned char)text[end])) end++;

    if (start >= end) return NULL;
    char *word = (char*)malloc(end - start + 1);
    if (!word) return NULL;
    memcpy(word, text + start, end - start);
    word[end - start] = '\0';
    return word;
}

/* Check if word is at the given position (exact word match) */
static int is_word_at(const text_document_t *doc, lsp_position_t pos, const char *word) {
    char *w = cco_get_word_at(doc, pos);
    if (!w) return 0;
    int r = (strcmp(w, word) == 0);
    free(w);
    return r;
}

/* ---- Diagnostics ------------------------------------------------------- */

lsp_diagnostic_t *cco_analyze_document(const text_document_t *doc, int *out_count) {
    *out_count = 0;
    if (!doc || !doc->text) return NULL;

    cco_clear_error();
    cco_parse_result_t *res = cco_parse_full(doc->text);

    int n = cco_diag_count();
    if (n == 0) {
        cco_parse_result_free(res);
        return NULL;
    }

    lsp_diagnostic_t *diags = (lsp_diagnostic_t*)calloc((size_t)n, sizeof(lsp_diagnostic_t));
    if (!diags) { cco_parse_result_free(res); return NULL; }

    for (int i = 0; i < n; i++) {
        const cco_diag_t *cd = cco_diag_get(i);
        if (!cd) continue;
        diags[*out_count].range.start.line      = (long long)(cd->line > 0 ? cd->line - 1 : 0);
        diags[*out_count].range.start.character = (long long)(cd->col > 0 ? cd->col - 1 : 0);
        diags[*out_count].range.end.line        = diags[*out_count].range.start.line;
        diags[*out_count].range.end.character   = diags[*out_count].range.start.character + 1;
        diags[*out_count].severity = cd->is_error ? 1 : 2;
        diags[*out_count].message  = cd->message;
        (*out_count)++;
    }

    cco_parse_result_free(res);
    return diags;
}

/* ---- Hover ------------------------------------------------------------- */

char *cco_analyze_hover(const text_document_t *doc, lsp_position_t pos) {
    if (!doc || !doc->text) return NULL;

    char *word = cco_get_word_at(doc, pos);
    if (!word) return NULL;

    /* Try to parse fully to get symbol table */
    cco_parse_result_t *res = cco_parse_full(doc->text);
    if (!res) { free(word); return NULL; }

    cco_symbol_table_t *st = cco_parse_result_symbols(res);
    char *result = NULL;

    /* Check for $-keywords */
    if (word[0] == '$') {
        const char *desc = NULL;
        if (strcmp(word, "$typedef")  == 0) desc = "Define a type alias";
        else if (strcmp(word, "$enum") == 0) desc = "Define an enumeration";
        else if (strcmp(word, "$temp") == 0) desc = "Define a template (class)";
        else if (strcmp(word, "$function") == 0) desc = "Define a constructor or static method";
        else if (strcmp(word, "$default") == 0) desc = "Use default constructor";
        else if (strcmp(word, "$return") == 0) desc = "Return a value from a method";
        else if (strcmp(word, "$this") == 0) desc = "Reference to `this` (static method context)";
        else if (strcmp(word, "$format") == 0) desc = "Parse a string as CCO at runtime";
        if (desc) {
            size_t n = strlen(desc) + 32;
            result = (char*)malloc(n);
            if (result) snprintf(result, n, "```cco\n%s\n```\n%s", word, desc);
        }
        cco_parse_result_free(res);
        free(word);
        return result;
    }

    /* Check type keywords */
    if (strcmp(word, "String")  == 0 ||
        strcmp(word, "Integer") == 0 ||
        strcmp(word, "Float")   == 0 ||
        strcmp(word, "Boolean") == 0 ||
        strcmp(word, "Array")   == 0 ||
        strcmp(word, "dyn")     == 0 ||
        strcmp(word, "None")    == 0) {
        size_t n = strlen(word) + 48;
        result = (char*)malloc(n);
        if (result) snprintf(result, n, "```cco\n%s\n```\nPrimitive type keyword", word);
        cco_parse_result_free(res);
        free(word);
        return result;
    }

    if (!st) { cco_parse_result_free(res); free(word); return NULL; }

    /* Check typedefs */
    cco_type_expr_t *te = cco_symtab_get_typedef(st, word);
    if (te) {
        result = (char*)malloc(256);
        if (result) snprintf(result, 256, "```cco\n$typedef.%s: <type>\n```\nType alias", word);
        cco_parse_result_free(res);
        free(word);
        return result;
    }

    /* Check enums and enum values */
    size_t sc = cco_symtab_get_count(st);
    for (size_t i = 0; i < sc; i++) {
        const char *ename = cco_symtab_get_name(st, i);
        if (!ename) continue;
        const char **vals; size_t vc;
        if (cco_symtab_get_enum_values(st, ename, &vals, &vc)) {
            /* Check if word is an enum value */
            for (size_t j = 0; j < vc; j++) {
                if (vals[j] && strcmp(vals[j], word) == 0) {
                    result = (char*)malloc(256);
                    if (result) snprintf(result, 256, "```cco\n%s\n```\nValue of `%s` enum (index %zu)", word, ename, j);
                    cco_parse_result_free(res);
                    free(word);
                    return result;
                }
            }
            /* Check if word is the enum name */
            if (strcmp(ename, word) == 0) {
                result = (char*)malloc(512);
                if (result) {
                    char buf[512]; size_t pos2 = 0;
                    pos2 += snprintf(buf + pos2, sizeof(buf) - pos2, "```cco\n$enum.%s = (", word);
                    for (size_t j = 0; j < vc; j++) {
                        if (j > 0) pos2 += snprintf(buf + pos2, sizeof(buf) - pos2, ", ");
                        pos2 += snprintf(buf + pos2, sizeof(buf) - pos2, "%s", vals[j] ? vals[j] : "?");
                    }
                    snprintf(buf + pos2, sizeof(buf) - pos2, ")\n```\nEnumeration");
                    result = strdup(buf);
                }
                cco_parse_result_free(res);
                free(word);
                return result;
            }
        }

        /* Check templates */
        cco_template_t *tmpl = cco_symtab_get_template(st, ename);
        if (tmpl && strcmp(ename, word) == 0) {
            const char *parent = cco_template_get_parent_name(tmpl);
            size_t nf = cco_template_field_count(tmpl);
            char buf[1024]; size_t pos2 = 0;
            pos2 += snprintf(buf + pos2, sizeof(buf) - pos2, "```cco\n$temp.%s", word);
            if (parent) pos2 += snprintf(buf + pos2, sizeof(buf) - pos2, " + %s", parent);
            pos2 += snprintf(buf + pos2, sizeof(buf) - pos2, ": (\n");
            for (size_t j = 0; j < nf; j++) {
                const char *fn = cco_template_field_name(tmpl, j);
                if (fn) pos2 += snprintf(buf + pos2, sizeof(buf) - pos2, "    %s: ?,\n", fn);
            }
            snprintf(buf + pos2, sizeof(buf) - pos2, ")\n```\nTemplate");
            result = strdup(buf);
            cco_parse_result_free(res);
            free(word);
            return result;
        }
    }

    cco_parse_result_free(res);
    free(word);
    return NULL;
}

/* ---- Definition -------------------------------------------------------- */

int cco_analyze_definition(const text_document_t *doc, lsp_position_t pos,
                            char **out_uri, lsp_range_t *out_range) {
    if (!doc || !doc->text) return 0;
    char *word = cco_get_word_at(doc, pos);
    if (!word) return 0;

    /* Parse full to get symbol table and line info */
    cco_parse_result_t *res = cco_parse_full(doc->text);
    if (!res) { free(word); return 0; }
    cco_symbol_table_t *st = cco_parse_result_symbols(res);

    /* Search for declarations in source text */
    const char *text = doc->text;
    size_t len = strlen(text);

    /* Try to find $typedef.word, $enum.word, $temp.word in source */
    char patterns[3][64];
    snprintf(patterns[0], sizeof(patterns[0]), "$typedef.%s", word);
    snprintf(patterns[1], sizeof(patterns[1]), "$enum.%s",    word);
    snprintf(patterns[2], sizeof(patterns[2]), "$temp.%s",    word);

    for (int pi = 0; pi < 3; pi++) {
        const char *found = strstr(text, patterns[pi]);
        if (found) {
            size_t offset = (size_t)(found - text);
            /* Compute line/col */
            size_t line = 0, col = 0;
            for (size_t i = 0; i < offset && i < len; i++) {
                if (text[i] == '\n') { line++; col = 0; }
                else col++;
            }
            if (out_uri) *out_uri = strdup(doc->uri);
            if (out_range) {
                out_range->start.line      = (long long)line;
                out_range->start.character = (long long)col;
                /* Find end of declaration */
                size_t end = offset;
                while (end < len && text[end] != '\n' && text[end] != '\r') end++;
                size_t end_col = col + (end - offset);
                out_range->end.line      = (long long)line;
                out_range->end.character = (long long)end_col;
            }
            cco_parse_result_free(res);
            free(word);
            return 1;
        }
    }

    /* For enum values, find the enum definition */
    if (st) {
        size_t sc = cco_symtab_get_count(st);
        for (size_t i = 0; i < sc; i++) {
            const char *ename = cco_symtab_get_name(st, i);
            if (!ename) continue;
            const char **vals; size_t vc;
            if (cco_symtab_get_enum_values(st, ename, &vals, &vc)) {
                for (size_t j = 0; j < vc; j++) {
                    if (vals[j] && strcmp(vals[j], word) == 0) {
                        /* Found enum value — jump to enum definition */
                        char pat[64];
                        snprintf(pat, sizeof(pat), "$enum.%s", ename);
                        const char *found = strstr(text, pat);
                        if (found) {
                            size_t offset = (size_t)(found - text);
                            size_t line = 0, col = 0;
                            for (size_t k = 0; k < offset && k < len; k++) {
                                if (text[k] == '\n') { line++; col = 0; }
                                else col++;
                            }
                            if (out_uri) *out_uri = strdup(doc->uri);
                            if (out_range) {
                                out_range->start.line      = (long long)line;
                                out_range->start.character = (long long)col;
                                out_range->end.line        = (long long)line;
                                out_range->end.character   = (long long)(col + strlen(pat));
                            }
                            cco_parse_result_free(res);
                            free(word);
                            return 1;
                        }
                        break;
                    }
                }
            }
        }
    }

    cco_parse_result_free(res);
    free(word);
    return 0;
}

/* ---- Completions ------------------------------------------------------- */

/* Get the character just before the cursor on the same line */
static char char_before(const text_document_t *doc, lsp_position_t pos) {
    size_t offset = text_document_offset(doc, (size_t)pos.line, (size_t)pos.character);
    if (offset == 0) return '\0';
    return doc->text[offset - 1];
}

/* Get text from beginning of line to cursor */
static char *text_before_cursor(const text_document_t *doc, lsp_position_t pos) {
    size_t offset = text_document_offset(doc, (size_t)pos.line, (size_t)pos.character);
    size_t line_start = doc->line_offsets[(size_t)pos.line];
    size_t len = offset - line_start;
    char *buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, doc->text + line_start, len);
    buf[len] = '\0';
    return buf;
}

lsp_completion_item_t *cco_analyze_completions(const text_document_t *doc,
                                                lsp_position_t pos, int *out_count) {
    *out_count = 0;
    if (!doc || !doc->text) return NULL;

    /* Determine context by looking at char before cursor */
    char before = char_before(doc, pos);
    char *prefix = text_before_cursor(doc, pos);

    /* Find the start of the current word */
    const char *p = prefix ? prefix + strlen(prefix) : "";
    while (p > (prefix ? prefix : "") && is_ident_char((unsigned char)p[-1])) p--;
    size_t word_len = (prefix ? strlen(prefix) : 0) - (size_t)(p - (prefix ? prefix : ""));
    const char *word_start = p;

    /* Parse to get symbol table for template names etc. */
    cco_parse_result_t *res = cco_parse_full(doc->text);
    cco_symbol_table_t *st = res ? cco_parse_result_symbols(res) : NULL;

    size_t cap = 32, cnt = 0;
    lsp_completion_item_t *items = (lsp_completion_item_t*)calloc(cap, sizeof(lsp_completion_item_t));
    if (!items) { free(prefix); cco_parse_result_free(res); return NULL; }

    int dollar_context = (before == '$');
    int hash_context   = (before == '#');
    int colon_context  = (before == ':' || (word_len > 0 && memcmp(word_start, "String", 6) == 0));

    /* Check if we're after $ */
    if (dollar_context) {
        /* $-keyword completions */
        struct { const char *name; const char *detail; int kind; } kw[] = {
            {"typedef",  "Define a type alias",                       15},
            {"enum",     "Define an enumeration",                     15},
            {"temp",     "Define a template (class)",                 15},
            {"function", "Define constructor or static method",       15},
            {"default",  "Use default constructor",                   15},
            {"return",   "Return from a method",                      15},
            {"this",     "Reference to this (static context)",        15},
            {"format",   "Parse string as CCO at runtime",            15},
        };
        for (size_t i = 0; i < sizeof(kw)/sizeof(kw[0]); i++) {
            if (cnt >= cap) { cap *= 2; items = (lsp_completion_item_t*)realloc(items, cap * sizeof(lsp_completion_item_t)); }
            items[cnt].label = strdup(kw[i].name);
            items[cnt].kind  = 14; /* Keyword */
            items[cnt].detail = strdup(kw[i].detail);
            items[cnt].insert_text = NULL;
            cnt++;
        }
    }

    /* After #: template names */
    if (hash_context && st) {
        size_t sc = cco_symtab_get_count(st);
        for (size_t i = 0; i < sc; i++) {
            const char *name = cco_symtab_get_name(st, i);
            if (!name) continue;
            cco_template_t *tmpl = cco_symtab_get_template(st, name);
            if (tmpl) {
                if (cnt >= cap) { cap *= 2; items = (lsp_completion_item_t*)realloc(items, cap * sizeof(lsp_completion_item_t)); }
                items[cnt].label = strdup(name);
                items[cnt].kind  = 13; /* Class */
                items[cnt].detail = strdup("Template");
                items[cnt].insert_text = NULL;
                cnt++;
            }
        }
    }

    /* After ':' or type context: type keywords + known types */
    if (colon_context || hash_context || dollar_context == 0) {
        struct { const char *name; const char *detail; int kind; } type_kw[] = {
            {"String",  "String type",  22},
            {"Integer", "Integer type", 22},
            {"Float",   "Float type",   22},
            {"Boolean", "Boolean type", 22},
            {"Array",   "Array type",   22},
            {"dyn",     "Dynamic dispatch type", 22},
            {"None",    "None type",    22},
        };
        for (size_t i = 0; i < sizeof(type_kw)/sizeof(type_kw[0]); i++) {
            if (cnt >= cap) { cap *= 2; items = (lsp_completion_item_t*)realloc(items, cap * sizeof(lsp_completion_item_t)); }
            items[cnt].label = strdup(type_kw[i].name);
            items[cnt].kind  = type_kw[i].kind;
            items[cnt].detail = strdup(type_kw[i].detail);
            items[cnt].insert_text = NULL;
            cnt++;
        }

        /* Type aliases and enum names from symbol table */
        if (st) {
            size_t sc = cco_symtab_get_count(st);
            for (size_t i = 0; i < sc; i++) {
                const char *name = cco_symtab_get_name(st, i);
                if (!name) continue;
                if (cnt >= cap) { cap *= 2; items = (lsp_completion_item_t*)realloc(items, cap * sizeof(lsp_completion_item_t)); }
                int kind = 22;
                const char *det = "Type";
                if (cco_symtab_get_template(st, name)) {
                    kind = 13; /* Class */
                    det  = "Template";
                } else {
                    const char **vals; size_t vc;
                    if (cco_symtab_get_enum_values(st, name, &vals, &vc))
                        det = "Enum";
                    else
                        det = "Type alias";
                }
                items[cnt].label = strdup(name);
                items[cnt].kind  = kind;
                items[cnt].detail = strdup(det);
                items[cnt].insert_text = NULL;
                cnt++;
            }
        }
    }

    free(prefix);
    cco_parse_result_free(res);
    *out_count = (int)cnt;
    return items;
}

/* ---- Document Symbols -------------------------------------------------- */

cco_symbol_list_t *cco_analyze_document_symbols(const text_document_t *doc) {
    cco_symbol_list_t *list = (cco_symbol_list_t*)calloc(1, sizeof(cco_symbol_list_t));
    if (!list) return NULL;

    if (!doc || !doc->text) return list;

    const char *text = doc->text;
    size_t len = strlen(text);

    /* Scan for $typedef.Name, $enum.Name, $temp.Name */
    const char *p = text;
    while (p && p < text + len) {
        const char *dollar = strchr(p, '$');
        if (!dollar) break;

        /* Check if followed by typedef/enum/temp */
        const char *after = dollar + 1;
        const char *dot   = NULL;
        int kind = 0;

        if (strncmp(after, "typedef", 7) == 0) {
            dot = after + 7; kind = 22; /* TypeAlias */
        } else if (strncmp(after, "enum", 4) == 0) {
            dot = after + 4; kind = 21; /* Enum */
        } else if (strncmp(after, "temp", 4) == 0) {
            dot = after + 4; kind = 13; /* Class */
        }

        if (dot && *dot == '.') {
            const char *name_start = dot + 1;
            const char *name_end = name_start;
            while (is_ident_char((unsigned char)*name_end) && *name_end != '.') name_end++;
            if (name_end > name_start) {
                if (list->count >= list->cap) {
                    list->cap = list->cap ? list->cap * 2 : 16;
                    list->names = (char**)realloc(list->names, list->cap * sizeof(char*));
                    list->kinds = (int*)realloc(list->kinds, list->cap * sizeof(int));
                }
                list->names[list->count] = (char*)malloc((size_t)(name_end - name_start) + 1);
                if (list->names[list->count]) {
                    memcpy(list->names[list->count], name_start, (size_t)(name_end - name_start));
                    list->names[list->count][name_end - name_start] = '\0';
                }
                list->kinds[list->count] = kind;
                list->count++;
            }
            p = name_end;
        } else {
            p = dollar + 1;
        }
    }

    return list;
}

void cco_symbol_list_free(cco_symbol_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++)
        free(list->names[i]);
    free(list->names);
    free(list->kinds);
    free(list);
}
