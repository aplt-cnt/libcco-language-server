#ifndef CCO_TEXT_DOCUMENT_H
#define CCO_TEXT_DOCUMENT_H

#include "lsp.h"
#include <stddef.h>

typedef struct {
    char   *uri;
    char   *text;
    size_t  line_count;
    size_t *line_offsets; /* offset of each line in text */
    size_t  line_cap;
} text_document_t;

text_document_t *text_document_new(const char *uri, const char *text);
void             text_document_free(text_document_t *doc);
void             text_document_set_text(text_document_t *doc, const char *text);
void             text_document_apply_change(text_document_t *doc,
                                            lsp_range_t range,
                                            const char *new_text);

const char *text_document_line(const text_document_t *doc, size_t line, size_t *len);
size_t      text_document_offset(const text_document_t *doc, size_t line, size_t col);

#endif
