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
