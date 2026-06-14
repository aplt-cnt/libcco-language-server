#ifndef CCO_ANALYZE_H
#define CCO_ANALYZE_H

#include "lsp.h"
#include "text_document.h"
#include <stddef.h>

typedef struct {
    char **names;
    int   *kinds; /* LSP SymbolKind */
    size_t count;
    size_t cap;
} cco_symbol_list_t;

/* Full document analysis: returns diagnostics array + count */
lsp_diagnostic_t *cco_analyze_document(const text_document_t *doc, int *out_count);

/* Hover: returns markdown string (caller must free) */
char *cco_analyze_hover(const text_document_t *doc, lsp_position_t pos);

/* Definition: returns URI + range if found, otherwise returns NULL range */
int cco_analyze_definition(const text_document_t *doc, lsp_position_t pos,
                           char **out_uri, lsp_range_t *out_range);

/* Completions at a position */
lsp_completion_item_t *cco_analyze_completions(const text_document_t *doc,
                                                lsp_position_t pos, int *out_count);

/* Document symbols (outline) */
cco_symbol_list_t *cco_analyze_document_symbols(const text_document_t *doc);
void               cco_symbol_list_free(cco_symbol_list_t *list);

/* Get word/ident at position; caller must free result */
char *cco_get_word_at(const text_document_t *doc, lsp_position_t pos);

#endif
