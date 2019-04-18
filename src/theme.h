/* Goxel 3D voxels editor
 *
 * copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
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

#ifndef THEME_H
#define THEME_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Section: Theme
 *
 * Theme structures and functions used to set goxel ui colors.
 * The theme is split into groups representing different part of the gui:
 * the widgets, the tab, the menus, etc.
 *
 * For each group we can specify a set of colors:
 * - Background
 * - Outline
 * - Inner
 * - Inner selected
 * - Text
 * - Text selected
 *
 * If a color is not set in a group, we can get it from its parent group.
 *
 * The theme also keep track of the item sizes, but this is not supposed to
 * be editable so the code is a bit different.
 */


/*
 * Enum: THEME_GROUP
 * Enum all the theme elements groups.
 */
enum {
    THEME_GROUP_BASE,
    THEME_GROUP_WIDGET,
    THEME_GROUP_TAB,
    THEME_GROUP_MENU,
    THEME_GROUP_COUNT
};

/*
 * Enum: THEME_COLOR
 * Enum of all the theme colors.
 */
enum {
    THEME_COLOR_BACKGROUND,
    THEME_COLOR_OUTLINE,
    THEME_COLOR_INNER,
    THEME_COLOR_INNER_SELECTED,
    THEME_COLOR_TEXT,
    THEME_COLOR_TEXT_SELECTED,
    THEME_COLOR_COUNT
};

/*
 * Type: theme_group_info_t
 * Informations about a given theme group, that can be used for a theme
 * editor.
 *
 * Attributes:
 *   name   - Name of this group (id).
 *   parent - Index of the parent group.
 *   colors - Bool array of all the colors we can set in this group.
 */
typedef struct {
    const char *name;
    int parent;
    bool colors[THEME_COLOR_COUNT];
} theme_group_info_t;

extern theme_group_info_t THEME_GROUP_INFOS[THEME_GROUP_COUNT];

/*
 * Type: theme_color_info_t
 * Informations about a given theme color type, that can be used for a theme
 * editor.
 *
 * Attributes:
 *   name   - Name of this color (id).
 */
typedef struct {
    const char *name;
} theme_color_info_t;

extern theme_color_info_t THEME_COLOR_INFOS[THEME_COLOR_COUNT];


/*
 * Type: theme_group_t
 * Contains all the colors set for a given group.
 */
typedef struct {
    uint8_t colors[THEME_COLOR_COUNT][4];
} theme_group_t;

/*
 * Macro: THEME_SIZES
 * X macro to list all the themes size attributes.
 */
#define THEME_SIZES(X) \
    X(item_height) \
    X(icons_height) \
    X(item_padding_h) \
    X(item_rounding) \
    X(item_spacing_h) \
    X(item_spacing_v) \
    X(item_inner_spacing_h)


typedef struct theme theme_t;
struct theme {
    char name[64];

    struct {
        #define X(attr) int attr;
        THEME_SIZES(X)
        #undef X
    } sizes;

    theme_group_t groups[THEME_GROUP_COUNT];

    theme_t *prev, *next; // Global list of themes.
};

// Return the current theme.
theme_t *theme_get(void);
theme_t *theme_get_list(void);
void theme_revert_default(void);
void theme_save(void);
void theme_get_color(int group, int color, bool selected, uint8_t out[4]);
void theme_set(const char *name);

#endif // THEME_H
