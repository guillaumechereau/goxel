/* Goxel 3D voxels editor
 *
 * copyright (c) 2023-present Guillaume Chereau <guillaume@noctua-software.com>
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

#include "quickjs/quickjs.h"
#include "quickjs/quickjs-libc.h"

int script_run(const char *filename, int argc, const char **argv)
{
    char *script;
    int ret = 0, size;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue val;

    script = read_file(filename, &size);
    if (!script) {
        fprintf(stderr, "Cannot read '%s'\n", filename);
        return -1;
    }

    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    js_std_add_helpers(ctx, argc, (char**)argv);
    val = JS_Eval(ctx, script, size, filename, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        js_std_dump_error(ctx);
        ret = -1;
    }
    JS_FreeValue(ctx, val);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    free(script);
    return ret;
}
