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
#include <errno.h> // IWYU pragma: keep.

#include "../ext_src/stb/stb_ds.h"

#include "utils/ini.h"

static int shortcut_callback(action_t *action, void *user)
{
    if (!(action->flags & ACTION_CAN_EDIT_SHORTCUT)) return 0;
    gui_push_id(action->id);
    if (action->help)
        gui_text("%s: %s", action->id, tr(action->help));
    else
        gui_text("%s", action->id);
    gui_next_column();
    // XXX: need to check if the inputs are valid!
    gui_input_text("", action->shortcut, sizeof(action->shortcut));
    gui_next_column();
    gui_pop_id();
    return 0;
}

static void on_keymap(int idx, keymap_t *keymap)
{
    int i;
    char id[32];
    bool selected;
    const char *preview = "";

    const struct {
        const char *label;
        int input;
    } input_choices[] = {
        { "Right Mouse", GESTURE_RMB },
        { "Middle Mouse", GESTURE_MMB },
        { "Ctrl Right Mouse", GESTURE_CTRL | GESTURE_RMB },
        { "Ctrl Middle Mouse", GESTURE_CTRL | GESTURE_MMB },
        { "Shift Right Mouse", GESTURE_SHIFT | GESTURE_RMB },
        { "Shift Middle Mouse", GESTURE_SHIFT | GESTURE_MMB },
    };

    const char *action_choices[] = { "Pan", "Rotate", "Zoom" };

    if (keymap->action < 0 || keymap->action > 2) {
        return;
    }

    snprintf(id, sizeof(id), "keymap_%d", idx);
    gui_push_id(id);

    if (gui_combo_begin("##action", action_choices[keymap->action])) {
        for (i = 0; i < ARRAY_SIZE(action_choices); i++) {
            selected = i == keymap->action;
            if (gui_combo_item(action_choices[i], selected)) {
                keymap->action = input_choices[i].input;
                settings_save();
            }
        }
        gui_combo_end();
    }
    gui_next_column();

    for (i = 0; i < ARRAY_SIZE(input_choices); i++) {
        if (input_choices[i].input == keymap->input) {
            preview = input_choices[i].label;
            break;
        }
    }

    if (gui_combo_begin("##input", preview)) {
        for (i = 0; i < ARRAY_SIZE(input_choices); i++) {
            selected = input_choices[i].input == keymap->input;
            if (gui_combo_item(input_choices[i].label, selected)) {
                keymap->input = input_choices[i].input;
                settings_save();
            }
        }
        gui_combo_end();
    }

    gui_next_column();

    if (gui_button("Delete", 0, 0)) {
        arrdel(goxel.keymaps, idx);
    }

    gui_next_column();

    gui_pop_id();
}

static void on_add_keymap_button(void)
{
    keymap_t keymap = { 0, GESTURE_RMB };
    arrput(goxel.keymaps, keymap);
    settings_save();
}

int gui_settings_popup(void *data)
{
    const char *names[128];
    theme_t *theme;
    int i, nb, current;
    theme_t *themes = theme_get_list();
    int ret = 0;
    const tr_lang_t *language;
    const tr_lang_t *languages;

    if (gui_section_begin(_(LANGUAGE), GUI_SECTION_COLLAPSABLE)) {
        language = tr_get_language();
        if (gui_combo_begin("##lang", language->name)) {
            languages = tr_get_supported_languages();
            for (i = 0; languages[i].id; i++) {
                if (gui_combo_item(languages[i].name,
                            &languages[i] == language)) {
                    // Note: we don't change the language yet, we do it in
                    // goxel_iter so not to mess up the UI render.
                    goxel.lang = languages[i].id;
                    settings_save();
                }
            }
            gui_combo_end();
        }
    } gui_section_end();

    if (gui_section_begin(_(THEME), GUI_SECTION_COLLAPSABLE)) {
        DL_COUNT(themes, theme, nb);
        i = 0;
        DL_FOREACH(themes, theme) {
            if (strcmp(theme->name, theme_get()->name) == 0) current = i;
            names[i++] = theme->name;
        }
        if (gui_combo("##themes", &current, names, nb)) {
            theme_set(names[current]);
            settings_save();
        }
    } gui_section_end();


    if (gui_section_begin(_(PATHS), GUI_SECTION_COLLAPSABLE_CLOSED)) {
        gui_text("Palettes: %s/palettes", sys_get_user_dir());
        gui_text("Progs: %s/progs", sys_get_user_dir());
    } gui_section_end();

    if (gui_section_begin(_(SHORTCUTS), GUI_SECTION_COLLAPSABLE_CLOSED)) {
        gui_columns(2);
        gui_separator();
        actions_iter(shortcut_callback, NULL);
        gui_separator();
        gui_columns(1);
    } gui_section_end();

    if (gui_section_begin("Keymaps", GUI_SECTION_COLLAPSABLE_CLOSED)) {
        gui_columns(3);
        gui_separator();
        for (i = 0; i < arrlen(goxel.keymaps); i++) {
            on_keymap(i, &goxel.keymaps[i]);
        }
        gui_separator();
        gui_columns(1);
        if (gui_button(_(ADD), 0, 0)) {
            on_add_keymap_button();
        }
    }
    gui_section_end();

    gui_popup_bottom_begin();
    ret = gui_button(_(OK), 0, 0);
    gui_popup_bottom_end();
    return ret;
}

static void add_keymap(const char *name, const char *value)
{
    keymap_t keymap = {
        .action = -1,
        .input = 0,
    };

    if (strcmp(name, "pan") == 0) keymap.action = 0;
    if (strcmp(name, "rotate") == 0) keymap.action = 1;
    if (strcmp(name, "zoom") == 0) keymap.action = 2;

    if (strcasestr(value, "right mouse")) keymap.input |= GESTURE_RMB;
    if (strcasestr(value, "middle mouse")) keymap.input |= GESTURE_MMB;
    if (strcasestr(value, "shift")) keymap.input |= GESTURE_SHIFT;
    if (strcasestr(value, "ctrl")) keymap.input |= GESTURE_CTRL;

    if (keymap.action == -1 || keymap.input == 0) {
        LOG_W("Cannot parse keymap %s = %s", name, value);
        return;
    }

    arrput(goxel.keymaps, keymap);
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
        if (strcmp(name, "language") == 0) {
            tr_set_language(value);
            goxel.lang = tr_get_language()->id;
        }
    }
    if (strcmp(section, "shortcuts") == 0) {
        a = action_get_by_name(name);
        if (a) {
            strncpy(a->shortcut, value, sizeof(a->shortcut) - 1);
        } else {
            LOG_W("Cannot set shortcut for unknown action '%s'", name);
        }
    }
    if (strcmp(section, "keymaps") == 0) {
        add_keymap(name, value);
    }
    return 0;
}

void settings_load(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/settings.ini", sys_get_user_dir());
    LOG_I("Read settings file: %s", path);
    arrfree(goxel.keymaps);
    ini_parse(path, settings_ini_handler, NULL);
    actions_check_shortcuts();
}

static int shortcut_save_callback(action_t *a, void *user)
{
    FILE *file = user;
    if (strcmp(a->shortcut, a->default_shortcut ?: "") != 0)
        fprintf(file, "%s=%s\n", a->id, a->shortcut);
    return 0;
}

static void save_keymaps(FILE *file)
{
    int i, action, input;

    fprintf(file, "[keymaps]\n");
    for (i = 0; i < arrlen(goxel.keymaps); i++) {
        action = goxel.keymaps[i].action;
        input = goxel.keymaps[i].input;
        switch (action) {
        case 0:
            fprintf(file, "pan=");
            break;
        case 1:
            fprintf(file, "rotate=");
            break;
        case 2:
            fprintf(file, "zoom=");
            break;
        }
        if (input & GESTURE_CTRL) {
            fprintf(file, "Ctrl ");
        }
        if (input & GESTURE_SHIFT) {
            fprintf(file, "Shift ");
        }
        if (input & GESTURE_MMB) {
            fprintf(file, "Middle Mouse");
        }
        if (input & GESTURE_RMB) {
            fprintf(file, "Right Mouse");
        }
        fprintf(file, "\n");
    }
}

void settings_save(void)
{
    char path[1024];
    FILE *file;

    snprintf(path, sizeof(path), "%s/settings.ini", sys_get_user_dir());
    LOG_I("Save settings to %s", path);
    sys_make_dir(path);
    file = fopen(path, "w");
    if (!file) {
        LOG_E("Cannot save settings to %s: %s", path, strerror(errno));
        return;
    }
    fprintf(file, "[ui]\n");
    fprintf(file, "theme=%s\n", theme_get()->name);
    fprintf(file, "language=%s\n", goxel.lang);
    fprintf(file, "\n");

    fprintf(file, "[shortcuts]\n");
    actions_iter(shortcut_save_callback, file);

    fprintf(file, "\n");
    save_keymaps(file);
    fprintf(file, "\n");

    fclose(file);
}
