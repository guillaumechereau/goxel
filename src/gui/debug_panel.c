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

void gui_debug_panel(void)
{
    mesh_global_stats_t stats;

    gui_text("FPS: %d", (int)round(goxel.fps));
    mesh_get_global_stats(&stats);
    gui_text("Nb meshes: %d", stats.nb_meshes);
    gui_text("Nb blocks: %d", stats.nb_blocks);
    gui_text("Mem: %dM", (int)(stats.mem / (1 << 20)));

    if (!DEFINED(GLES2)) {
        gui_checkbox_flag("Show wireframe", &goxel.view_effects,
                          EFFECT_WIREFRAME, NULL);
    }

    if (gui_button("Clear undo history", -1, 0)) {
        image_history_resize(goxel.image, 0);
    }
    if (gui_button("On low memory", -1, 0)) {
        goxel_on_low_memory();
    }
    if (gui_button("Test release", -1, 0)) {
        goxel.request_test_graphic_release = true;
    }

}

