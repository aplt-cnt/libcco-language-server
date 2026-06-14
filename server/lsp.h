#ifndef CCO_LSP_H
#define CCO_LSP_H

#include "json.h"
#include <stddef.h>

/* --- Protocol framing --------------------------------------------------- */
typedef struct {
    char       *data;
    size_t      len;
    size_t      cap;
} lsp_buffer_t;

int   lsp_read_message(void *stream, char **out_body, size_t *out_len);
void  lsp_write_message(void *stream, const char *body);
void  lsp_write_notification(void *stream, const char *method, json_value_t *params);
void  lsp_write_response(void *stream, json_value_t *id, json_value_t *result);
void  lsp_write_error(void *stream, json_value_t *id, int code, const char *msg);

/* --- LSP types ----------------------------------------------------------- */
typedef struct {
    long long line;
    long long character;
} lsp_position_t;

typedef struct {
    lsp_position_t start;
    lsp_position_t end;
} lsp_range_t;

typedef struct {
    lsp_range_t  range;
    const char  *message;
    int          severity; /* 1=error, 2=warning, 3=info, 4=hint */
} lsp_diagnostic_t;

typedef struct {
    char *label;
    int   kind; /* 1=Text, 2=Method, 3=Function, 4=Constructor, ...
                   9=Keyword, 10=Snippet, 12=Variable, 13=Class,
                   14=Interface, 15=Module, 17=Property, ...
                   21=Enum, 22=EnumMember, 23=TypeParameter */
    char *detail;
    char *insert_text;
} lsp_completion_item_t;

/* --- Server capabilities ------------------------------------------------- */
typedef struct {
    int text_document_sync_kind; /* 0=None, 1=Full, 2=Incremental */
    int completion_provider;
    int hover_provider;
    int definition_provider;
    int document_symbol_provider;
} lsp_capabilities_t;

/* --- Handlers ------------------------------------------------------------ */
typedef void (*lsp_handler_fn)(json_value_t *params, json_value_t *id,
                               void *stream, void *ctx);

typedef struct {
    const char    *method;
    lsp_handler_fn handler;
} lsp_handler_entry_t;

/* --- Helpers ------------------------------------------------------------- */
lsp_position_t lsp_position_from_json(json_value_t *v);
lsp_range_t    lsp_range_from_json(json_value_t *v);

json_value_t *lsp_position_to_json(lsp_position_t pos);
json_value_t *lsp_range_to_json(lsp_range_t r);
json_value_t *lsp_diagnostic_to_json(lsp_diagnostic_t d);
json_value_t *lsp_completion_item_to_json(lsp_completion_item_t item);

void lsp_diag_free(lsp_diagnostic_t *d);

#endif
