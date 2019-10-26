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

/*
 * Experimental support for semi automatic binding of C structure to be used
 * from script language.
 */

#ifndef META_H
#define META_H

#include <stdint.h>
#include <stdbool.h>

// Negative type are reference counted.
enum {
    TYPE_STRING = -1,
    TYPE_OBJ = -2,

    TYPE_NULL = 0,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_ERROR,
};

enum {
    ATTR_GET = 1,
};

typedef struct val
{
    int32_t type;
    union {
        int32_t int32;
        double  float64;
        void    *ptr;
    };
} val_t;

typedef struct attribute
{
    const char *name;
    int type;
    struct {
        int offset;
        int size;
    } member;
} attribute_t;

typedef struct klass
{
    const char *name;
    attribute_t *attributes;
} klass_t;

typedef struct obj
{
    int ref;
    const klass_t *klass;
} obj_t;

val_t val_dup(val_t val);
void val_free(val_t val);

val_t val_new_null(void);
val_t val_new_bool(bool v);
val_t val_new_int(int32_t v);
val_t val_new_double(double v);
val_t val_new_string(const char *string);
val_t val_new_error(void);
val_t val_new_obj(void *ptr);

val_t val_get_attr(val_t val, const char *attr);
void val_set_attr(val_t val, const char *attr, val_t value);

val_t obj_get_attr(const obj_t *obj, const char *attr);

#define MEMBER(k, m) .member = {offsetof(k, m), sizeof(((k*)0)->m)}

#endif // META_H
