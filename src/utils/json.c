/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "json.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#include "../../ext_src/json/json.c"
#include "../../ext_src/json/json-builder.c"

#pragma GCC diagnostic pop

#include <assert.h>
#include <stdbool.h>

static size_t base64_encode(const uint8_t *data, size_t len, char *buf)
{
    const char table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                          'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                          'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                          'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                          'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                          'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                          'w', 'x', 'y', 'z', '0', '1', '2', '3',
                          '4', '5', '6', '7', '8', '9', '+', '/'};
    const int mod_table[] = {0, 2, 1};
    uint32_t a, b, c, triple;
    int i, j;
    size_t out_len = 4 * ((len + 2) / 3);
    if (!buf) return out_len;
    for (i = 0, j = 0; i < len;) {
        a = i < len ? data[i++] : 0;
        b = i < len ? data[i++] : 0;
        c = i < len ? data[i++] : 0;
        triple = (a << 0x10) + (b << 0x08) + c;

        buf[j++] = table[(triple >> 3 * 6) & 0x3F];
        buf[j++] = table[(triple >> 2 * 6) & 0x3F];
        buf[j++] = table[(triple >> 1 * 6) & 0x3F];
        buf[j++] = table[(triple >> 0 * 6) & 0x3F];
    }
    for (i = 0; i < mod_table[len % 3]; i++)
        buf[out_len - 1 - i] = '=';

    return out_len;
}

json_value *json_object_push_int(json_value *obj, const json_char *name,
                                 json_int_t v)
{
    return json_object_push(obj, name, json_integer_new(v));
}

json_value *json_object_push_string(json_value *obj, const json_char *name,
                                    const json_char *v)
{
    return json_object_push(obj, name, json_string_new(v));
}

json_value *json_object_push_bool(json_value *obj, const json_char *name,
                                  bool v)
{
    return json_object_push(obj, name, json_boolean_new(v));
}

json_value *json_object_push_float(json_value *obj, const json_char *name,
                                   double v)
{
    return json_object_push(obj, name, json_double_new(v));
}

json_value *json_data_new(const void *data, uint32_t len, const char *mime)
{
    char *string;
    if (!mime) mime = "application/octet-stream";
    string = calloc(strlen("data:") + strlen(mime) + strlen(";base64,") +
                    base64_encode(data, len, NULL) + 1, 1);
    sprintf(string, "data:%s;base64,", mime);
    base64_encode(data, len, string + strlen(string));
    return json_string_new_nocopy(strlen(string), string);
}

json_value *json_int_array_new(const int *v, int nb)
{
    int i;
    json_value *array = json_array_new(nb);
    for (i = 0; i < nb; i++) {
        json_array_push(array, json_integer_new(v[i]));
    }
    return array;
}

json_value *json_float_array_new(const float *v, int nb)
{
    int i;
    json_value *array = json_array_new(nb);
    for (i = 0; i < nb; i++) {
        json_array_push(array, json_double_new(v[i]));
    }
    return array;
}

int json_index(json_value *value)
{
    int i;
    assert(value->parent);
    assert(value->parent->type == json_array);
    for (i = 0; i < value->parent->u.array.length; i++) {
        if (value->parent->u.array.values[i] == value)
            return i;
    }
    assert(false);
    return -1;
}
