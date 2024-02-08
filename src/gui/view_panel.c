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
        int        label;
    } COLORS[] = {
        {goxel.back_color, STR_BACKGROUND},
        {goxel.grid_color, STR_GRID},
        {goxel.image_box_color, STR_BOX},
    };
    int i;

    gui_text(_(COLORS));
    for (i = 0; i < (int)ARRAY_SIZE(COLORS); i++) {
        gui_color_small(tr(COLORS[i].label), COLORS[i].color);
    }
    gui_checkbox(_(HIDE_BOX), &goxel.hide_box, NULL);

    gui_text(_(EFFECTS));

    if (gui_input_float(_(OCCLUSION), &goxel.rend.settings.occlusion_strength,
                        0.1, 0, 1, NULL)) {
        goxel.rend.settings.occlusion_strength =
            clamp(goxel.rend.settings.occlusion_strength, 0, 1);
    }
    if (gui_input_float(_(SMOOTHNESS), &goxel.rend.settings.smoothness,
                        0.1, 0, 1, NULL)) {
        goxel.rend.settings.smoothness =
            clamp(goxel.rend.settings.smoothness, 0, 1);
    }

    gui_checkbox_flag(_(GRID), &goxel.view_effects, EFFECT_GRID, NULL);
    gui_checkbox_flag(_(EDGES), &goxel.view_effects, EFFECT_EDGES, NULL);
    gui_checkbox_flag(_(SHADELESS),
            &goxel.rend.settings.effects, EFFECT_UNLIT, NULL);
    gui_checkbox_flag(_(BORDER),
            &goxel.rend.settings.effects, EFFECT_BORDERS, NULL);
    gui_checkbox_flag(_(TRANSPARENT),
            &goxel.rend.settings.effects, EFFECT_SEE_BACK, NULL);
    gui_checkbox_flag(_(MARCHING_CUBES),
                &goxel.rend.settings.effects, EFFECT_MARCHING_CUBES, NULL);

    if (goxel.rend.settings.effects & EFFECT_MARCHING_CUBES) {
        gui_checkbox_flag(_(SMOOTH), &goxel.rend.settings.effects,
                          EFFECT_MC_SMOOTH, NULL);
    }
}
