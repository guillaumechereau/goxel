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

ACTION_REGISTER(export_as_png,
    .help = "Export the image as a png file",
    .cfunc = export_as_png,
    .csig = "vpii",
    .file_format = {
        .name = "png",
        .ext = "*.png\0",
    },
)
