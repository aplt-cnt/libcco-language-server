#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ---- Internal parser state -------------------------------------------- */
typedef struct {
    const char *p;
    const char *end;
    int         error;
    const char *error_pos;
} json_parser_t;

static void json_skip_ws(json_parser_t *ps) {
    while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' ||
           *ps->p == '\n' || *ps->p == '\r'))
        ps->p++;
}

static int json_peek(json_parser_t *ps) {
    json_skip_ws(ps);
    return ps->p < ps->end ? (unsigned char)*ps->p : EOF;
}

static int json_advance(json_parser_t *ps) {
    int c = json_peek(ps);
    if (c != EOF) ps->p++;
    return c;
}

static int json_expect(json_parser_t *ps, int c) {
    int got = json_advance(ps);
    if (got != c) {
        ps->error = 1;
        ps->error_pos = ps->p;
        return 0;
    }
    return 1;
}

static json_value_t *json_parse_value(json_parser_t *ps);

static json_value_t *json_parse_string(json_parser_t *ps) {
    if (json_advance(ps) != '"') { ps->error = 1; return NULL; }
    size_t cap = 64, len = 0;
    char *s = (char*)malloc(cap);
    if (!s) { ps->error = 1; return NULL; }
    while (ps->p < ps->end) {
        int c = (unsigned char)*ps->p;
        if (c == '"') { ps->p++; break; }
        if (c == '\\') {
            ps->p++;
            if (ps->p >= ps->end) { free(s); ps->error = 1; return NULL; }
            switch (*ps->p) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'u': {
                    ps->p++;
                    char hex[5] = {0};
                    for (int i = 0; i < 4; i++, ps->p++) {
                        if (ps->p >= ps->end) { free(s); ps->error = 1; return NULL; }
                        hex[i] = *ps->p;
                    }
                    ps->p--;
                    unsigned long cp = strtoul(hex, NULL, 16);
                    if (cp < 0x80) { c = (int)cp; }
                    else if (cp < 0x800) {
                        if (len + 2 > cap) { cap *= 2; s = (char*)realloc(s, cap); }
                        s[len++] = (char)(0xC0 | (cp >> 6));
                        c = (char)(0x80 | (cp & 0x3F));
                    } else {
                        if (len + 3 > cap) { cap *= 2; s = (char*)realloc(s, cap); }
                        s[len++] = (char)(0xE0 | (cp >> 12));
                        s[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        c = (char)(0x80 | (cp & 0x3F));
                    }
                    ps->p++;
                    continue;
                }
                default: c = *ps->p; break;
            }
            ps->p++;
        } else {
            ps->p++;
        }
        if (len + 1 >= cap) { cap *= 2; s = (char*)realloc(s, cap); }
        s[len++] = (char)c;
    }
    if (len >= cap) { cap++; s = (char*)realloc(s, cap); }
    s[len] = '\0';
    json_value_t *v = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (!v) { free(s); return NULL; }
    v->type = JSON_STRING;
    v->data.string_val = s;
    return v;
}

static json_value_t *json_parse_number(json_parser_t *ps) {
    const char *start = ps->p;
    int is_float = 0;
    if (*ps->p == '-') ps->p++;
    while (ps->p < ps->end && isdigit((unsigned char)*ps->p)) ps->p++;
    if (ps->p < ps->end && *ps->p == '.') {
        is_float = 1; ps->p++;
        while (ps->p < ps->end && isdigit((unsigned char)*ps->p)) ps->p++;
    }
    if (ps->p < ps->end && (*ps->p == 'e' || *ps->p == 'E')) {
        is_float = 1; ps->p++;
        if (ps->p < ps->end && (*ps->p == '+' || *ps->p == '-')) ps->p++;
        while (ps->p < ps->end && isdigit((unsigned char)*ps->p)) ps->p++;
    }
    size_t len = (size_t)(ps->p - start);
    char buf[128];
    if (len >= sizeof(buf)) { ps->error = 1; return NULL; }
    memcpy(buf, start, len); buf[len] = '\0';
    json_value_t *v = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (!v) return NULL;
    if (is_float) {
        v->type = JSON_DOUBLE;
        v->data.double_val = strtod(buf, NULL);
    } else {
        v->type = JSON_INT;
        v->data.int_val = strtoll(buf, NULL, 10);
    }
    return v;
}

static json_value_t *json_parse_array(json_parser_t *ps) {
    json_value_t *arr = json_array();
    if (!arr) return NULL;
    json_advance(ps); /* skip [ */
    if (json_peek(ps) == ']') { json_advance(ps); return arr; }
    for (;;) {
        json_value_t *elem = json_parse_value(ps);
        if (ps->error || !elem) { json_free(arr); json_free(elem); return NULL; }
        json_array_append(arr, elem);
        int c = json_peek(ps);
        if (c == ']') { json_advance(ps); return arr; }
        if (c == ',') { json_advance(ps); continue; }
        json_free(arr); ps->error = 1; return NULL;
    }
}

static json_value_t *json_parse_object(json_parser_t *ps) {
    json_value_t *obj = json_object();
    if (!obj) return NULL;
    json_advance(ps); /* skip { */
    if (json_peek(ps) == '}') { json_advance(ps); return obj; }
    for (;;) {
        if (json_peek(ps) != '"') { json_free(obj); ps->error = 1; return NULL; }
        json_value_t *k = json_parse_string(ps);
        if (!k) { json_free(obj); return NULL; }
        if (json_expect(ps, ':') == 0) { json_free(k); json_free(obj); return NULL; }
        json_value_t *v = json_parse_value(ps);
        if (!v) { json_free(k); json_free(obj); return NULL; }
        json_object_set_n(obj, k->data.string_val, strlen(k->data.string_val), v);
        json_free(k);
        int c = json_peek(ps);
        if (c == '}') { json_advance(ps); return obj; }
        if (c == ',') { json_advance(ps); continue; }
        json_free(obj); ps->error = 1; return NULL;
    }
}

static json_value_t *json_parse_keyword(json_parser_t *ps, const char *kw, json_type_t t) {
    size_t n = strlen(kw);
    if ((size_t)(ps->end - ps->p) < n) { ps->error = 1; return NULL; }
    if (memcmp(ps->p, kw, n) != 0) { ps->error = 1; return NULL; }
    ps->p += n;
    json_value_t *v = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (!v) return NULL;
    v->type = t;
    if (t == JSON_BOOL) v->data.bool_val = (kw[0] == 't');
    return v;
}

static json_value_t *json_parse_value(json_parser_t *ps) {
    int c = json_peek(ps);
    if (c == EOF) return NULL;
    switch (c) {
        case '"': return json_parse_string(ps);
        case '{': return json_parse_object(ps);
        case '[': return json_parse_array(ps);
        case 't': return json_parse_keyword(ps, "true",  JSON_BOOL);
        case 'f': return json_parse_keyword(ps, "false", JSON_BOOL);
        case 'n': return json_parse_keyword(ps, "null",  JSON_NULL);
        default:
            if (c == '-' || isdigit(c)) return json_parse_number(ps);
            ps->error = 1;
            return NULL;
    }
}

json_value_t *json_parse(const char *text) {
    json_parser_t ps;
    ps.p = text;
    ps.end = text + strlen(text);
    ps.error = 0;
    ps.error_pos = NULL;
    json_value_t *v = json_parse_value(&ps);
    if (ps.error) {
        json_free(v);
        return NULL;
    }
    json_skip_ws(&ps);
    if (ps.p < ps.end) {
        json_free(v);
        return NULL;
    }
    return v;
}

/* ---- Construction ------------------------------------------------------ */

json_value_t *json_null(void) {
    json_value_t *v = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (v) v->type = JSON_NULL;
    return v;
}

json_value_t *json_bool(int v) {
    json_value_t *r = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (r) { r->type = JSON_BOOL; r->data.bool_val = v ? 1 : 0; }
    return r;
}

json_value_t *json_int(long long v) {
    json_value_t *r = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (r) { r->type = JSON_INT; r->data.int_val = v; }
    return r;
}

json_value_t *json_double(double v) {
    json_value_t *r = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (r) { r->type = JSON_DOUBLE; r->data.double_val = v; }
    return r;
}

json_value_t *json_string(const char *s) {
    return s ? json_string_n(s, strlen(s)) : json_null();
}

json_value_t *json_string_n(const char *s, size_t n) {
    json_value_t *r = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (!r) return NULL;
    r->type = JSON_STRING;
    r->data.string_val = (char*)malloc(n + 1);
    if (!r->data.string_val) { free(r); return NULL; }
    memcpy(r->data.string_val, s, n);
    r->data.string_val[n] = '\0';
    return r;
}

json_value_t *json_array(void) {
    json_value_t *r = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (r) { r->type = JSON_ARRAY; }
    return r;
}

json_value_t *json_array_append(json_value_t *arr, json_value_t *v) {
    if (!arr || arr->type != JSON_ARRAY) { json_free(v); return arr; }
    if (arr->data.array.count >= arr->data.array.cap) {
        size_t nc = arr->data.array.cap ? arr->data.array.cap * 2 : 4;
        json_value_t **ni = (json_value_t**)realloc(arr->data.array.items, nc * sizeof(json_value_t*));
        if (!ni) { json_free(v); return arr; }
        arr->data.array.items = ni;
        arr->data.array.cap = nc;
    }
    arr->data.array.items[arr->data.array.count++] = v;
    return arr;
}

json_value_t *json_object(void) {
    json_value_t *r = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (r) r->type = JSON_OBJECT;
    return r;
}

json_value_t *json_object_set_n(json_value_t *obj, const char *key, size_t kn, json_value_t *v) {
    if (!obj || obj->type != JSON_OBJECT) { json_free(v); return obj; }
    for (size_t i = 0; i < obj->data.object.count; i++) {
        if (strlen(obj->data.object.pairs[i].key) == kn &&
            memcmp(obj->data.object.pairs[i].key, key, kn) == 0) {
            json_free(obj->data.object.pairs[i].value);
            obj->data.object.pairs[i].value = v;
            return obj;
        }
    }
    if (obj->data.object.count >= obj->data.object.cap) {
        size_t nc = obj->data.object.cap ? obj->data.object.cap * 2 : 4;
        json_pair_t *np = (json_pair_t*)realloc(obj->data.object.pairs, nc * sizeof(json_pair_t));
        if (!np) { json_free(v); return obj; }
        obj->data.object.pairs = np;
        obj->data.object.cap = nc;
    }
    size_t i = obj->data.object.count++;
    obj->data.object.pairs[i].key = (char*)malloc(kn + 1);
    if (obj->data.object.pairs[i].key) {
        memcpy(obj->data.object.pairs[i].key, key, kn);
        obj->data.object.pairs[i].key[kn] = '\0';
    }
    obj->data.object.pairs[i].value = v;
    return obj;
}

json_value_t *json_object_set(json_value_t *obj, const char *key, json_value_t *v) {
    return json_object_set_n(obj, key, strlen(key), v);
}

/* ---- Query helpers ----------------------------------------------------- */

json_value_t *json_get(const json_value_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    for (size_t i = 0; i < obj->data.object.count; i++) {
        if (strcmp(obj->data.object.pairs[i].key, key) == 0)
            return obj->data.object.pairs[i].value;
    }
    return NULL;
}

int json_is_array(const json_value_t *v)   { return v && v->type == JSON_ARRAY; }
int json_is_object(const json_value_t *v)  { return v && v->type == JSON_OBJECT; }
int json_is_string(const json_value_t *v)  { return v && v->type == JSON_STRING; }
const char *json_as_string(const json_value_t *v) { return json_is_string(v) ? v->data.string_val : NULL; }
long long json_as_int(const json_value_t *v) { return v && v->type == JSON_INT ? v->data.int_val : 0; }
int json_as_bool(const json_value_t *v) { return v && v->type == JSON_BOOL ? v->data.bool_val : 0; }
size_t json_array_size(const json_value_t *v) { return json_is_array(v) ? v->data.array.count : 0; }
json_value_t *json_array_get(const json_value_t *v, size_t i) {
    return json_is_array(v) && i < v->data.array.count ? v->data.array.items[i] : NULL;
}

/* ---- Serialization ----------------------------------------------------- */

static void json_escape(const char *s, char **buf, size_t *len, size_t *cap) {
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        const char *esc = NULL;
        char esc_buf[8];
        switch (c) {
            case '"':  esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\b': esc = "\\b";  break;
            case '\f': esc = "\\f";  break;
            case '\n': esc = "\\n";  break;
            case '\r': esc = "\\r";  break;
            case '\t': esc = "\\t";  break;
            default:
                if (c < 0x20) {
                    int n = snprintf(esc_buf, sizeof(esc_buf), "\\u%04x", c);
                    esc = esc_buf;
                    (void)n;
                }
                break;
        }
        if (esc) {
            size_t el = strlen(esc);
            if (*len + el + 1 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
            memcpy(*buf + *len, esc, el);
            *len += el;
        } else {
            if (*len + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
            (*buf)[(*len)++] = c;
        }
    }
}

void json_serialize_buf(const json_value_t *v, char **buf, size_t *len, size_t *cap) {
    if (!v) { memcpy(*buf + *len, "null", 4); *len += 4; return; }
    switch (v->type) {
    case JSON_NULL:
        if (*len + 5 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        memcpy(*buf + *len, "null", 4); *len += 4;
        break;
    case JSON_BOOL:
        if (v->data.bool_val) {
            if (*len + 5 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
            memcpy(*buf + *len, "true", 4); *len += 4;
        } else {
            if (*len + 6 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
            memcpy(*buf + *len, "false", 5); *len += 5;
        }
        break;
    case JSON_INT: {
        char tmp[32];
        int n = snprintf(tmp, sizeof(tmp), "%lld", v->data.int_val);
        if (*len + (size_t)n + 1 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        memcpy(*buf + *len, tmp, (size_t)n); *len += (size_t)n;
        break;
    }
    case JSON_DOUBLE: {
        char tmp[64];
        int n = snprintf(tmp, sizeof(tmp), "%g", v->data.double_val);
        if (*len + (size_t)n + 1 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        memcpy(*buf + *len, tmp, (size_t)n); *len += (size_t)n;
        break;
    }
    case JSON_STRING:
        if (*len + 3 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        (*buf)[(*len)++] = '"';
        json_escape(v->data.string_val, buf, len, cap);
        if (*len + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        (*buf)[(*len)++] = '"';
        break;
    case JSON_ARRAY:
        if (*len + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        (*buf)[(*len)++] = '[';
        for (size_t i = 0; i < v->data.array.count; i++) {
            if (i > 0) { if (*len + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
                (*buf)[(*len)++] = ','; }
            json_serialize_buf(v->data.array.items[i], buf, len, cap);
        }
        if (*len + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        (*buf)[(*len)++] = ']';
        break;
    case JSON_OBJECT:
        if (*len + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        (*buf)[(*len)++] = '{';
        for (size_t i = 0; i < v->data.object.count; i++) {
            if (i > 0) { if (*len + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
                (*buf)[(*len)++] = ','; }
            if (*len + 3 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
            (*buf)[(*len)++] = '"';
            json_escape(v->data.object.pairs[i].key, buf, len, cap);
            if (*len + 3 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
            (*buf)[(*len)++] = '"';
            if (*len + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
            (*buf)[(*len)++] = ':';
            json_serialize_buf(v->data.object.pairs[i].value, buf, len, cap);
        }
        if (*len + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        (*buf)[(*len)++] = '}';
        break;
    }
}

char *json_serialize(const json_value_t *v) {
    size_t cap = 256, len = 0;
    char *buf = (char*)malloc(cap);
    if (!buf) return NULL;
    json_serialize_buf(v, &buf, &len, &cap);
    buf[len] = '\0';
    return buf;
}

/* ---- Free -------------------------------------------------------------- */
void json_free(json_value_t *v) {
    if (!v) return;
    switch (v->type) {
    case JSON_STRING:
        free(v->data.string_val);
        break;
    case JSON_ARRAY:
        for (size_t i = 0; i < v->data.array.count; i++)
            json_free(v->data.array.items[i]);
        free(v->data.array.items);
        break;
    case JSON_OBJECT:
        for (size_t i = 0; i < v->data.object.count; i++) {
            free(v->data.object.pairs[i].key);
            json_free(v->data.object.pairs[i].value);
        }
        free(v->data.object.pairs);
        break;
    default: break;
    }
    free(v);
}
