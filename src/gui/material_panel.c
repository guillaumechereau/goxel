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

static void material_advanced_panel(void)
{
    float v;

    gui_push_id("render_advanced");
    gui_group_begin(NULL);
    v = goxel.rend.settings.occlusion_strength;
    if (gui_input_float("occlusion", &v, 0.1, 0.0, 1.0, NULL)) {
        v = clamp(v, 0, 1); \
        goxel.rend.settings.occlusion_strength = v;
    }
#define MAT_FLOAT(name, min, max) \
    v = goxel.rend.settings.name;  \
    if (gui_input_float(#name, &v, 0.1, min, max, NULL)) { \
        v = clamp(v, min, max); \
        goxel.rend.settings.name = v; \
    }

    MAT_FLOAT(ambient, 0, 1);
    MAT_FLOAT(metallic, 0, 1);
    MAT_FLOAT(roughness, 0, 1);
    MAT_FLOAT(smoothness, 0, 1);

#undef MAT_FLOAT
    gui_group_end();

    gui_checkbox_flag("Unlit",
            &goxel.rend.settings.effects, EFFECT_UNLIT, NULL);
    gui_checkbox_flag("Borders",
            &goxel.rend.settings.effects, EFFECT_BORDERS, NULL);
    gui_checkbox_flag("See back",
            &goxel.rend.settings.effects, EFFECT_SEE_BACK, NULL);
    if (gui_checkbox_flag("Marching Cubes",
                &goxel.rend.settings.effects, EFFECT_MARCHING_CUBES, NULL)) {
        goxel.rend.settings.smoothness = 1;
    }
    if (goxel.rend.settings.effects & EFFECT_MARCHING_CUBES) {
        gui_checkbox_flag("Flat", &goxel.rend.settings.effects, EFFECT_FLAT,
                          NULL);
    }
    gui_pop_id();
}


void gui_material_panel(void)
{
    int i, current = -1;
    int nb = render_get_default_settings(0, NULL, NULL);
    float v;
    char *name;
    const char **names;
    render_settings_t settings;

    names = (const char**)calloc(nb, sizeof(*names));
    for (i = 0; i < nb; i++) {
        render_get_default_settings(i, &name, &settings);
        names[i] = name;
        if (memcmp(&goxel.rend.settings, &settings,
                         sizeof(settings)) == 0)
            current = i;
    }
    gui_text("Presets:");
    if (gui_combo("##Presets", &current, names, nb)) {
        render_get_default_settings(current, NULL, &settings);
        goxel.rend.settings = settings;
    }
    free(names);

    if (!DEFINED(GOXEL_NO_SHADOW)) {
        v = goxel.rend.settings.shadow;
        if (gui_input_float("shadow", &v, 0.1, 0, 0, NULL)) {
            goxel.rend.settings.shadow = clamp(v, 0, 1);
        }
    }
    material_advanced_panel();
}

