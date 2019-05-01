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


#include "goxel.h"

typedef struct {
    const char *name;
    const char *defines;
    gl_shader_t *shader;
} shader_t;

static shader_t g_shaders[5] = {};

gl_shader_t *shader_get(const char *name, const char *defines,
                        void (*on_created)(gl_shader_t *s))
{
    int i;
    shader_t *s = NULL;
    const char *code;
    char path[128];
    char pre[256] = {};


    for (i = 0; i < ARRAY_SIZE(g_shaders); i++) {
        s = &g_shaders[i];
        if (!s->name) break;
        if (s->name == name && s->defines == defines)
            return s->shader;
    }
    assert(i < ARRAY_SIZE(g_shaders));
    s->name = name;
    s->defines = defines;

    sprintf(path, "asset://data/shaders/%s.glsl", name);
    code = assets_get(path, NULL);
    assert(code);

    while (defines && *defines) {
        sprintf(pre + strlen(pre), "#define %s\n", defines);
        defines += strlen(defines);
    }

    s->shader = gl_shader_create(code, code, pre);

    if (on_created) on_created(s->shader);
    return s->shader;
}
