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
    STR_ABOVE,
    STR_ABSOLUTE,
    STR_ACTION_ADD_SELECTION_HELP,
    STR_ACTION_CLONE_LAYER_HELP,
    STR_ACTION_CUT_AS_NEW_LAYER,
    STR_ACTION_FILL_SELECTION_HELP,
    STR_ACTION_IMAGE_AUTO_RESIZE_HELP,
    STR_ACTION_IMAGE_LAYER_TO_VOLUME_HELP,
    STR_ACTION_LAYER_TO_VOLUME_HELP,
    STR_ACTION_NEW_SHAPE_LAYER_HELP,
    STR_ACTION_RESET_HELP,
    STR_ACTION_SET_MODE_ADD_HELP,
    STR_ACTION_SET_MODE_PAINT_HELP,
    STR_ACTION_SET_MODE_SUB_HELP,
    STR_ACTION_SUB_SELECTION_HELP,
    STR_ACTION_TOGGLE_MODE_HELP,
    STR_ACTION_UNLINK_LAYER_HELP,
    STR_ACTION_VIEW_TOGGLE_ORTHO_HELP,
    STR_ADD,
    STR_ADJUSTEMENTS,
    STR_ALPHA,
    STR_ANGLE,
    STR_ANTIALIASING,
    STR_BACKGROUND,
    STR_BELOW,
    STR_BORDER,
    STR_BOUNDED,
    STR_BOX,
    STR_BRUSH,
    STR_CAMERAS,
    STR_CANCEL,
    STR_CLEAR,
    STR_CLONE,
    STR_COLOR,
    STR_COLORS,
    STR_COLOR_PICKER,
    STR_COPY,
    STR_CROP,
    STR_CROP_TO_IMAGE,
    STR_CUBE,
    STR_CUT,
    STR_CUT_TO_NEW_LAYER,
    STR_CYLINDER,
    STR_DEBUG,
    STR_DISTANCE,
    STR_DUPLICATE,
    STR_EDGES,
    STR_EDIT,
    STR_EFFECTS,
    STR_ENVIRONMENT,
    STR_EXPORT,
    STR_EXTRUDE,
    STR_FILE,
    STR_FILE_HAS_UNSAVED_CHANGES,
    STR_FILL,
    STR_FIT_TO_CONTENT,
    STR_FIXED,
    STR_FLOOR,
    STR_FORMAT,
    STR_FRONT,
    STR_FUZZY_SELECT,
    STR_GRID,
    STR_HIDE_BOX,
    STR_IMAGE,
    STR_IMPORT,
    STR_INTENSITY,
    STR_LANGUAGE,
    STR_LASER,
    STR_LAYERS,
    STR_LEFT,
    STR_LIGHT,
    STR_LINE,
    STR_MARCHING_CUBES,
    STR_MASK,
    STR_MATERIAL,
    STR_MATERIALS,
    STR_MERGE_DOWN,
    STR_MERGE_VISIBLE,
    STR_MOVE,
    STR_NEW,
    STR_NONE,
    STR_OCCLUSION,
    STR_OFFSET,
    STR_OK,
    STR_OPEN,
    STR_ORIGIN,
    STR_ORTHOGRAPHIC,
    STR_OVERWRITE_EXPORT,
    STR_PAINT,
    STR_PALETTE,
    STR_PASTE,
    STR_PATHS,
    STR_PERSPECTIVE,
    STR_PLANE,
    STR_QUIT,
    STR_RECT_SELECT,
    STR_REDO,
    STR_RELATIVE,
    STR_RENDER,
    STR_RESET,
    STR_RESIZE,
    STR_RESTART,
    STR_RIGHT,
    STR_ROTATION,
    STR_SAMPLES,
    STR_SAVE,
    STR_SAVE_AS,
    STR_SAVE_COLORS_AS_VERTEX_ATTRIBUTE,
    STR_SELECTION,
    STR_SELECTION_IN,
    STR_SELECTION_OUT,
    STR_SELECT_BY_COLOR,
    STR_SET,
    STR_SETTINGS,
    STR_SHADELESS,
    STR_SHADOW,
    STR_SHAPE,
    STR_SHORTCUTS,
    STR_SIMPLIFY,
    STR_SIZE,
    STR_SKY,
    STR_SMOOTH,
    STR_SMOOTHNESS,
    STR_SNAP,
    STR_SPHERE,
    STR_START,
    STR_STOP,
    STR_SUB,
    STR_SUCCESS,
    STR_SYMMETRY,
    STR_THEME,
    STR_THRESHOLD,
    STR_TOOLS,
    STR_TOP,
    STR_TRANSLATION,
    STR_TRANSPARENT,
    STR_TRANSPARENT_BACKGROUND,
    STR_UNDO,
    STR_UNIFORM,
    STR_UNLINK,
    STR_USE_Y_UP_CONVENTION,
    STR_VERTEX_COLOR,
    STR_VIEW,
    STR_VISIBLE,
    STR_VOLUME,
    STR_WORLD,
    STR_Y_UP,
};

const char *tr(int str);

#define _(x) tr(STR_##x)

typedef struct {
    const char *id;
    const char *name;
} tr_lang_t;

void tr_set_language(const char *id);
const tr_lang_t *tr_get_language(void);
const tr_lang_t *tr_get_supported_languages(void);
