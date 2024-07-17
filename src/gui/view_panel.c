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
        {goxel.back_color, _("Background")},
        {goxel.grid_color, _("Grid")},
        {goxel.image_box_color, _("Box")},
    };
    int i;

    gui_text(_("Colors"));
    for (i = 0; i < (int)ARRAY_SIZE(COLORS); i++) {
        gui_color_small(tr(COLORS[i].label), COLORS[i].color);
    }
    gui_checkbox(_("Hide Box"), &goxel.hide_box, NULL);

    gui_text(_("Effects"));

    if (gui_input_float(_("Occlusion"), &goxel.rend.settings.occlusion_strength,
                        0.1, 0, 1, NULL)) {
        goxel.rend.settings.occlusion_strength =
            clamp(goxel.rend.settings.occlusion_strength, 0, 1);
    }
    if (gui_input_float(_("Smoothness"), &goxel.rend.settings.smoothness,
                        0.1, 0, 1, NULL)) {
        goxel.rend.settings.smoothness =
            clamp(goxel.rend.settings.smoothness, 0, 1);
    }

    gui_checkbox_flag(_("Grid"), &goxel.view_effects, EFFECT_GRID, NULL);
    gui_checkbox_flag(_("Edges"), &goxel.view_effects, EFFECT_EDGES, NULL);
    gui_checkbox_flag(_("Shadeless"),
            &goxel.rend.settings.effects, EFFECT_UNLIT, NULL);
    gui_checkbox_flag(_("Border"),
            &goxel.rend.settings.effects, EFFECT_BORDERS, NULL);
    gui_checkbox_flag(_("Transparent"),
            &goxel.rend.settings.effects, EFFECT_SEE_BACK, NULL);
    gui_checkbox_flag(_("Marching Cubes"),
                &goxel.rend.settings.effects, EFFECT_MARCHING_CUBES, NULL);

    if (goxel.rend.settings.effects & EFFECT_MARCHING_CUBES) {
        gui_checkbox_flag(_("Smooth"), &goxel.rend.settings.effects,
                          EFFECT_MC_SMOOTH, NULL);
    }
}
