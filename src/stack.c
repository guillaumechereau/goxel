/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "goxel.h"

typedef struct
{
    union {
        void   *p;
        int     i;
    };
    char type;
} astack_entry_t;

struct astack
{
    int size;
    astack_entry_t entries[8];
};

astack_t *stack_create(void)
{
    return calloc(1, sizeof(astack_t));
}

void stack_delete(astack_t *s)
{
    free(s);
}

static int fix_i(const astack_t *s, int i)
{
    if (i < 0) i = s->size + i;
    if (i >= s->size) return -1;
    return i;
}

void stack_push_i(astack_t *s, int i)
{
    s->entries[s->size++] = (astack_entry_t) {.i = i, .type = 'i'};
}

void stack_push_p(astack_t *s, void *p)
{
    s->entries[s->size++] = (astack_entry_t) {.p = p, .type = 'p'};
}

int stack_get_i(const astack_t *s, int i)
{
    const astack_entry_t *e;
    i = fix_i(s, i);
    assert(i >= 0);
    e = &s->entries[i];
    return e->type == 'i' ? e->i : 0;
}

void *stack_get_p(const astack_t *s, int i)
{
    const astack_entry_t *e;
    i = fix_i(s, i);
    assert(i >= 0);
    e = &s->entries[i];
    return e->type == 'p' ? e->p : NULL;
}

void stack_pop(astack_t *s)
{
    assert(s->size > 0);
    s->size--;
}

