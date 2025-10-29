/* Goxel 3D voxels editor
 *
 * copyright (c) 2015-2022 Guillaume Chereau <guillaume@noctua-software.com>
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

// File: goxel.h

#ifndef GOXEL_H
#define GOXEL_H

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#ifndef NOMINMAX
#   define NOMINMAX
#endif

#include "action.h"
#include "assets.h"
#include "block_def.h"
#include "camera.h"
#include "filters.h"
#include "gesture.h"
#include "gesture3d.h"
#include "gizmos.h"
#include "gui.h"
#include "i18n.h"
#include "image.h"
#include "inputs.h"
#include "layer.h"
#include "log.h"
#include "material.h"
#include "volume.h"
#include "volume_utils.h"
#include "model3d.h"
#include "palette.h"
#include "pathtracer.h"
#include "render.h"
#include "shape.h"
#include "system.h"
#include "theme.h"
#include "tools.h"
#include "utarray.h"
#include "uthash.h"
#include "utlist.h"

#include "utils/box.h"
#include "utils/cache.h"
#include "utils/color.h"
#include "utils/geometry.h"
#include "utils/gl.h"
#include "utils/img.h"
#include "utils/path.h"
#include "utils/plane.h"
#include "utils/sound.h"
#include "utils/texture.h"
#include "utils/vec.h"

#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOXEL_VERSION_STR "0.15.2"
#ifndef GOXEL_DEFAULT_THEME
#   define GOXEL_DEFAULT_THEME "dark"
#endif

// #### Set the DEBUG macro ####
#ifndef DEBUG
#   if !defined(NDEBUG)
#       define DEBUG 1
#   else
#       define DEBUG 0
#   endif
#endif

#if !DEBUG && !defined(NDEBUG)
#   define NDEBUG
#endif
#include <assert.h>
// #############################



// #### DEFINED macro ##########
// DEFINE(NAME) returns 1 if NAME is defined to 1, 0 otherwise.
#define DEFINED(macro) DEFINED_(macro)
#define macrotest_1 ,
#define DEFINED_(value) DEFINED__(macrotest_##value)
#define DEFINED__(comma) DEFINED___(comma 1, 0)
#define DEFINED___(_, v, ...) v
// #############################


#ifdef __EMSCRIPTEN__
#   include <emscripten.h>
#   define KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
#   define KEEPALIVE
#endif

// ####### Section: Utils ################################################

// Some useful inline functions / macros

/*
 * Macro: ARRAY_SIZE
 * Return the number of elements in an array
 */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/*
 * Macro: SWAP
 * Swap two variables
 */
#define SWAP(x0, x) do {typeof(x0) tmp = x0; x0 = x; x = tmp;} while (0)

/*
 * Macro: USER_PASS
 * Used to pass values to callback 'void *user' arguments.
 *
 * For a function with the following declaration: f(void *user), we can pass
 * several arguments packed in an array like that:
 *
 * int x, y;
 * f(USER_PASS(&x, &y));
 */
#define USER_PASS(...) ((void *)(const void *[]){__VA_ARGS__})

/*
 * Macro: USER_GET
 * Used to unpack values passed to a callback with USER_PASS:
 *
 * void f(void *user)
 * {
 *     char *arg1 = USER_GET(user, 0);
 *     int   arg2 = *((int*)USER_GET(user, 1));
 * }
 */
#define USER_GET(var, n) (((void**)var)[n])

/* Define: DR2D
 * Convertion ratio from radian to degree. */
#define DR2D (180 / M_PI)

/* Define: DR2D
 * Convertion ratio from degree to radian. */
#define DD2R (M_PI / 180)

/* Define: KB
 * 1024 */
#define KB 1024

/* Define: MB
 * 1024^2 */
#define MB (1024 * KB)

/* Define: GB
 * 1024^3 */
#define GB (1024 * MB)

/*
 * Macro: min
 * Safe min function.
 */
#define min(a, b) ({ \
      __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a < _b ? _a : _b; \
      })

/*
 * Macro: max
 * Safe max function.
 */
#define max(a, b) ({ \
      __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a > _b ? _a : _b; \
      })

/*
 * Macro: max3
 * Safe max function, return the max of three values.
 */
#define max3(x, y, z) (max((x), max((y), (z))))

/*
 * Macro: min3
 * Safe max function, return the max of three values.
 */
#define min3(x, y, z) (min((x), min((y), (z))))

/*
 * Macro: clamp
 * Clamp a value.
 */
#define clamp(x, a, b) (min(max(x, a), b))

/*
 * Macro: cmp
 * Compare two values.
 *
 * Return:
 *   +1 if a > b, -1 if a < b, 0 if a == b.
 */
#define cmp(a, b) ({ \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    (_a > _b) ? +1 : (_a < _b) ? -1 : 0; \
})

/* Function: smoothstep
 * Perform Hermite interpolation between two values.
 *
 * This is similar to the smoothstep function in OpenGL shader language.
 *
 * Parameters:
 *   edge0 - Lower edge of the Hermite function.
 *   edge1 - Upper edge of the Hermite function.
 *   x     - Source value for interpolation.
 */
static inline float smoothstep(float edge0, float edge1, float x)
{
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

/*
 * Function: mix
 * Linear blend of x and y.
 *
 * Similar to GLES mix function.
 */
static inline float mix(float x, float y, float t)
{
    return (1.0 - t) * x + t * y;
}

/* Function: set_flag
 * Set some int bits to 0 or 1.
 *
 * Parameters:
 *   x    - The int value to change.
 *   flag - Bitmask of the bits we want to set.
 *   v    - Value to set.
 */
static inline void set_flag(int *x, int flag, bool v)
{
    v ? (*x |= flag) : (*x &= ~flag);
}

/*
 * Extra texture creation function from a path, that can also be an asset.
 */
texture_t *texture_new_image(const char *path, int flags);

/*
 * Function: read_file
 * Read a file from disk.
 *
 * Return:
 *   A newly allocated buffer containing the file data.
 */
char *read_file(const char *path, int *size);

/*
 * Function: unix_to_dtf
 * Get gregorian date from unix time.
 *
 * Parameters:
 *   t  - Unix time.
 *   iy - Output year.
 *   im - Output month (1 - 12).
 *   id - Output day (1 - 31).
 *   h  - Output hour.
 *   m  - Output minute.
 *   s  - Output seconds.
 */
int unix_to_dtf(double t, int *iy, int *im, int *id, int *h, int *m, int *s);

/*
 * Function: utf_16_to_8
 * Convert a string encoded in utf_16 to utf_8.
 *
 * Parameters:
 *   in16   - Input string in utf 16 encoding.
 *   out8   - Output buffer that receive the utf8 string.
 *   size8  - Size of the output buffer.
 */
int utf_16_to_8(const wchar_t *in16, char *out8, size_t size8);

/*
 * Function: str_endswith
 * Return whether a string ends with an other one.
 */
bool str_endswith(const char *str, const char *end);

/*
 * Function: str_startswith
 * Return whether a string starts with an other one.
 */
bool str_startswith(const char *s1, const char *s2);

/*
 * Function: str_replace_ext
 * Replace the extension part of a file path with a new one.
 */
bool str_replace_ext(const char *str, const char *new_ext,
                     char *out, size_t out_size);


/*
 * Function: unproject
 * Convert from screen coordinate to world coordinates.
 *
 * Similar to gluUnproject.
 *
 * Parameters:
 *   win        - Windows coordinates to be mapped.
 *   model      - Modelview matrix.
 *   proj       - Projection matrix.
 *   viewport   - Viewport rect (x, y, w, h).
 *   out        - Output of the computed object coordinates.
 */
void unproject(const float win[3], const float model[4][4],
               const float proj[4][4], const float viewport[4],
               float out[3]);

// #### Dialogs ################
enum {
    DIALOG_FLAG_SAVE    = 1 << 0,
    DIALOG_FLAG_OPEN    = 1 << 1,
    DIALOG_FLAG_DIR     = 1 << 2,
};

// All the glyphs in the goxel texture.
#define GLYPH_MOUSE_LMB "\ue660"
#define GLYPH_MOUSE_MMB "\ue661"
#define GLYPH_MOUSE_RMB "\ue662"

// All the icons positions inside icon.png (as Y*8 + X + 1).

#define X(NAME, x, y, theme) \
    NAME = (y * 8 + x + 1) | (theme << 16)

enum {
    ICON_NULL = 0,

    X(ICON_TOOL_BRUSH,              0, 0, THEME_GROUP_ICON),
    X(ICON_TOOL_PICK,               1, 0, THEME_GROUP_ICON),
    X(ICON_TOOL_SHAPE,              2, 0, THEME_GROUP_ICON),
    X(ICON_TOOL_PLANE,              3, 0, THEME_GROUP_ICON),
    X(ICON_TOOL_LASER,              4, 0, THEME_GROUP_ICON),
    X(ICON_TOOL_MOVE,               5, 0, THEME_GROUP_ICON),
    X(ICON_TOOL_EXTRUDE,            6, 0, THEME_GROUP_ICON),
    X(ICON_TOOL_FUZZY_SELECT,       7, 0, THEME_GROUP_ICON),

    X(ICON_MODE_ADD,                0, 1, THEME_GROUP_ICON),
    X(ICON_MODE_SUB,                1, 1, THEME_GROUP_ICON),
    X(ICON_MODE_PAINT,              2, 1, THEME_GROUP_ICON),
    X(ICON_SHAPE_CUBE,              3, 1, THEME_GROUP_ICON),
    X(ICON_SHAPE_SPHERE,            4, 1, THEME_GROUP_ICON),
    X(ICON_SHAPE_CYLINDER,          5, 1, THEME_GROUP_ICON),
    X(ICON_TOOL_RECT_SELECTION,     6, 1, THEME_GROUP_ICON),
    X(ICON_TOOL_LINE,               7, 1, THEME_GROUP_ICON),

    X(ICON_ADD,                     0, 2, 0),
    X(ICON_REMOVE,                  1, 2, 0),
    X(ICON_ARROW_BACK,              2, 2, 0),
    X(ICON_ARROW_FORWARD,           3, 2, 0),
    X(ICON_LINK,                    4, 2, 0),
    X(ICON_MENU,                    5, 2, 0),
    X(ICON_DELETE,                  6, 2, 0),
    X(ICON_TOOL_PROCEDURAL,         7, 2, 0),

    X(ICON_VISIBILITY,              0, 3, 0),
    X(ICON_VISIBILITY_OFF,          1, 3, 0),
    X(ICON_ARROW_DOWNWARD,          2, 3, 0),
    X(ICON_ARROW_UPWARD,            3, 3, 0),
    X(ICON_EDIT,                    4, 3, 0),
    X(ICON_COPY,                    5, 3, 0),
    X(ICON_GALLERY,                 6, 3, 0),
    X(ICON_INFO,                    7, 3, 0),

    X(ICON_SETTINGS,                0, 4, 0),
    X(ICON_CLOUD,                   1, 4, 0),
    X(ICON_SHAPE,                   2, 4, 0),
    X(ICON_CLOSE,                   3, 4, 0),
    X(ICON_THREE_DOTS,              4, 4, 0),
    X(ICON_HAMMER,                  5, 4, THEME_GROUP_ICON_EDIT),

    X(ICON_TOOLS,                   0, 5, THEME_GROUP_ICON_EDIT),
    X(ICON_PALETTE,                 1, 5, THEME_GROUP_ICON_EDIT),
    X(ICON_LAYERS,                  2, 5, THEME_GROUP_ICON_EDIT),
    X(ICON_RENDER,                  3, 5, THEME_GROUP_ICON_RENDER),
    X(ICON_CAMERA,                  4, 5, THEME_GROUP_ICON_VIEW),
    X(ICON_IMAGE,                   5, 5, THEME_GROUP_ICON_RENDER),
    X(ICON_EXPORT,                  6, 5, THEME_GROUP_ICON_RENDER),
    X(ICON_DEBUG,                   7, 5, THEME_GROUP_ICON_OTHER),

    X(ICON_VIEW,                    0, 6, THEME_GROUP_ICON_VIEW),
    X(ICON_MATERIAL,                1, 6, THEME_GROUP_ICON_VIEW),
    X(ICON_LIGHT,                   2, 6, THEME_GROUP_ICON_VIEW),
    X(ICON_TOOL_SELECTION,          3, 6, 0),
    X(ICON_INTERSECT,               4, 6, 0),
    X(ICON_SUBTRACT,                5, 6, 0),
    X(ICON_SNAP,                    6, 6, 0),
    X(ICON_SYMMETRY,                7, 6, 0),
};

#undef X

// #### Block ##################
// The block size can only be 16.
#define BLOCK_SIZE 16
#define VOXEL_TEXTURE_SIZE 8

// Generate an optimal palette whith a fixed number of colors from a volume.
void quantization_gen_palette(const volume_t *volume, int nb,
                              uint8_t (*palette)[4]);

// #### Goxel : core object ####

// Flags to set where the mouse snap.  In order of priority.
enum {
    SNAP_IMAGE_BOX      = 1 << 0,
    SNAP_SELECTION_IN   = 1 << 1,
    SNAP_SELECTION_OUT  = 1 << 2,
    SNAP_VOLUME         = 1 << 3,
    SNAP_PLANE          = 1 << 4, // The global plane.
    SNAP_CAMERA         = 1 << 5, // Used for laser tool.

    SNAP_SHAPE_BOX      = 1 << 6, // Snap to custom box.
    SNAP_SHAPE_PLANE    = 1 << 7, // Snap to custom plane.
    SNAP_SHAPE_LINE     = 1 << 8, // Snap to custom line.
    SNAP_SHAPE_SEGMENT  = 1 << 9, // Snap to custom line.

    SNAP_ROUNDED        = 1 << 10, // Round the result.
};

typedef struct keymap {
    int action; // 0: pan, 1: rotate, 2, zoom.
    int input;  // union of GESTURE enum.
} keymap_t;

// Represent one hint text to show in the UI.
typedef struct hint {
    int flags;
    char title[128];
    char msg[128];
} hint_t;

typedef struct goxel
{
    int        screen_size[2];
    float      screen_scale;
    image_t    *image;

    // Flag so that we reinit OpenGL after the context has been killed.
    bool       graphics_initialized;
    // We can't reset the graphics in the middle of the gui, so use this.
    // for testing.
    bool       request_test_graphic_release;

    // Tools can set this volume and it will replace the current layer volume
    // during render.
    volume_t   *tool_volume;

    volume_t   *layers_volume_;
    uint32_t   layers_volume_hash;

    volume_t   *render_volume_; // All the layers + tool volume.
    uint32_t   render_volume_hash;

    layer_t    *render_layers;
    uint32_t   render_layers_hash;

    struct     {
        volume_t *volume;
        float  box[4][4];
    } clipboard;

    int        snap_mask;    // Global snap mask (can edit in the GUI).
    float      snap_offset;  // Only for brush tool, remove that?

    float      plane[4][4];         // The snapping plane.
    bool       show_export_viewport;

    uint8_t    back_color[4];
    uint8_t    grid_color[4];
    uint8_t    image_box_color[4];
    bool       hide_box;

    texture_t  *pick_fbo;
    painter_t  painter;
    renderer_t rend;

    tool_t     *tool;
    float      tool_radius;
    bool       pathtrace; // Render pathtraced mode.

    struct {
        float  rotation[4];
        float  pos[2];
        float  camera_ofs[3];
        float  camera_mat[4][4];
    } move_origin;

    palette_t  *palettes;   // The list of all the palettes
    palette_t  *palette;    // The current color palette

    // Last used colors for palette panel
    struct {
        uint8_t color[4];               // RGBA color values
        char    name[256];              // Color name for tooltip
    } last_used_colors[10];
    int        last_used_colors_count;  // Number of colors currently stored

    // Palette search functionality
    char       palette_search_query[256];    // Current search string
    palette_entry_t *filtered_entries;      // Filtered palette results
    int        filtered_count;               // Number of filtered results

    double     delta_time;  // Elapsed time since last frame (sec)
    int        frame_count; // Global frames counter.
    double     frame_time;  // Clock time at beginning of the frame (sec)
    double     fps;         // Average fps.
    bool       quit;        // Set to true to quit the application.

    int        view_effects; // EFFECT_WIREFRAME | EFFECT_GRID | EFFECT_EDGES

    // Stb array of all the gestures we listen to.
    gesture_t **gestures;

    // All the 3d gestures we listen to.
    gesture3d_t gesture3ds[32];
    int gesture3ds_count;

    pathtracer_t pathtracer;

    // Used to check if the active volume changed to play tick sound.
    uint64_t    last_volume_key;
    double      last_click_time;

    // Some stats for the UI.
    struct {
        int current_panel; // Index of the current visible control panel.
        float panel_width;
        float viewport[4];
        margins_t safe_margins;
    } gui;

    char **recent_files; // stb arraw of most recently used files.

    const char *lang; // Current set language.

    // Stb array of keymap settings (different from shortcuts).
    keymap_t *keymaps;

    // Can be set to a key code (only KEY_LEFT_ALT is supported for now).
    int emulate_three_buttons_mouse;

    // Stb arrary of hints to show on top of the screen.
    hint_t *hints;

} goxel_t;

// the global goxel instance.
extern goxel_t goxel;

// XXX: add some doc.
void goxel_init(void);
void goxel_release(void);
void goxel_reset(void);

// Probably better to merge those two.
int goxel_iter(const inputs_t *inputs);
void goxel_render(const inputs_t *inputs);

/*
 * Function: goxel_create_graphics
 * Called after the graphics context has been created.
 */
void goxel_create_graphics(void);

/*
 * Function: goxel_release_graphics
 * Called before the graphics context gets destroyed.
 */
void goxel_release_graphics(void);

/*
 * Function: goxel_on_low_memory
 * Attempt to release cached memory
 */
void goxel_on_low_memory(void);

int goxel_unproject(const float viewport[4],
                    const float pos[2], int snap_mask,
                    const float snap_shape[4][4],
                    float offset,
                    float out[3], float normal[3]);

void goxel_render_view(const float viewport[4], bool render_mode);
void goxel_render_export_view(const float viewport[4]);
// Called by the gui when the mouse hover a 3D view.
// XXX: change the name since we also call it when the mouse get out of
// the view.
void goxel_mouse_in_view(const float viewport[4], const inputs_t *inputs,
                         bool capture_keys);

const volume_t *goxel_get_layers_volume(const image_t *img);
const volume_t *goxel_get_render_volume(const image_t *img);

/*
 * Function: goxel_get_render_layers
 * Compute merged current image layer list
 *
 * This returns a simplified list of layers from the current image where
 * we merged as many layers as possible into a single one.
 *
 * It also can replace the current layer volume with the tool preview.
 *
 * This is the function that should be used the get the actual list of layers
 * to be rendered.
 */
const layer_t *goxel_get_render_layers(bool with_tool_preview);

enum {
    HINT_LARGE = 1 << 2,
    HINT_COORDINATES = 1 << 3,
};

void goxel_add_hint(int flags, const char *title, const char *msg);

void goxel_import_image_plane(const char *path);

int goxel_import_file(const char *path, const char *format);
int goxel_export_to_file(const char *path, const char *format);

// Function to add a color to the last used colors list
void goxel_add_to_last_used_colors(const uint8_t color[4], const char *name);

// Palette search functions
void goxel_filter_palette(const char *query);
void goxel_clear_palette_search(void);

// Render the view into an RGB[A] buffer.
void goxel_render_to_buf(uint8_t *buf, int w, int h, int bpp);

void goxel_open_file(const char *path);

void save_to_file(const image_t *img, const char *path);
int load_from_file(const char *path, bool replace);

// Iter info of a gox file, without actually reading it.
// For the moment only returns the image preview if available.
int gox_iter_infos(const char *path,
                   int (*callback)(const char *attr, int size,
                                   void *value, void *user),
                   void *user);


void settings_load(void);
void settings_save(void);

void goxel_add_recent_file(const char *path);

/*
 * goxel_apply_color_filter
 * Apply a color filter to all the current selected voxels.
 *
 * This is a conveniance function so that we don't have to handle the case
 * where we have a selection mask or not.
 */
void goxel_apply_color_filter(
        void (*fn)(void *args, uint8_t color[4]), void *args);

/*
 * Process a 3d gesture.
 *
 * Used by the tools to react to 3d gesture.
 * Should be called once per frame for each gesture we are listening to.
 */
bool goxel_gesture3d(const gesture3d_t *gesture);

/*
 * Call this if the keymaps array has changed.
 */
void goxel_update_keymaps(void);

// Section: tests

/* Function: tests_run
 * Run all the unit tests */
void tests_run(void);


#endif // GOXEL_H
