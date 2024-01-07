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
    STR_BACKGROUND,
    STR_BOUNDED,
    STR_BORDER,
    STR_BOX,
    STR_BRUSH,
    STR_CAMERAS,
    STR_CLEAR,
    STR_CLONE,
    STR_COLOR,
    STR_COLOR_PICKER,
    STR_COLORS,
    STR_COPY,
    STR_CROP,
    STR_CROP_TO_IMAGE,
    STR_CUBE,
    STR_CYLINDER,
    STR_DEBUG,
    STR_DUPLICATE,
    STR_EDGES,
    STR_EDIT,
    STR_EFFECTS,
    STR_EXPORT,
    STR_EXTRUDE,
    STR_FILE,
    STR_FRONT,
    STR_FUZZY_SELECT,
    STR_GRID,
    STR_HIDE_BOX,
    STR_IMAGE,
    STR_IMPORT,
    STR_LASER,
    STR_LAYERS,
    STR_LEFT,
    STR_LIGHT,
    STR_LINE,
    STR_MARCHING_CUBES,
    STR_MASK,
    STR_MATERIAL,
    STR_MATERIALS,
    STR_MERGE,
    STR_MOVE,
    STR_NEW,
    STR_OCCLUSION,
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
    STR_SHADELESS,
    STR_SHAPE,
    STR_SIZE,
    STR_SMOOTH,
    STR_SMOOTHNESS,
    STR_SNAP,
    STR_SPHERE,
    STR_SUB,
    STR_SYMMETRY,
    STR_TOOLS,
    STR_TOP,
    STR_TRANSPARENT,
    STR_UNLINK,
    STR_UNDO,
    STR_VIEW,
    STR_VOLUME,
};

const char *tr(int str);

#define _(x) tr(STR_##x)
