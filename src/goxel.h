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
#include "camera.h"
#include "utlist.h"
#include "uthash.h"
#include "utarray.h"
#include "noc_file_dialog.h"
#include "block_def.h"
#include "gesture.h"
#include "gesture3d.h"
#include "gui.h"
#include "inputs.h"
#include "log.h"
#include "luagoxel.h"
#include "mesh.h"
#include "mesh_utils.h"
#include "model3d.h"
#include "palette.h"
#include "texture.h"
#include "theme.h"
#include "pathtracer.h"
#include "render.h"
#include "shape.h"
#include "system.h"

#include "utils/cache.h"
#include "utils/crc64.h"
#include "utils/gl.h"
#include "utils/vec.h"
#include "utils/sound.h"

#include <float.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define GOXEL_VERSION_STR "0.8.3"
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
 * Function: read_file
 * Read a file from disk.
 *
 * Return:
 *   A newly allocated buffer containing the file data.
 */
char *read_file(const char *path, int *size);

/*
 * Function: img_read
 * Read an image from a file.
 */
uint8_t *img_read(const char *path, int *width, int *height, int *bpp);

/*
 * Function: img_read_from_mem
 * Read an image from memory.
 */
uint8_t *img_read_from_mem(const char *data, int size,
                           int *w, int *h, int *bpp);

/*
 * Function: img_write
 * Write an image to a file.
 */
void img_write(const uint8_t *img, int w, int h, int bpp, const char *path);

/*
 * Function: img_write_to_mem
 * Write an image to memory.
 */
uint8_t *img_write_to_mem(const uint8_t *img, int w, int h, int bpp,
                          int *size);

/*
 * Function: img_downsample
 * Downsample an image by half, using interpolation.
 */
void img_downsample(const uint8_t *img, int w, int h, int bpp,
                    uint8_t *out);
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
 * Function: str_equ
 * Return whether two strings are equal.
 */
static inline bool str_equ(const char *s1, const char *s2) {
    return strcmp(s1, s2) == 0;
}

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

// #############################

// XXX: I should clean up a but the code of vec.h so that I can put those on
// top.
#include "box.h"
#include "plane.h"


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

    ICON_MODE_ADD = 9,
    ICON_MODE_SUB = 10,
    ICON_MODE_PAINT = 11,
    ICON_SHAPE_CUBE = 12,
    ICON_SHAPE_SPHERE = 13,
    ICON_SHAPE_CYLINDER = 14,
    ICON_TOOL_SELECTION = 15,

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
};

/*
 * Some icons have their color blended depending on the style.  We define
 * them with a range in the icons atlas:
 */
#define ICON_COLORIZABLE_START 17
#define ICON_COLORIZABLE_END   41

// #### Tool/Operation/Painter #

enum {
    TOOL_NONE = 0,
    TOOL_BRUSH,
    TOOL_SHAPE,
    TOOL_LASER,
    TOOL_SET_PLANE,
    TOOL_MOVE,
    TOOL_PICK_COLOR,
    TOOL_SELECTION,
    TOOL_PROCEDURAL,
    TOOL_EXTRUDE,

    TOOL_COUNT
};

// Mesh mask for goxel_update_meshes function.
enum {
    MESH_LAYERS = 1 << 0,
    MESH_PICK   = 1 << 1,
    MESH_RENDER = 1 << 2,
};


// #### Block ##################
// The block size can only be 16.
#define BLOCK_SIZE 16
#define VOXEL_TEXTURE_SIZE 8


// Generate an optimal palette whith a fixed number of colors from a mesh.
void quantization_gen_palette(const mesh_t *mesh, int nb,
                              uint8_t (*palette)[4]);


// #### Goxel : core object ####

// Key id, same as GLFW for convenience.
enum {
    KEY_ESCAPE      = 256,
    KEY_ENTER       = 257,
    KEY_TAB         = 258,
    KEY_BACKSPACE   = 259,
    KEY_DELETE      = 261,
    KEY_RIGHT       = 262,
    KEY_LEFT        = 263,
    KEY_DOWN        = 264,
    KEY_UP          = 265,
    KEY_PAGE_UP     = 266,
    KEY_PAGE_DOWN   = 267,
    KEY_HOME        = 268,
    KEY_END         = 269,
    KEY_LEFT_SHIFT  = 340,
    KEY_RIGHT_SHIFT = 344,
    KEY_CONTROL     = 341,
};

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


typedef struct history history_t;

typedef struct layer layer_t;
struct layer {
    layer_t     *next, *prev;
    mesh_t      *mesh;
    int         id;         // Uniq id in the image (for clones).
    bool        visible;
    char        name[256];  // 256 chars max.
    float       box[4][4];  // Bounding box.
    float       mat[4][4];
    // For 2d image layers.
    texture_t   *image;
    // For clone layers:
    int         base_id;
    uint64_t    base_mesh_key;
    // For shape layers.
    const shape_t *shape;
    uint64_t    shape_key;
    uint8_t     color[4];
};

typedef struct image image_t;
struct image {
    layer_t *layers;
    layer_t *active_layer;
    camera_t *cameras;
    camera_t *active_camera;
    float    box[4][4];

    // For saving.
    char     *path;
    int      export_width;
    int      export_height;
    uint64_t saved_key;     // image_get_key() value of saved file.

    image_t *history;
    image_t *history_next, *history_prev;
};

image_t *image_new(void);
void image_delete(image_t *img);
layer_t *image_add_layer(image_t *img);
void image_delete_layer(image_t *img, layer_t *layer);
void image_move_layer(image_t *img, layer_t *layer, int d);
layer_t *image_duplicate_layer(image_t *img, layer_t *layer);
void image_merge_visible_layers(image_t *img);
void image_history_push(image_t *img);
void image_undo(image_t *img);
void image_redo(image_t *img);
bool image_layer_can_edit(const image_t *img, const layer_t *layer);

/*
 * Function: image_get_key
 * Return a value that is guarantied to change when the image change.
 */
uint64_t image_get_key(const image_t *img);

enum {
    // Tools flags.
    TOOL_REQUIRE_CAN_EDIT = 1 << 0, // Set to tools that can edit the layer.
    TOOL_REQUIRE_CAN_MOVE = 1 << 1, // Set to tools that can move the layer.
    TOOL_ALLOW_PICK_COLOR = 1 << 2, // Ctrl switches to pick color tool.
};

// Tools
typedef struct tool tool_t;
struct tool {
    int id;
    const char *action_id;
    int (*iter_fn)(tool_t *tool, const painter_t *painter,
                   const float viewport[4]);
    int (*gui_fn)(tool_t *tool);
    const char *default_shortcut;
    int state; // XXX: to be removed I guess.
    int flags;
};

#define TOOL_REGISTER(id_, name_, klass_, ...) \
    static klass_ GOX_tool_##id_ = {\
            .tool = { \
                .id = id_, .action_id = "tool_set_" #name_, __VA_ARGS__ \
            } \
        }; \
    static void GOX_register_tool_##tool_(void) __attribute__((constructor)); \
    static void GOX_register_tool_##tool_(void) { \
        tool_register_(&GOX_tool_##id_.tool); \
    }

void tool_register_(const tool_t *tool);
int tool_iter(tool_t *tool, const painter_t *painter, const float viewport[4]);
int tool_gui(tool_t *tool);

int tool_gui_snap(void);
int tool_gui_shape(const shape_t **shape);
int tool_gui_radius(void);
int tool_gui_smoothness(void);
int tool_gui_color(void);
int tool_gui_symmetry(void);
int tool_gui_drag_mode(int *mode);

typedef struct goxel
{
    int        screen_size[2];
    float      screen_scale;
    image_t    *image;

    mesh_t     *layers_mesh; // All the layers combined.
    // Tools can set this mesh and it will replace the current layer mesh
    // during render.
    mesh_t     *tool_mesh;
    mesh_t     *render_mesh; // All the layers + tool mesh.

    struct     {
        mesh_t *mesh;
        float  box[4][4];
    } clipboard;

    history_t  *history;     // Undo/redo history.
    int        snap_mask;    // Global snap mask (can edit in the GUI).
    float      snap_offset;  // Only for brush tool, remove that?

    float      plane[4][4];         // The snapping plane.
    bool       show_export_viewport;

    camera_t   camera;

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
    bool       tool_shape_two_steps; // Param of the shape tool.
    int        tool_drag_mode; // 0: move, 1: resize.

    float      selection[4][4];   // The selection box.

    struct {
        float  rotation[4];
        float  pos[2];
        float  camera_ofs[3];
    } move_origin;

    palette_t  *palettes;   // The list of all the palettes
    palette_t  *palette;    // The current color palette
    char       *help_text;  // Seen in the bottom of the screen.
    char       *hint_text;  // Seen in the bottom of the screen.

    int        frame_count; // Global frames counter.
    double     frame_time;  // Clock time at beginning of the frame (sec)
    double     fps;         // Average fps.
    bool       quit;        // Set to true to quit the application.
    bool       show_wireframe; // Show debug wireframe on meshes.

    struct {
        gesture_t drag;
        gesture_t pan;
        gesture_t rotate;
        gesture_t hover;
        gesture_t pinch;
    } gestures;

    pathtracer_t pathtracer;

    // Used to check if the active mesh changed to play tick sound.
    uint64_t    last_mesh_key;
    double      last_click_time;
} goxel_t;

// the global goxel instance.
extern goxel_t goxel;

// XXX: add some doc.
void goxel_init(void);
void goxel_release(void);
void goxel_reset(void);
int goxel_iter(inputs_t *inputs);
void goxel_render(void);
void goxel_render_view(const float viewport[4], bool render_mode);
void goxel_render_export_view(const float viewport[4]);
// Called by the gui when the mouse hover a 3D view.
// XXX: change the name since we also call it when the mouse get out of
// the view.
void goxel_mouse_in_view(const float viewport[4], const inputs_t *inputs,
                         bool capture_keys);

// Recompute the meshes.  mask from MESH_ enum.
void goxel_update_meshes(int mask);

void goxel_set_help_text(const char *msg, ...);
void goxel_set_hint_text(const char *msg, ...);

void goxel_import_image_plane(const char *path);

// Render the view into an RGB[A] buffer.
void goxel_render_to_buf(uint8_t *buf, int w, int h, int bpp);


// #############################

void save_to_file(const image_t *img, const char *path, bool with_preview);
int load_from_file(const char *path);

// Iter info of a gox file, without actually reading it.
// For the moment only returns the image preview if available.
int gox_iter_infos(const char *path,
                   int (*callback)(const char *attr, int size,
                                   void *value, void *user),
                   void *user);


// #### Colors functions #######
void hsl_to_rgb(const uint8_t hsl[3], uint8_t rgb[3]);
void rgb_to_hsl(const uint8_t rgb[3], uint8_t hsl[3]);

// #############################

void wavefront_export(const mesh_t *mesh, const char *path);
void ply_export(const mesh_t *mesh, const char *path);

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

// Section: tests

/* Function: tests_run
 * Run all the unit tests */
void tests_run(void);

// Section: script

/*
 * Function: script_run
 * Run a lua script from a file.
 */
int script_run(const char *filename, int argc, const char **argv);



#endif // GOXEL_H
