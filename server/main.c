#include "cco_analyze.h"
#include "lsp.h"
#include "text_document.h"
#include <cnt/cco.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- Document storage --------------------------------------------------- */
#define MAX_DOCS 64

static text_document_t *g_docs[MAX_DOCS];
static char            *g_uris[MAX_DOCS];
static int              g_doc_count = 0;

static text_document_t *find_doc(const char *uri) {
    for (int i = 0; i < g_doc_count; i++)
        if (strcmp(g_uris[i], uri) == 0)
            return g_docs[i];
    return NULL;
}

static int upsert_doc(const char *uri, text_document_t *doc) {
    for (int i = 0; i < g_doc_count; i++) {
        if (strcmp(g_uris[i], uri) == 0) {
            text_document_free(g_docs[i]);
            g_docs[i] = doc;
            return i;
        }
    }
    if (g_doc_count >= MAX_DOCS) return -1;
    g_uris[g_doc_count]  = strdup(uri);
    g_docs[g_doc_count]  = doc;
    return g_doc_count++;
}

static void remove_doc(const char *uri) {
    for (int i = 0; i < g_doc_count; i++) {
        if (strcmp(g_uris[i], uri) == 0) {
            text_document_free(g_docs[i]);
            free(g_uris[i]);
            g_docs[i] = g_docs[--g_doc_count];
            g_uris[i] = g_uris[g_doc_count];
            return;
        }
    }
}

static void publish_diagnostics(void *stream, const char *uri, text_document_t *doc) {
    int count = 0;
    lsp_diagnostic_t *diags = cco_analyze_document(doc, &count);

    json_value_t *params = json_object();
    json_object_set(params, "uri", json_string(uri));
    json_value_t *arr = json_array();
    for (int i = 0; i < count; i++) {
        json_array_append(arr, lsp_diagnostic_to_json(diags[i]));
    }
    json_object_set(params, "diagnostics", arr);

    lsp_write_notification(stream, "textDocument/publishDiagnostics", params);
    /* params ownership transferred; don't free */
    free(diags);
}

/* ---- Handlers ----------------------------------------------------------- */

static void handle_initialize(json_value_t *params, json_value_t *id,
                               void *stream, void *ctx) {
    (void)params;
    (void)ctx;

    json_value_t *cap = json_object();
    /* TextDocumentSyncKind: Full = 1 */
    json_object_set(cap, "textDocumentSync", json_int(1));
    json_object_set(cap, "completionProvider", json_object());
    json_object_set(cap, "hoverProvider", json_bool(1));
    json_object_set(cap, "definitionProvider", json_bool(1));
    json_object_set(cap, "documentSymbolProvider", json_bool(1));

    json_value_t *result = json_object();
    json_object_set(result, "capabilities", cap);
    lsp_write_response(stream, id, result);
}

static void handle_shutdown(json_value_t *params, json_value_t *id,
                             void *stream, void *ctx) {
    (void)params;
    (void)ctx;
    lsp_write_response(stream, id, json_null());
    exit(0);
}

static void handle_did_open(json_value_t *params, json_value_t *id,
                             void *stream, void *ctx) {
    (void)id;
    (void)ctx;
    json_value_t *td = json_get(params, "textDocument");
    if (!td) return;
    const char *uri  = json_as_string(json_get(td, "uri"));
    const char *text = json_as_string(json_get(td, "text"));
    if (!uri || !text) return;

    text_document_t *doc = text_document_new(uri, text);
    if (!doc) return;
    upsert_doc(uri, doc);

    publish_diagnostics(stream, uri, doc);
}

static void handle_did_change(json_value_t *params, json_value_t *id,
                               void *stream, void *ctx) {
    (void)id;
    (void)ctx;
    json_value_t *td = json_get(params, "textDocument");
    if (!td) return;
    const char *uri = json_as_string(json_get(td, "uri"));
    if (!uri) return;

    text_document_t *doc = find_doc(uri);
    if (!doc) return;

    /* Get full text from contentChanges[0].text */
    json_value_t *changes = json_get(params, "contentChanges");
    if (json_is_array(changes) && json_array_size(changes) > 0) {
        json_value_t *ch = json_array_get(changes, 0);
        const char *text = json_as_string(json_get(ch, "text"));
        if (text) {
            text_document_set_text(doc, text);
        }
    }

    publish_diagnostics(stream, uri, doc);
}

static void handle_did_close(json_value_t *params, json_value_t *id,
                              void *stream, void *ctx) {
    (void)id;
    (void)ctx;
    json_value_t *td = json_get(params, "textDocument");
    if (!td) return;
    const char *uri = json_as_string(json_get(td, "uri"));
    if (uri) remove_doc(uri);
}

static void handle_completion(json_value_t *params, json_value_t *id,
                               void *stream, void *ctx) {
    (void)ctx;
    json_value_t *td = json_get(params, "textDocument");
    json_value_t *posv = json_get(params, "position");
    if (!td || !posv) { lsp_write_response(stream, id, json_null()); return; }

    const char *uri = json_as_string(json_get(td, "uri"));
    text_document_t *doc = find_doc(uri);
    if (!doc) { lsp_write_response(stream, id, json_null()); return; }

    lsp_position_t pos = lsp_position_from_json(posv);
    int count = 0;
    lsp_completion_item_t *items = cco_analyze_completions(doc, pos, &count);

    json_value_t *arr = json_array();
    for (int i = 0; i < count; i++) {
        json_array_append(arr, lsp_completion_item_to_json(items[i]));
    }

    lsp_write_response(stream, id, arr);

    for (int i = 0; i < count; i++) {
        free(items[i].label);
        free(items[i].detail);
        free(items[i].insert_text);
    }
    free(items);
}

static void handle_hover(json_value_t *params, json_value_t *id,
                          void *stream, void *ctx) {
    (void)ctx;
    json_value_t *td = json_get(params, "textDocument");
    json_value_t *posv = json_get(params, "position");
    if (!td || !posv) { lsp_write_response(stream, id, json_null()); return; }

    const char *uri = json_as_string(json_get(td, "uri"));
    text_document_t *doc = find_doc(uri);
    if (!doc) { lsp_write_response(stream, id, json_null()); return; }

    lsp_position_t pos = lsp_position_from_json(posv);
    char *markdown = cco_analyze_hover(doc, pos);

    if (markdown) {
        json_value_t *contents = json_object();
        json_object_set(contents, "kind",    json_string("markdown"));
        json_object_set(contents, "value",   json_string(markdown));
        json_value_t *result = json_object();
        json_object_set(result, "contents", contents);
        lsp_write_response(stream, id, result);
        free(markdown);
    } else {
        lsp_write_response(stream, id, json_null());
    }
}

static void handle_definition(json_value_t *params, json_value_t *id,
                               void *stream, void *ctx) {
    (void)ctx;
    json_value_t *td = json_get(params, "textDocument");
    json_value_t *posv = json_get(params, "position");
    if (!td || !posv) { lsp_write_response(stream, id, json_null()); return; }

    const char *uri = json_as_string(json_get(td, "uri"));
    text_document_t *doc = find_doc(uri);
    if (!doc) { lsp_write_response(stream, id, json_null()); return; }

    lsp_position_t pos = lsp_position_from_json(posv);
    char *def_uri = NULL;
    lsp_range_t range = {{0,0},{0,0}};
    int found = cco_analyze_definition(doc, pos, &def_uri, &range);

    if (found && def_uri) {
        json_value_t *loc = json_object();
        json_object_set(loc, "uri",   json_string(def_uri));
        json_object_set(loc, "range", lsp_range_to_json(range));
        lsp_write_response(stream, id, loc);
        free(def_uri);
    } else {
        lsp_write_response(stream, id, json_null());
    }
}

static void handle_document_symbol(json_value_t *params, json_value_t *id,
                                    void *stream, void *ctx) {
    (void)ctx;
    json_value_t *td = json_get(params, "textDocument");
    if (!td) { lsp_write_response(stream, id, json_null()); return; }

    const char *uri = json_as_string(json_get(td, "uri"));
    text_document_t *doc = find_doc(uri);
    if (!doc) { lsp_write_response(stream, id, json_null()); return; }

    cco_symbol_list_t *syms = cco_analyze_document_symbols(doc);
    json_value_t *arr = json_array();
    for (size_t i = 0; i < syms->count; i++) {
        /* Find line number by scanning for the symbol */
        const char *text = doc->text;
        /* Build pattern to search */
        char pat[256];
        snprintf(pat, sizeof(pat), "$%s", syms->names[i]);
        /* Actually find $typedef.X, $enum.X, $temp.X */
        const char *found = NULL;
        if (syms->kinds[i] == 22) {
            char p2[64]; snprintf(p2, sizeof(p2), "$typedef.%s", syms->names[i]);
            found = strstr(text, p2);
        } else if (syms->kinds[i] == 21) {
            char p2[64]; snprintf(p2, sizeof(p2), "$enum.%s", syms->names[i]);
            found = strstr(text, p2);
        } else {
            char p2[64]; snprintf(p2, sizeof(p2), "$temp.%s", syms->names[i]);
            found = strstr(text, p2);
        }
        long long line = 0;
        if (found) {
            size_t off = (size_t)(found - text);
            line = 0;
            for (size_t j = 0; j < off && j < strlen(text); j++)
                if (text[j] == '\n') line++;
        }

        json_value_t *sym = json_object();
        json_object_set(sym, "name", json_string(syms->names[i]));
        json_object_set(sym, "kind", json_int(syms->kinds[i]));
        json_value_t *rng = json_object();
        json_object_set(rng, "start", lsp_position_to_json((lsp_position_t){line, 0}));
        json_object_set(rng, "end",   lsp_position_to_json((lsp_position_t){line, 0}));
        json_object_set(sym, "range", rng);
        json_object_set(sym, "selectionRange", rng);
        json_array_append(arr, sym);
    }
    lsp_write_response(stream, id, arr);
    cco_symbol_list_free(syms);
}

/* ---- Main --------------------------------------------------------------- */

int main(void) {
#ifdef _WIN32
    /* Set stdin/stdout to binary mode */
    #include <fcntl.h>
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    lsp_handler_entry_t handlers[] = {
        {"initialize",                handle_initialize},
        {"shutdown",                  handle_shutdown},
        {"textDocument/didOpen",      handle_did_open},
        {"textDocument/didChange",    handle_did_change},
        {"textDocument/didClose",     handle_did_close},
        {"textDocument/completion",   handle_completion},
        {"textDocument/hover",        handle_hover},
        {"textDocument/definition",   handle_definition},
        {"textDocument/documentSymbol", handle_document_symbol},
        {NULL, NULL}
    };

    for (;;) {
        char *body = NULL;
        size_t len = 0;
        if (lsp_read_message(stdin, &body, &len) != 0)
            break;

        json_value_t *msg = json_parse(body);
        free(body);

        if (!msg) continue;

        const char *method = json_as_string(json_get(msg, "method"));
        json_value_t *id   = json_get(msg, "id");
        json_value_t *params = json_get(msg, "params");

        /* Handle $/cancelRequest: just ignore */
        if (method && strncmp(method, "$/", 2) == 0) {
            json_free(msg);
            continue;
        }

        int handled = 0;
        for (int i = 0; handlers[i].method; i++) {
            if (strcmp(handlers[i].method, method) == 0) {
                /* Detach id from msg so handler can free/use it */
                json_value_t *id_copy = NULL;
                if (id) {
                    id_copy = json_int(json_as_int(id));
                }
                handlers[i].handler(params, id_copy, (void*)stdout, NULL);
                handled = 1;
                break;
            }
        }

        if (!handled && id) {
            /* Unknown method — return error if it had an id */
            lsp_write_error(stdout, json_int(json_as_int(id)),
                            -32601, "Method not found");
        }

        json_free(msg);
    }

    return 0;
}
