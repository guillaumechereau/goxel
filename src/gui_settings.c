/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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


bool gui_settings_popup(void)
{
    const char **names;
    theme_t *theme;
    int i, nb, current;
    theme_t *themes = theme_get_list();

    gui_popup_body_begin();

    DL_COUNT(themes, theme, nb);
    names = (const char**)calloc(nb, sizeof(*names));
    i = 0;
    DL_FOREACH(themes, theme) {
        if (strcmp(theme->name, theme_get()->name) == 0) current = i;
        names[i++] = theme->name;
    }

    gui_text("theme");
    if (gui_combo("##themes", &current, names, nb)) {
        theme_set(names[current]);
    }

    free(names);

    // For the moment I disable the theme editor!
#if 0
    int group, color;
    theme_t *theme = theme_get();
    ImVec4 fcolor;

    gui_group_begin("Sizes");
    #define X(a) gui_input_int(#a, &theme->sizes.a, 0, 1000);
    THEME_SIZES(X)
    #undef X
    gui_group_end();

    for (group = 0; group < THEME_GROUP_COUNT; group++) {
        if (ImGui::CollapsingHeader(THEME_GROUP_INFOS[group].name)) {
            for (color = 0; color < THEME_COLOR_COUNT; color++) {
                if (!THEME_GROUP_INFOS[group].colors[color]) continue;
                fcolor = theme->groups[group].colors[color];
                if (ImGui::ColorEdit4(THEME_COLOR_INFOS[color].name,
                                      (float*)&fcolor)) {
                    theme->groups[group].colors[color] = fcolor;
                }
            }
        }
    }

    if (ImGui::Button("Revert")) theme_revert_default();
    ImGui::SameLine();
    if (ImGui::Button("Save")) theme_save();
#endif
    gui_popup_body_end();
    gui_same_line();
    return gui_button("OK", 0, 0);
}
