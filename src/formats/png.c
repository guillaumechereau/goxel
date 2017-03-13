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

// XXX: this function has to be rewritten.
static void export_as_png(const char *path, int w, int h)
{
    w = w ?: goxel->image->export_width;
    h = h ?: goxel->image->export_height;
    int rect[4] = {0, 0, w * 2, h * 2};
    uint8_t *data2, *data;
    texture_t *fbo;
    renderer_t rend = goxel->rend;
    mesh_t *mesh;
    camera_t camera = goxel->camera;

    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                   "png\0*.png\0", NULL, "untitled.png");
    if (!path) return;

    camera.aspect = (float)w / h;
    LOG_I("Exporting to file %s", path);

    mesh = goxel->layers_mesh;
    fbo = texture_new_buffer(w * 2, h * 2, TF_DEPTH);

    camera_update(&camera);
    rend.view_mat = camera.view_mat;
    rend.proj_mat = camera.proj_mat;
    rend.fbo = fbo->framebuffer;

    render_mesh(&rend, mesh, 0);
    render_render(&rend, rect, &vec4_zero);
    data2 = calloc(w * h * 4 , 4);
    data = calloc(w * h, 4);
    texture_get_data(fbo, w * 2, h * 2, 4, data2);
    img_downsample(data2, w * 2, h * 2, 4, data);
    img_write(data, w, h, 4, path);
    free(data2);
    free(data);
}

ACTION_REGISTER(export_as_png,
    .help = "Export the image as a png file",
    .cfunc = export_as_png,
    .csig = "vpii",
    .file_format = {
        .name = "png",
        .ext = "*.png\0",
    },
)
