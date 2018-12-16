/* Goxel 3D voxels editor
 *
 * copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "duktape.h"
#include "duk_console.h"

duk_ret_t js_goxel_call(duk_context *ctx)
{
    const char *id;
    action_t *a;
    id = duk_to_string(ctx, 0);
    a = action_get(id);
    duk_remove(ctx, 0);
    return action_exec_duk(a, ctx);
}

static void add_globals(duk_context *ctx)
{
    duk_push_c_function(ctx, js_goxel_call, DUK_VARARGS);
    duk_put_global_string(ctx, "goxel_call");
}

/*
 * Function: script_run
 * Run a js script from a file.
 */
int script_run(const char *filename)
{
    char *data;
    int len;
    data = read_file(filename, &len);
    if (!data) return -1;
    duk_context *ctx = duk_create_heap_default();
    add_globals(ctx);
    duk_console_init(ctx, 0);
    duk_push_lstring(ctx, data, len);
    if (duk_peval(ctx) != 0) {
        printf("Error: %s\n", duk_safe_to_string(ctx, -1));
        goto end;
    }
    duk_pop(ctx);  // ignore result.
end:
    free(data);
    duk_destroy_heap(ctx);
    return 0;
}

void duk_to_pos(duk_context *ctx, int idx, int pos[3])
{
    int i;
    for (i = 0; i < 3; i++) {
        duk_get_prop_index(ctx, idx, i);
        pos[i] = duk_to_int(ctx, -1);
        duk_pop(ctx);
    }
}

void duk_to_color(duk_context *ctx, int idx, uint8_t color[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        duk_get_prop_index(ctx, idx, i);
        color[i] = duk_to_int(ctx, -1);
        duk_pop(ctx);
    }
}

void duk_to_aabb(duk_context *ctx, int idx, int aabb[2][3])
{
    duk_to_pos(ctx, idx, aabb[1]);
    aabb[0][0] = 0;
    aabb[0][1] = 0;
    aabb[0][2] = 0;
}

void duk_push_intarray(duk_context *ctx, int len, const int *v)
{
    int i;
    duk_push_array(ctx);
    for (i = 0; i < len; i++) {
        duk_push_int(ctx, v[i]);
        duk_put_prop_index(ctx, -2, i);
    }
}
