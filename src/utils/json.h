#ifndef JSON_H
#define JSON_H

#include "../../ext_src/json/json.h"
#include "../../ext_src/json/json-builder.h"

#include <stdbool.h>

// Some extra helper functions.

json_value *json_object_push_int(json_value *obj, const json_char *name,
                                 json_int_t v);
json_value *json_object_push_string(json_value *obj, const json_char *name,
                                    const json_char *v);
json_value *json_object_push_bool(json_value *obj, const json_char *name,
                                  bool v);
json_value *json_object_push_float(json_value *obj, const json_char *name,
                                   double v);

json_value *json_data_new(const void *data, uint32_t len, const char *mime);

json_value *json_int_array_new(const int *v, int nb);
json_value *json_float_array_new(const float *v, int nb);

int json_index(json_value *v);

#endif // JSON_H
