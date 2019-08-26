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
    int bpp = goxel.image->export_transparent_background ? 4 : 3;
    w = w ?: goxel.image->export_width;
    h = h ?: goxel.image->export_height;
    path = path ?: noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                   "png\0*.png\0", NULL, "untitled.png");
    if (!path) return;
    LOG_I("Exporting to file %s", path);
    buf = calloc(w * h, bpp);
    goxel_render_to_buf(buf, w, h, bpp);
    img_write(buf, w, h, bpp, path);
    free(buf);
}

static void export_gui(void) {
    int maxsize, i;

    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize));
    maxsize /= 2; // Because png export already double it.
    goxel.show_export_viewport = true;
    gui_group_begin(NULL);
    gui_checkbox("Custom size", &goxel.image->export_custom_size, NULL);
    if (!goxel.image->export_custom_size) {
        goxel.image->export_width = goxel.gui.viewport[2];
        goxel.image->export_height = goxel.gui.viewport[3];
    }

    gui_enabled_begin(goxel.image->export_custom_size);
    i = goxel.image->export_width;
    if (gui_input_int("w", &i, 1, maxsize))
        goxel.image->export_width = clamp(i, 1, maxsize);
    i = goxel.image->export_height;
    if (gui_input_int("h", &i, 1, maxsize))
        goxel.image->export_height = clamp(i, 1, maxsize);
    gui_enabled_end();
    gui_group_end();

    gui_checkbox("Transparent background",
                 &goxel.image->export_transparent_background,
                 NULL);
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
