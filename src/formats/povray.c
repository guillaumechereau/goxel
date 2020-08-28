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
#include "file_format.h"
#include "utils/mustache.h"

static int export_as_pov(const image_t *image, const char *path)
{
    FILE *file;
    layer_t *layer;
    int size, p[3], w, h;
    char *buf;
    const char *template;
    uint8_t v[4];
    float modelview[4][4], light_dir[3];
    mustache_t *m, *m_cam, *m_light, *m_voxels, *m_voxel;
    camera_t camera = *image->active_camera;
    mesh_iterator_t iter;

    w = image->export_width;
    h = image->export_height;

    template = assets_get("asset://data/other/povray_template.pov", NULL);
    assert(template);
    camera.aspect = (float)w / h;
    camera_update(&camera);

    mat4_copy(camera.view_mat, modelview);
    // cam_to_view = mat4_inverted(camera.view_mat);
    // cam_look_at = mat4_mul_vec(cam_to_view, vec4(0, 0, -1, 1)).xyz;
    render_get_light_dir(&goxel.rend, light_dir);

    m = mustache_root();
    mustache_add_str(m, "version", GOXEL_VERSION_STR);
    m_cam = mustache_add_dict(m, "camera");
    mustache_add_str(m_cam, "width", "%d", w);
    mustache_add_str(m_cam, "height", "%d", h);
    mustache_add_str(m_cam, "angle", "%.1f", camera.fovy * camera.aspect);
    mustache_add_str(m_cam, "modelview",
                     "<%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f>",
                     modelview[0][0], modelview[0][1], modelview[0][2],
                     modelview[1][0], modelview[1][1], modelview[1][2],
                     modelview[2][0], modelview[2][1], modelview[2][2],
                     modelview[3][0], modelview[3][1], modelview[3][2]);
    m_light = mustache_add_dict(m, "light");
    mustache_add_str(m_light, "ambient", "%.2f",
                     goxel.rend.settings.ambient);
    mustache_add_str(m_light, "point_at", "<%.1f, %.1f, %.1f + 1024>",
                     -light_dir[0], -light_dir[1], -light_dir[2]);

    m_voxels = mustache_add_list(m, "voxels");
    DL_FOREACH(image->layers, layer) {
        iter = mesh_get_iterator(layer->mesh, MESH_ITER_VOXELS);
        while (mesh_iter(&iter, p)) {
            mesh_get_at(layer->mesh, &iter, p, v);
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
    return 0;
}

FILE_FORMAT_REGISTER(povray,
    .name = "povray",
    .ext = "povray\0*.povray\0",
    .export_func = export_as_pov,
)
