#include "lsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Content-Length framing -------------------------------------------- */
int lsp_read_message(void *stream, char **out_body, size_t *out_len) {
    (void)stream; /* we always use stdin */
    char header[256];
    size_t content_length = 0;

    while (fgets(header, sizeof(header), stdin)) {
        if (strcmp(header, "\r\n") == 0 || strcmp(header, "\n") == 0)
            break;
        if (sscanf(header, "Content-Length: %zu", &content_length) == 1)
            continue;
    }
    if (content_length == 0) return -1;

    char *body = (char*)malloc(content_length + 1);
    if (!body) return -1;

    size_t total = 0;
    while (total < content_length) {
        size_t n = fread(body + total, 1, content_length - total, stdin);
        if (n == 0) { free(body); return -1; }
        total += n;
    }
    body[content_length] = '\0';
    *out_body = body;
    *out_len  = content_length;
    return 0;
}

void lsp_write_message(void *stream, const char *body) {
    (void)stream;
    size_t len = strlen(body);
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", len, body);
    fflush(stdout);
}

void lsp_write_notification(void *stream, const char *method, json_value_t *params) {
    json_value_t *msg = json_object();
    json_object_set(msg, "jsonrpc", json_string("2.0"));
    json_object_set(msg, "method",  json_string(method));
    if (params) json_object_set(msg, "params", params);
    else        json_object_set(msg, "params", json_null());
    char *s = json_serialize(msg);
    lsp_write_message(stream, s);
    free(s);
    json_free(msg);
}

void lsp_write_response(void *stream, json_value_t *id, json_value_t *result) {
    json_value_t *msg = json_object();
    json_object_set(msg, "jsonrpc", json_string("2.0"));
    if (id) json_object_set(msg, "id", id);
    else    json_object_set(msg, "id", json_null());
    json_object_set(msg, "result", result ? result : json_null());
    char *s = json_serialize(msg);
    lsp_write_message(stream, s);
    free(s);
    json_free(msg);
}

void lsp_write_error(void *stream, json_value_t *id, int code, const char *msg_text) {
    json_value_t *err = json_object();
    json_object_set(err, "code",    json_int(code));
    json_object_set(err, "message", json_string(msg_text));

    json_value_t *resp = json_object();
    json_object_set(resp, "jsonrpc", json_string("2.0"));
    if (id) json_object_set(resp, "id", id);
    else    json_object_set(resp, "id", json_null());
    json_object_set(resp, "error", err);
    char *s = json_serialize(resp);
    lsp_write_message(stream, s);
    free(s);
    json_free(resp);
}

/* ---- LSP type converters ----------------------------------------------- */
lsp_position_t lsp_position_from_json(json_value_t *v) {
    lsp_position_t p = {0, 0};
    if (!v) return p;
    p.line      = json_as_int(json_get(v, "line"));
    p.character = json_as_int(json_get(v, "character"));
    return p;
}

lsp_range_t lsp_range_from_json(json_value_t *v) {
    lsp_range_t r = {{0,0},{0,0}};
    if (!v) return r;
    r.start = lsp_position_from_json(json_get(v, "start"));
    r.end   = lsp_position_from_json(json_get(v, "end"));
    return r;
}

json_value_t *lsp_position_to_json(lsp_position_t pos) {
    json_value_t *v = json_object();
    json_object_set(v, "line",      json_int(pos.line));
    json_object_set(v, "character", json_int(pos.character));
    return v;
}

json_value_t *lsp_range_to_json(lsp_range_t r) {
    json_value_t *v = json_object();
    json_object_set(v, "start", lsp_position_to_json(r.start));
    json_object_set(v, "end",   lsp_position_to_json(r.end));
    return v;
}

json_value_t *lsp_diagnostic_to_json(lsp_diagnostic_t d) {
    json_value_t *v = json_object();
    json_object_set(v, "range",    lsp_range_to_json(d.range));
    json_object_set(v, "severity", json_int(d.severity));
    json_object_set(v, "message",  json_string(d.message));
    return v;
}

json_value_t *lsp_completion_item_to_json(lsp_completion_item_t item) {
    json_value_t *v = json_object();
    json_object_set(v, "label", json_string(item.label));
    json_object_set(v, "kind",  json_int(item.kind));
    if (item.detail)      json_object_set(v, "detail", json_string(item.detail));
    if (item.insert_text) json_object_set(v, "insertText", json_string(item.insert_text));
    return v;
}

void lsp_diag_free(lsp_diagnostic_t *d) {
    if (d) free(d);
}
