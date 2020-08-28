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
#include <errno.h>

#include "utils/ini.h"

static int shortcut_callback(action_t *action, void *user)
{
    if (!(action->flags & ACTION_CAN_EDIT_SHORTCUT)) return 0;
    gui_push_id(action->id);
    gui_text("%s: %s", action->id, action->help);
    gui_next_column();
    // XXX: need to check if the inputs are valid!
    gui_input_text("", action->shortcut, sizeof(action->shortcut));
    gui_next_column();
    gui_pop_id();
    return 0;
}


int gui_settings_popup(void *data)
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
    int group;
    uint8_t *color;
    theme = theme_get();

    gui_group_begin("Sizes");
    #define X(a) gui_input_int(#a, &theme->sizes.a, 0, 1000);
    THEME_SIZES(X)
    #undef X
    gui_group_end();

    for (group = 0; group < THEME_GROUP_COUNT; group++) {
        if (gui_collapsing_header(THEME_GROUP_INFOS[group].name)) {
            for (i = 0; i < THEME_COLOR_COUNT; i++) {
                if (!THEME_GROUP_INFOS[group].colors[i]) continue;
                color = theme->groups[group].colors[i];
                gui_color(THEME_COLOR_INFOS[i].name, color);
            }
        }
    }

    if (gui_button("Revert", 0, 0)) theme_revert_default();
    gui_same_line();
    if (gui_button("Save", 0, 0)) theme_save();
#endif
    if (gui_collapsing_header("Paths", false)) {
        gui_text("Palettes: %s/palettes", sys_get_user_dir());
        gui_text("Progs: %s/progs", sys_get_user_dir());
    }

    if (gui_collapsing_header("Shortcuts", false)) {
        gui_columns(2);
        gui_separator();
        actions_iter(shortcut_callback, NULL);
        gui_separator();
        gui_columns(1);
    }

    gui_popup_body_end();
    if (gui_button("Save", 0, 0))
        settings_save();
    gui_same_line();
    return gui_button("OK", 0, 0);
}

static int settings_ini_handler(void *user, const char *section,
                                const char *name, const char *value,
                                int lineno)
{
    action_t *a;
    if (strcmp(section, "ui") == 0) {
        if (strcmp(name, "theme") == 0) {
            theme_set(value);
        }
    }
    if (strcmp(section, "shortcuts") == 0) {
        if ((a = action_get_by_name(name))) {
            strncpy(a->shortcut, value, sizeof(a->shortcut) - 1);
        } else {
            LOG_W("Cannot set shortcut for unknown action '%s'", name);
        }
    }
    return 0;
}

void settings_load(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/settings.ini", sys_get_user_dir());
    ini_parse(path, settings_ini_handler, NULL);
}

static int shortcut_save_callback(action_t *a, void *user)
{
    FILE *file = user;
    if (strcmp(a->shortcut, a->default_shortcut ?: "") != 0)
        fprintf(file, "%s=%s\n", a->id, a->shortcut);
    return 0;
}

void settings_save(void)
{
    char path[1024];
    FILE *file;
    snprintf(path, sizeof(path), "%s/settings.ini", sys_get_user_dir());
    sys_make_dir(path);
    file = fopen(path, "w");
    if (!file) {
        LOG_E("Cannot save settings to %s: %s", path, strerror(errno));
        return;
    }
    fprintf(file, "[ui]\n");
    fprintf(file, "theme=%s\n", theme_get()->name);

    fprintf(file, "[shortcuts]\n");
    actions_iter(shortcut_save_callback, file);

    fclose(file);
}
