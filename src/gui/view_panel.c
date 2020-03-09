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

void gui_view_panel(void)
{
    // XXX: I don't like to use this array.
    const struct {
        uint8_t    *color;
        const char *label;
    } COLORS[] = {
        {goxel.back_color, "Back color"},
        {goxel.grid_color, "Grid color"},
        {goxel.image_box_color, "Box color"},
    };
    int i;

    for (i = 0; i < (int)ARRAY_SIZE(COLORS); i++) {
        gui_color_small(COLORS[i].label, COLORS[i].color);
    }
    gui_checkbox("Hide box", &goxel.hide_box, NULL);

    gui_text("Effects");

    if (gui_input_float("occlusion", &goxel.rend.settings.occlusion_strength,
                        0.1, 0, 1, NULL)) {
        goxel.rend.settings.occlusion_strength =
            clamp(goxel.rend.settings.occlusion_strength, 0, 1);
    }
    if (gui_input_float("Smoothness", &goxel.rend.settings.smoothness,
                        0.1, 0, 1, NULL)) {
        goxel.rend.settings.smoothness =
            clamp(goxel.rend.settings.smoothness, 0, 1);
    }

    gui_checkbox_flag("Grid", &goxel.view_effects, EFFECT_GRID, NULL);
    gui_checkbox_flag("Edges", &goxel.view_effects, EFFECT_EDGES, NULL);
    gui_checkbox_flag("Unlit",
            &goxel.rend.settings.effects, EFFECT_UNLIT, NULL);
    gui_checkbox_flag("Borders",
            &goxel.rend.settings.effects, EFFECT_BORDERS, NULL);
    gui_checkbox_flag("See back",
            &goxel.rend.settings.effects, EFFECT_SEE_BACK, NULL);
    gui_checkbox_flag("Marching Cubes",
                &goxel.rend.settings.effects, EFFECT_MARCHING_CUBES, NULL);

    if (goxel.rend.settings.effects & EFFECT_MARCHING_CUBES) {
        gui_checkbox_flag("Smooth Colors", &goxel.rend.settings.effects,
                          EFFECT_MC_SMOOTH, NULL);
    }
}
