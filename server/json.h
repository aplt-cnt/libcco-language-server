#ifndef CCO_LSP_JSON_H
#define CCO_LSP_JSON_H

#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_INT,
    JSON_DOUBLE,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

typedef struct json_value json_value_t;
typedef struct json_pair  json_pair_t;

struct json_pair {
    char         *key;
    json_value_t *value;
};

struct json_value {
    json_type_t type;
    union {
        int        bool_val;
        long long  int_val;
        double     double_val;
        char      *string_val;
        struct { json_value_t **items; size_t count, cap; } array;
        struct { json_pair_t  *pairs;  size_t count, cap; } object;
    } data;
};

json_value_t *json_null(void);
json_value_t *json_bool(int v);
json_value_t *json_int(long long v);
json_value_t *json_double(double v);
json_value_t *json_string(const char *s);
json_value_t *json_string_n(const char *s, size_t n);
json_value_t *json_array(void);
json_value_t *json_array_append(json_value_t *arr, json_value_t *v);
json_value_t *json_object(void);
json_value_t *json_object_set(json_value_t *obj, const char *key, json_value_t *v);
json_value_t *json_object_set_n(json_value_t *obj, const char *key, size_t kn, json_value_t *v);

json_value_t *json_parse(const char *text);
void          json_free(json_value_t *v);

char         *json_serialize(const json_value_t *v);
void          json_serialize_buf(const json_value_t *v, char **buf, size_t *len, size_t *cap);

json_value_t *json_get(const json_value_t *obj, const char *key);
int           json_is_array(const json_value_t *v);
int           json_is_object(const json_value_t *v);
int           json_is_string(const json_value_t *v);
const char   *json_as_string(const json_value_t *v);
long long     json_as_int(const json_value_t *v);
int           json_as_bool(const json_value_t *v);
size_t        json_array_size(const json_value_t *v);
json_value_t *json_array_get(const json_value_t *v, size_t i);

#endif
