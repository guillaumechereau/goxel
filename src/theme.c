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
#include "ini.h"


#define SIZE_ATTRS(X) \
    X(item_height) \
    X(item_padding_h) \
    X(item_rounding) \
    X(item_spacing_h) \
    X(item_spacing_v) \
    X(item_inner_spacing_h)

#define COLOR_ATTRS(X) \
    X(background) \
    X(outline) \
    X(inner) \
    X(inner_selected) \
    X(text) \
    X(text_selected) \
    X(tabs) \
    X(tabs_background) \
    X(icons) \
    X(icons_selected) \

static const theme_t g_default_theme = {
    .name = "default",
    .sizes = {
        .item_height = 18,
        .item_padding_h = 4,
        .item_rounding = 4,
        .item_spacing_h = 4,
        .item_spacing_v = 4,
        .item_inner_spacing_h = 6,
    },
    .colors = {
        .background = VEC4(96, 114, 114, 255),
        .outline = VEC4(77, 77, 77, 255),
        .inner = VEC4(161, 161, 161, 255),
        .text = VEC4(17, 17, 17, 255),
        .inner_selected = VEC4(102, 102, 204, 255),
        .text_selected = VEC4(17, 17, 17, 255),
        .tabs_background = VEC4(48, 66, 66, 255),
        .tabs = VEC4(69, 86, 86, 255),
        .icons = VEC4(0, 0, 0, 255),
        .icons_selected = VEC4(0, 0, 0, 255),
    },
};

static theme_t g_theme = {};

static uint32_t tohex(uvec4b_t c)
{
    return (int)c.r << 24 |
           (int)c.g << 16 |
           (int)c.b <<  8 |
           (int)c.a <<  0;
}

static uvec4b_t parse_color(const char *s)
{
    uint32_t v;
    uvec4b_t ret;
    v = strtol(s + 1, NULL, 16);
    ret.r = (v >> 24) & 0xff;
    ret.g = (v >> 16) & 0xff;
    ret.b = (v >>  8) & 0xff;
    ret.a = (v >>  0) & 0xff;
    return ret;
}

static int theme_ini_handler(void *user, const char *section,
                             const char *name, const char *value, int lineno)
{
    if (strcmp(section, "sizes") == 0) {
        #define X_CHECK_SIZE(n) \
        if (strcmp(name, #n) == 0) g_theme.sizes.n = atoi(value);
        SIZE_ATTRS(X_CHECK_SIZE)
    }
    if (strcmp(section, "colors") == 0) {
        #define X_CHECK_COLOR(n) \
        if (strcmp(name, #n) == 0) g_theme.colors.n = parse_color(value);
        COLOR_ATTRS(X_CHECK_COLOR)
    }

    return 0;
}

void theme_set(const char *name)
{
    char *path;
    FILE *file;

    name = name ?: "default";
    asprintf(&path, "%s/themes/%s.ini", sys_get_user_dir(), name);
    file = fopen(path, "r");
    free(path);
    g_theme = g_default_theme;

    if (file) {
        ini_parse_file(file, theme_ini_handler, NULL);
        fclose(file);
    }
}

theme_t *theme_get(void)
{
    if (!(*g_theme.name)) theme_set(NULL);
    return &g_theme;
}

void theme_revert_default(void)
{
    g_theme = g_default_theme;
}

void theme_save(void)
{
    char *path;
    FILE *file;
    const theme_t *t = &g_theme;
    asprintf(&path, "%s/themes/default.ini", sys_get_user_dir());
    sys_make_dir(path);
    file = fopen(path, "w");

    fprintf(file, "[sizes]\n");
    #define X_SAVE_SIZE(n) \
    fprintf(file, #n "=%d\n", t->sizes.n);
    SIZE_ATTRS(X_SAVE_SIZE)

    fprintf(file, "[colors]\n");
    #define X_SAVE_COLOR(n) \
    fprintf(file, #n "=#%X\n", tohex(t->colors.n));
    COLOR_ATTRS(X_SAVE_COLOR)

    fclose(file);
    free(path);
}
