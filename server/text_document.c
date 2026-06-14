#include "text_document.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

text_document_t *text_document_new(const char *uri, const char *text) {
    text_document_t *doc = (text_document_t*)calloc(1, sizeof(text_document_t));
    if (!doc) return NULL;
    doc->uri = strdup(uri ? uri : "");
    doc->line_cap = 64;
    doc->line_offsets = (size_t*)malloc(doc->line_cap * sizeof(size_t));
    if (!doc->line_offsets) { free(doc->uri); free(doc); return NULL; }
    text_document_set_text(doc, text ? text : "");
    return doc;
}

void text_document_free(text_document_t *doc) {
    if (!doc) return;
    free(doc->uri);
    free(doc->text);
    free(doc->line_offsets);
    free(doc);
}

void text_document_set_text(text_document_t *doc, const char *text) {
    if (!doc) return;
    free(doc->text);
    doc->text = strdup(text);
    doc->line_count = 0;

    /* Build line offset index */
    doc->line_offsets[0] = 0;
    doc->line_count = 1;
    for (size_t i = 0; text[i]; i++) {
        if (text[i] == '\n') {
            if (doc->line_count >= doc->line_cap) {
                doc->line_cap *= 2;
                doc->line_offsets = (size_t*)realloc(doc->line_offsets,
                                                     doc->line_cap * sizeof(size_t));
            }
            doc->line_offsets[doc->line_count++] = i + 1;
        }
    }
}

void text_document_apply_change(text_document_t *doc, lsp_range_t range,
                                 const char *new_text) {
    (void)range;
    /* For simplicity, do full sync: just replace text */
    text_document_set_text(doc, new_text);
}

const char *text_document_line(const text_document_t *doc, size_t line, size_t *len) {
    if (!doc || line >= doc->line_count) return NULL;
    size_t start = doc->line_offsets[line];
    size_t end   = (line + 1 < doc->line_count)
                   ? doc->line_offsets[line + 1]
                   : strlen(doc->text);
    if (end > start && doc->text[end - 1] == '\n') end--;
    if (end > start && doc->text[end - 1] == '\r') end--;
    if (len) *len = end - start;
    return doc->text + start;
}

size_t text_document_offset(const text_document_t *doc, size_t line, size_t col) {
    if (!doc || line >= doc->line_count) return 0;
    size_t base = doc->line_offsets[line];
    size_t line_len;
    const char *ln = text_document_line(doc, line, &line_len);
    if (!ln) return base;
    if (col > line_len) col = line_len;
    return base + col;
}
