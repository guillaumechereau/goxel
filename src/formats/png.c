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
    uint8_t *buf;
    w = w ?: goxel.image->export_width;
    h = h ?: goxel.image->export_height;
    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                   "png\0*.png\0", NULL, "untitled.png");
    if (!path) return;
    LOG_I("Exporting to file %s", path);
    buf = calloc(w * h, 4);
    goxel_render_to_buf(buf, w, h, 4);
    img_write(buf, w, h, 4, path);
    free(buf);
}

static void export_gui(void) {
    int maxsize, i;
    float view_rect[4];

    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize));
    maxsize /= 2; // Because png export already double it.
    goxel.show_export_viewport = true;
    gui_group_begin(NULL);
    i = goxel.image->export_width;
    if (gui_input_int("w", &i, 1, maxsize))
        goxel.image->export_width = clamp(i, 1, maxsize);
    i = goxel.image->export_height;
    if (gui_input_int("h", &i, 1, maxsize))
        goxel.image->export_height = clamp(i, 1, maxsize);
    if (gui_button("Fit screen", 1, 0)) {
        gui_get_view_rect(view_rect);
        goxel.image->export_width = view_rect[2];
        goxel.image->export_height = view_rect[3];
    }
    gui_group_end();
}

ACTION_REGISTER(export_as_png,
    .help = "Export the image as a png file",
    .cfunc = export_as_png,
    .csig = "vpii",
    .file_format = {
        .name = "png",
        .ext = "*.png\0",
        .export_gui = export_gui,
    },
)
