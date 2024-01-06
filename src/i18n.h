/* Goxel 3D voxels editor
 *
 * copyright (c) 2015-present Guillaume Chereau <guillaume@noctua-software.com>
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

/*
 * Simple support for locale strings: all the strings used by the interface
 * should be added in this list, and then defined in strings.c for all
 * supported languages.
 *
 * Then in the code we can use _() to get the locale translation, like
 * _(ADD).  For dynamic support we can also use tr(STR_ADD).
 */

#pragma once

enum {
    STR_NULL = 0,

    // Keep it sorted alphanumerically.
    STR_2D_IMAGE,
    STR_ADD,
    STR_ADJUSTEMENTS,
    STR_ALPHA,
    STR_ANTIALIASING,
    STR_BRUSH,
    STR_CAMERAS,
    STR_CLEAR,
    STR_COLOR,
    STR_COLOR_PICKER,
    STR_COPY,
    STR_CUBE,
    STR_CYLINDER,
    STR_DEBUG,
    STR_EDIT,
    STR_EXPORT,
    STR_EXTRUDE,
    STR_FILE,
    STR_FUZZY_SELECT,
    STR_FRONT,
    STR_IMAGE,
    STR_IMAGE_CUBE,
    STR_IMPORT,
    STR_LASER,
    STR_LAYERS,
    STR_LIGHT,
    STR_LINE,
    STR_LEFT,
    STR_MASK,
    STR_MATERIALS,
    STR_MOVE,
    STR_NEW,
    STR_OFFSET,
    STR_OPEN,
    STR_PAINT,
    STR_PALETTE,
    STR_PASTE,
    STR_PLANE,
    STR_QUIT,
    STR_RECT_SELECT,
    STR_REDO,
    STR_RENDER,
    STR_RESET,
    STR_RESIZE,
    STR_RIGHT,
    STR_SAVE,
    STR_SAVE_AS,
    STR_SELECTION,
    STR_SELECTION_IN,
    STR_SELECTION_OUT,
    STR_SET,
    STR_SETTINGS,
    STR_SHAPE,
    STR_SIZE,
    STR_SNAP,
    STR_SPHERE,
    STR_SUB,
    STR_SYMMETRY,
    STR_TOOLS,
    STR_TOP,
    STR_UNDO,
    STR_VIEW,
    STR_VOLUME,
};

const char *tr(int str);

#define _(x) tr(STR_##x)
