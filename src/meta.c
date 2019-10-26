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

#include "meta.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int ref;
    char *str;
} string_t;

static bool val_has_ref_count(val_t val)
{
    return val.type < 0;
}

val_t val_dup(val_t val)
{
    int *ref;
    if (val_has_ref_count(val)) {
        ref = val.ptr;
        (*ref)++;
    }
    return val;
}

void val_free(val_t val)
{
    int *ref;
    if (!val_has_ref_count(val)) return;
    ref = val.ptr;
    if ((*ref)-- > 0) return;
    free(val.ptr);
    val.ptr = NULL;
}

val_t val_new_null(void)
{
    return (val_t) {
        .type = TYPE_NULL,
    };
}

val_t val_new_bool(bool v)
{
    return (val_t) {
        .type = TYPE_BOOL,
        .int32 = v,
    };
}

val_t val_new_int(int32_t v)
{
    return (val_t) {
        .type = TYPE_INT,
        .int32 = v,
    };
}

val_t val_new_double(double v)
{
    return (val_t) {
        .type = TYPE_DOUBLE,
        .float64 = v,
    };
}

val_t val_new_error(void)
{
    return (val_t) {
        .type = TYPE_ERROR,
    };
}

val_t val_new_obj(void *ptr)
{
    if (!ptr)
        return val_new_null();
    (*((int*)ptr))++;
    return (val_t) {
        .type = TYPE_OBJ,
        .ptr = ptr,
    };
}

val_t val_new_string(const char *string)
{
    string_t *s = calloc(1, sizeof(*s));
    s->str = strdup(string);
    s->ref = 1;
    return (val_t) {
        .type = TYPE_STRING,
        .ptr = s,
    };
}

val_t val_get_attr(val_t val, const char *attr)
{
    obj_t *obj;
    if (val.type == TYPE_OBJ) {
        obj = val.ptr;
        return obj_get_attr(obj, attr);
    }
    return val_new_error();
}

static attribute_t *obj_get_attr_(const obj_t *obj, const char *name)
{
    attribute_t *attr;
    int i;
    assert(obj);
    if (!obj->klass->attributes) return NULL;
    for (i = 0; ; i++) {
        attr = &obj->klass->attributes[i];
        if (!attr->name) return NULL;
        if (strcmp(attr->name, name) == 0) break;
    }
    return attr;
}

val_t obj_get_attr(const obj_t *obj, const char *attr)
{
    void *p;
    attribute_t *a = obj_get_attr_(obj, attr);
    if (!a) return val_new_error();

    if (a->member.size) {
        p = ((void*)obj) + a->member.offset;
        switch (a->type) {
        case TYPE_BOOL:
            return val_new_bool(*(bool*)p);
        case TYPE_OBJ:
            return val_new_obj((void*)p);
        default:
            LOG_E("Type not supported");
            assert(false);
        }
    }

    return val_new_error();
}
