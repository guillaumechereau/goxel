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

#include "goxel.h"

static void export_as_pov(const char *path, int w, int h)
{
    FILE *file;
    layer_t *layer;
    int size, p[3];
    char *buf;
    const char *template;
    uint8_t v[4];
    mat4_t modelview;
    vec3_t light_dir;
    mustache_t *m, *m_cam, *m_light, *m_voxels, *m_voxel;
    camera_t camera = goxel->camera;
    mesh_iterator_t iter;

    w = w ?: goxel->image->export_width;
    h = h ?: goxel->image->export_height;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                    "povray\0*.pov\0", NULL, "untitled.pov");
    if (!path) return;

    template = assets_get("asset://data/povray_template.pov", NULL);
    assert(template);
    camera.aspect = (float)w / h;
    camera_update(&camera);

    modelview = camera.view_mat;
    // cam_to_view = mat4_inverted(camera.view_mat);
    // cam_look_at = mat4_mul_vec(cam_to_view, vec4(0, 0, -1, 1)).xyz;
    light_dir = render_get_light_dir(&goxel->rend);

    m = mustache_root();
    mustache_add_str(m, "version", GOXEL_VERSION_STR);
    m_cam = mustache_add_dict(m, "camera");
    mustache_add_str(m_cam, "width", "%d", w);
    mustache_add_str(m_cam, "height", "%d", h);
    mustache_add_str(m_cam, "angle", "%.1f", camera.fovy * camera.aspect);
    mustache_add_str(m_cam, "modelview",
                     "<%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f>",
                     modelview.v[0], modelview.v[1], modelview.v[2],
                     modelview.v[4], modelview.v[5], modelview.v[6],
                     modelview.v[8], modelview.v[9], modelview.v[10],
                     modelview.v[12], modelview.v[13], modelview.v[14]);
    m_light = mustache_add_dict(m, "light");
    mustache_add_str(m_light, "ambient", "%.2f",
                     goxel->rend.settings.ambient);
    mustache_add_str(m_light, "point_at", "<%.1f, %.1f, %.1f + 1024>",
                     -light_dir.x, -light_dir.y, -light_dir.z);

    m_voxels = mustache_add_list(m, "voxels");
    DL_FOREACH(goxel->image->layers, layer) {
        iter = mesh_get_iterator(layer->mesh, MESH_ITER_VOXELS);
        while (mesh_iter(&iter, p)) {
            mesh_get_at(layer->mesh, p, &iter, v);
            if (v[3] < 127) continue;
            m_voxel = mustache_add_dict(m_voxels, NULL);
            mustache_add_str(m_voxel, "pos", "<%d, %d, %d>",
                             p[0], p[1], p[2]);
            mustache_add_str(m_voxel, "color", "<%d, %d, %d>",
                             v[0], v[1], v[2]);
        }
    }

    size = mustache_render(m, template, NULL);
    buf = calloc(size + 1, 1);
    mustache_render(m, template, buf);
    mustache_free(m);

    file = fopen(path, "wb");
    fwrite(buf, 1, size, file);
    fclose(file);
    free(buf);
}

ACTION_REGISTER(export_as_pov,
    .help = "Save the image as a povray 3d file",
    .cfunc = export_as_pov,
    .csig = "vpii",
    .file_format = {
        .name = "povray",
        .ext = "*.povray\0",
    },
)
