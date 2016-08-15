/* Goxel 3D voxels editor
 *
 * copyright (c) 2016 Guillaume Chereau <guillaume@noctua-software.com>
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

// Povray export support.
// XXX: I should use a template library to render the pov file.

#include "goxel.h"

// Turn goxel coordinates into povray coordinates.
#define FIX_AXIS(x, y, z) (y), (z), (-x)

static void export_as_pov(goxel_t *goxel, const char *path)
{
    FILE *file;
    layer_t *layer;
    block_t *block;
    int x, y, z, vx, vy, vz;
    uvec4b_t v;
    mat4_t cam_to_view;
    vec3_t cam_pos, cam_look_at, light_dir;

    cam_to_view = mat4_inverted(goxel->camera.view_mat);
    cam_pos = mat4_mul_vec(cam_to_view, vec4(0, 0, 0, 1)).xyz;
    cam_look_at = mat4_mul_vec(cam_to_view, vec4(0, 0, -1, 1)).xyz;
    light_dir = render_get_light_dir(&goxel->rend);

    file = fopen(path, "wb");
    fprintf(file,
            "// Generated from goxel " GOXEL_VERSION_STR "\n"
            "camera {\n"
            "    location <%.1f, %.1f, %.1f>\n"
            "    look_at <%.1f, %.1f, %.1f>\n"
            "    angle %.1f\n"
            "}\n"
            "\n"
            "#declare Voxel = box {<-0.5, -0.5, -0.5>, <0.5, 0.5, 0.5>}\n"
            "#macro Vox(Pos, Color)\n"
            "    object {\n"
            "        Voxel\n"
            "        translate Pos\n"
            "        texture { pigment {color rgb Color / 255} }\n"
            "    }\n"
            "#end\n"
            "light_source {\n"
            "    <0, 1024, 0> color rgb <2, 2, 2>\n"
            "    parallel\n"
            "    point_at <%.1f, %.1f + 1024, %.1f>\n"
            "}\n",
            FIX_AXIS(cam_pos.x, cam_pos.y, cam_pos.z),
            FIX_AXIS(cam_look_at.x, cam_look_at.y, cam_look_at.z),
            goxel->camera.fovy,
            FIX_AXIS(-light_dir.x, -light_dir.y, -light_dir.z)
    );

    fprintf(file, "union {\n");
    DL_FOREACH(goxel->image->layers, layer) {
        MESH_ITER_VOXELS(layer->mesh, block, x, y, z, v) {
            if (v.a < 127) continue;
            vx = x + block->pos.x - BLOCK_SIZE / 2;
            vy = y + block->pos.y - BLOCK_SIZE / 2;
            vz = z + block->pos.z - BLOCK_SIZE / 2;
            fprintf(file, "    Vox(<%d, %d, %d>, <%d, %d, %d>)\n",
                    FIX_AXIS(vx, vy, vz), v.r, v.g, v.b);
        }
    }
    fprintf(file, "}\n");
    fclose(file);
}

ACTION_REGISTER(export_as_pov,
    .help = "Save the image as a povray 3d file",
    .func = export_as_pov,
    .sig = SIG(TYPE_VOID, ARG("goxel", TYPE_GOXEL),
                          ARG("path", TYPE_FILE_PATH)),
    .flags = ACTION_NO_CHANGE,
)
