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
#include "gesture.h"
#include "gesture3d.h"
#include "gui.h"
#include "image.h"
#include "inputs.h"
#include "layer.h"
#include "log.h"
#include "material.h"
#include "mesh.h"
#include "mesh_utils.h"
#include "model3d.h"
#include "noc_file_dialog.h"
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
#include "utils/gl.h"
#include "utils/img.h"
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

#define GOXEL_VERSION_STR "0.10.8"
#ifndef GOXEL_DEFAULT_THEME
#   define GOXEL_DEFAULT_THEME "original"
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

// CHECK is similar to an assert, but the condition is tested even in release
// mode.
#if DEBUG
    #define CHECK(c) assert(c)
#else
    #define CHECK(c) do { \
        if (!(c)) { \
            LOG_E("Error %s %s %d", __func__,  __FILE__, __LINE__); \
            exit(-1); \
        } \
    } while (0)
#endif

// I redefine asprintf so that if the function fails, we just crash the
// application.  I don't see how we can recover from an asprintf fails
// anyway.
#define asprintf(...) CHECK(asprintf(__VA_ARGS__) != -1)
#define vasprintf(...) CHECK(vasprintf(__VA_ARGS__) != -1)

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
#define USER_PASS(...) ((const void*[]){__VA_ARGS__})

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

// All the icons positions inside icon.png (as Y*8 + X + 1).
enum {
    ICON_NULL = 0,

    ICON_TOOL_BRUSH = 1,
    ICON_TOOL_PICK = 2,
    ICON_TOOL_SHAPE = 3,
    ICON_TOOL_PLANE = 4,
    ICON_TOOL_LASER = 5,
    ICON_TOOL_MOVE = 6,
    ICON_TOOL_EXTRUDE = 7,
    ICON_TOOL_FUZZY_SELECT = 8,

    ICON_MODE_ADD = 9,
    ICON_MODE_SUB = 10,
    ICON_MODE_PAINT = 11,
    ICON_SHAPE_CUBE = 12,
    ICON_SHAPE_SPHERE = 13,
    ICON_SHAPE_CYLINDER = 14,
    ICON_TOOL_SELECTION = 15,
    ICON_TOOL_LINE = 16,

    ICON_ADD = 17,
    ICON_REMOVE = 18,
    ICON_ARROW_BACK = 19,
    ICON_ARROW_FORWARD = 20,
    ICON_LINK = 21,
    ICON_MENU = 22,
    ICON_DELETE = 23,
    ICON_TOOL_PROCEDURAL = 24,

    ICON_VISIBILITY = 25,
    ICON_VISIBILITY_OFF = 26,
    ICON_ARROW_DOWNWARD = 27,
    ICON_ARROW_UPWARD = 28,
    ICON_EDIT = 29,
    ICON_COPY = 30,
    ICON_GALLERY = 31,
    ICON_INFO = 32,

    ICON_SETTINGS = 33,
    ICON_CLOUD = 34,
    ICON_SHAPE = 35,
    ICON_CLOSE = 36,

    ICON_TOOLS = 41,
    ICON_PALETTE = 42,
    ICON_LAYERS = 43,
    ICON_RENDER = 44,
    ICON_CAMERA = 45,
    ICON_IMAGE = 46,
    ICON_EXPORT = 47,
    ICON_DEBUG = 48,

    ICON_VIEW = 49,
    ICON_MATERIAL = 50,
    ICON_LIGHT = 51,
};

/*
 * Some icons have their color blended depending on the style.  We define
 * them with a range in the icons atlas:
 */
#define ICON_COLORIZABLE_START 17
#define ICON_COLORIZABLE_END   41

// #### Block ##################
// The block size can only be 16.
#define BLOCK_SIZE 16
#define VOXEL_TEXTURE_SIZE 8

// Generate an optimal palette whith a fixed number of colors from a mesh.
void quantization_gen_palette(const mesh_t *mesh, int nb,
                              uint8_t (*palette)[4]);

// #### Goxel : core object ####

// Flags to set where the mouse snap.  In order of priority.
enum {
    SNAP_IMAGE_BOX      = 1 << 0,
    SNAP_SELECTION_IN   = 1 << 1,
    SNAP_SELECTION_OUT  = 1 << 2,
    SNAP_MESH           = 1 << 3,
    SNAP_PLANE          = 1 << 4,
    SNAP_CAMERA         = 1 << 5, // Used for laser tool.
    SNAP_LAYER_OUT      = 1 << 6, // Snap the layer box.

    SNAP_ROUNDED        = 1 << 8, // Round the result.
};

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

    // Tools can set this mesh and it will replace the current layer mesh
    // during render.
    mesh_t     *tool_mesh;

    mesh_t     *layers_mesh_;
    uint32_t   layers_mesh_hash;

    mesh_t     *render_mesh_; // All the layers + tool mesh.
    uint32_t   render_mesh_hash;

    layer_t    *render_layers;
    uint32_t   render_layers_hash;

    struct     {
        mesh_t *mesh;
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

    cursor_t   cursor;

    tool_t     *tool;
    float      tool_radius;
    bool       no_edit; // Disable editing.

    // Some state for the tool iter functions.
    float      tool_plane[4][4];
    int        tool_drag_mode; // 0: move, 1: resize.

    float      selection[4][4];   // The selection box.

    struct {
        float  rotation[4];
        float  pos[2];
        float  camera_ofs[3];
        float  camera_mat[4][4];
    } move_origin;

    palette_t  *palettes;   // The list of all the palettes
    palette_t  *palette;    // The current color palette
    char       *help_text;  // Seen in the bottom of the screen.
    char       *hint_text;  // Seen in the bottom of the screen.

    double     delta_time;  // Elapsed time since last frame (sec)
    int        frame_count; // Global frames counter.
    double     frame_time;  // Clock time at beginning of the frame (sec)
    double     fps;         // Average fps.
    bool       quit;        // Set to true to quit the application.

    int        view_effects; // EFFECT_WIREFRAME | EFFECT_GRID | EFFECT_EDGES

    // All the gestures we listen to.  Up to 16.
    gesture_t *gestures[16];
    int gestures_count;

    pathtracer_t pathtracer;

    // Used to check if the active mesh changed to play tick sound.
    uint64_t    last_mesh_key;
    double      last_click_time;

    // Some stats for the UI.
    struct {
        void (*current_panel)(void);
        float panel_width;
        float viewport[4];
    } gui;

} goxel_t;

// the global goxel instance.
extern goxel_t goxel;

// XXX: add some doc.
void goxel_init(void);
void goxel_release(void);
void goxel_reset(void);
int goxel_iter(inputs_t *inputs);
void goxel_render(void);

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
                    const float pos[2], int snap_mask, float offset,
                    float out[3], float normal[3]);

void goxel_render_view(const float viewport[4], bool render_mode);
void goxel_render_export_view(const float viewport[4]);
// Called by the gui when the mouse hover a 3D view.
// XXX: change the name since we also call it when the mouse get out of
// the view.
void goxel_mouse_in_view(const float viewport[4], const inputs_t *inputs,
                         bool capture_keys);

const mesh_t *goxel_get_layers_mesh(const image_t *img);
const mesh_t *goxel_get_render_mesh(const image_t *img);

/*
 * Function: goxel_get_render_layers
 * Compute merged current image layer list
 *
 * This returns a simplified list of layers from the current image where
 * we merged as many layers as possible into a single one.
 *
 * It also can replace the current layer mesh with the tool preview.
 *
 * This is the function that should be used the get the actual list of layers
 * to be rendered.
 */
const layer_t *goxel_get_render_layers(bool with_tool_preview);

void goxel_set_help_text(const char *msg, ...);
void goxel_set_hint_text(const char *msg, ...);

void goxel_import_image_plane(const char *path);

int goxel_import_file(const char *path, const char *format);
int goxel_export_to_file(const char *path, const char *format);

// Render the view into an RGB[A] buffer.
void goxel_render_to_buf(uint8_t *buf, int w, int h, int bpp);

void save_to_file(const image_t *img, const char *path);
int load_from_file(const char *path);

// Iter info of a gox file, without actually reading it.
// For the moment only returns the image preview if available.
int gox_iter_infos(const char *path,
                   int (*callback)(const char *attr, int size,
                                   void *value, void *user),
                   void *user);

// Section: box_edit
/*
 * Function: gox_edit
 * Render a box that can be edited with the mouse.
 *
 * This is used for the move and selection tools.
 * Still a bit experimental.  In theory we should be able to edit any box,
 * but because of the snap mechanism, we can only edit the layer or selection
 * for the moment.
 *
 * Parameters:
 *   snap   - SNAP_LAYER_OUT for layer edit, SNAP_SELECTION_OUT for selection
 *            edit.
 *   mode   - 0: move, 1: resize.
 *   transf - Receive the output transformation.
 *   first  - Set to true if the edit is the first one.
 */
int box_edit(int snap, int mode, float transf[4][4], bool *first);


void settings_load(void);
void settings_save(void);

// Section: tests

/* Function: tests_run
 * Run all the unit tests */
void tests_run(void);


#endif // GOXEL_H
