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
#include "vec.h"
#include "utlist.h"
#include "uthash.h"
#include "utarray.h"
#include "noc_file_dialog.h"
#include "block_def.h"
#include "luagoxel.h"
#include "mesh.h"
#include "texture.h"
#include "theme.h"
#include "pathtracer.h"
#include "system.h"

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


// #### Logging macros #########

enum {
    GOX_LOG_VERBOSE = 2,
    GOX_LOG_DEBUG   = 3,
    GOX_LOG_INFO    = 4,
    GOX_LOG_WARN    = 5,
    GOX_LOG_ERROR   = 6,
};

#ifndef LOG_LEVEL
#   if DEBUG
#       define LOG_LEVEL GOX_LOG_DEBUG
#   else
#       define LOG_LEVEL GOX_LOG_INFO
#   endif
#endif

#define LOG(level, msg, ...) do { \
    if (level >= LOG_LEVEL) \
        dolog(level, msg, __func__, __FILE__, __LINE__, ##__VA_ARGS__); \
} while(0)

#define LOG_V(msg, ...) LOG(GOX_LOG_VERBOSE, msg, ##__VA_ARGS__)
#define LOG_D(msg, ...) LOG(GOX_LOG_DEBUG,   msg, ##__VA_ARGS__)
#define LOG_I(msg, ...) LOG(GOX_LOG_INFO,    msg, ##__VA_ARGS__)
#define LOG_W(msg, ...) LOG(GOX_LOG_WARN,    msg, ##__VA_ARGS__)
#define LOG_E(msg, ...) LOG(GOX_LOG_ERROR,   msg, ##__VA_ARGS__)

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

// #### Include OpenGL #########
#define GL_GLEXT_PROTOTYPES
#ifdef WIN32
#    include <windows.h>
#    include "GL/glew.h"
#endif
#ifdef __APPLE__
#   include "TargetConditionals.h"
#   if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#       define GLES2 1
#       include <OPenGLES/ES2/gl.h>
#       include <OpenGLES/ES2/glext.h>
#   else
#       include <OpenGL/gl.h>
#   endif
#else
#   ifdef GLES2
#       include <GLES2/gl2.h>
#       include <GLES2/gl2ext.h>
#   else
#       include <GL/gl.h>
#   endif
#endif
// #############################



// #### GL macro ###############
#if DEBUG
#  define GL(line) ({                                                   \
       line;                                                            \
       if (gl_check_errors(__FILE__, __LINE__)) assert(false);          \
   })
#else
#  define GL(line) line
#endif
// #############################




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

/*
 * Macro: vec3_set
 * Set a 3 sized array values.
 */
#define vec3_set(v, x, y, z) do { \
    (v)[0] = (x); (v)[1] = (y); (v)[2] = (z); } while(0)

/*
 * Macro: vec4_set
 * Set a 4 sized array values.
 */
#define vec4_set(v, x, y, z, w) do { \
    (v)[0] = (x); (v)[1] = (y); (v)[2] = (z); (v)[3] = (w); } while(0)

/*
 * Macro: vec4_copy
 * Copy a 4 sized array into an other one.
 */
#define vec4_copy(a, b) do { \
        (b)[0] = (a)[0]; (b)[1] = (a)[1]; (b)[2] = (a)[2]; (b)[3] = (a)[3]; \
    } while (0)

/*
 * Macro: vec4_equal
 * Test whether two 4 sized vectors are equal.
 */
#define vec4_equal(a, b) ({ \
        (a)[0] == (b)[0] && \
        (a)[1] == (b)[1] && \
        (a)[2] == (b)[2] && \
        (a)[3] == (b)[3]; })

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

// Used internally by the LOG macro
void dolog(int level, const char *msg,
           const char *func, const char *file, int line, ...);

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

/*
 * Function: b64_decode
 * Decode a base64 string
 *
 * Parameters:
 *   src  - A base 64 encoded string.
 *   dest - Buffer that will receive the decoded value or NULL.  If set to
 *          NULL the function just returns the size of the decoded data.
 *
 * Return:
 *   The size of the decoded data.
 */
int b64_decode(const char *src, void *dest);

/*
 * Function: crc64
 * Compute crc64 hash
 *
 * Parameters:
 *   crc    - Initial crc value.
 *   s      - Data pointer.
 *   len    - Data size.
 *
 * Return:
 *   The new crc64 value.
 */
uint64_t crc64(uint64_t crc, const void *s, uint64_t len);

// #############################

// XXX: I should clean up a but the code of vec.h so that I can put those on
// top.
#include "box.h"
#include "plane.h"

// ######### Section: GL utils ############################

int gl_check_errors(const char *file, int line);

/*
 * Function: gl_has_extension
 * Check whether an OpenGL extension is available.
 */
bool gl_has_extension(const char *extension);

/*
 * Function: gl_create_prog
 * Helper function to create an OpenGL program.
 */
int gl_create_prog(const char *vertex_shader, const char *fragment_shader,
                   const char *include);

/*
 * Function: gl_create_prog
 * Helper function to delete an OpenGL program.
 */
void gl_delete_prog(int prog);

/*
 * Function: gl_gen_fbo
 * Helper function to generate an OpenGL framebuffer object with an
 * associated texture.
 *
 * Parameters:
 *   w          - Width of the fbo.
 *   h          - Height of the fbo.
 *   format     - GL_RGBA or GL_DEPTH_COMPONENT.
 *   out_fbo    - The created fbo.
 *   out_tex    - The created texture.
 */
int gl_gen_fbo(int w, int h, GLenum format, int msaa,
               GLuint *out_fbo, GLuint *out_tex);


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

typedef struct shape {
    const char *id;
    float (*func)(const float p[3], const float s[3], float smoothness);
} shape_t;

void shapes_init(void);
extern shape_t shape_sphere;
extern shape_t shape_cube;
extern shape_t shape_cylinder;


// #### Block ##################
// The block size can only be 16.
#define BLOCK_SIZE 16
#define VOXEL_TEXTURE_SIZE 8

// Structure used for the OpenGL array data of blocks.
// XXX: we can probably make it smaller.
typedef struct voxel_vertex
{
    uint8_t  pos[3]                     __attribute__((aligned(4)));
    int8_t   normal[3]                  __attribute__((aligned(4)));
    uint8_t  color[4]                   __attribute__((aligned(4)));
    uint16_t pos_data                   __attribute__((aligned(4)));
    uint8_t  uv[2]                      __attribute__((aligned(4)));
    uint8_t  bshadow_uv[2]              __attribute__((aligned(4)));
    uint8_t  bump_uv[2]                 __attribute__((aligned(4)));
} voxel_vertex_t;


// ######## Section: Mesh util ############################################

/*
 * Enum: MODE
 * Define how layers/brush are merged.  Each mode defines how to apply a
 * source voxel into a destination voxel.
 *
 * MODE_OVER        - New values replace old one.
 * MODE_SUB         - Substract source alpha from destination
 * MODE_SUB_CLAMP   - Set alpha to the minimum between the destination value
 *                    and one minus the source value.
 * MODE_PAINT       - Set the color of the destination using the source.
 * MODE_MAX         - Set alpha to the max of the source and destination.
 * MODE_INTERSECT   - Set alpha to the min of the source and destination.
 * MODE_MULT_ALPHA  - Multiply the source and dest using source alpha.
 */
enum {
    MODE_NULL,
    MODE_OVER,
    MODE_SUB,
    MODE_SUB_CLAMP,
    MODE_PAINT,
    MODE_MAX,
    MODE_INTERSECT,
    MODE_MULT_ALPHA,
};

// Type: painter_t
// The painting context, including the tool, brush, mode, radius,
// color, etc...
//
// Attributes:
//   mode - Define how colors are applied.  One of the <MODE> enum value.
typedef struct painter {
    int             mode;
    const shape_t   *shape;
    uint8_t         color[4];
    float           smoothness;
    int             symmetry; // bitfield X Y Z
    float           symmetry_origin[3];
    float           (*box)[4][4];     // Clipping box (can be null)
} painter_t;


/* Function: mesh_get_box
 * Compute the bounding box of a mesh.  */
// XXX: remove this function!
void mesh_get_box(const mesh_t *mesh, bool exact, float box[4][4]);

/* Function: mesh_op
 * Apply a paint operation to a mesh.
 * This function render geometrical 3d shapes into a mesh.
 * The shape, mode and color are defined in the painter argument.
 *
 * Parameters:
 *   mesh    - The mesh we paint into.
 *   painter - Defines the paint operation to apply.
 *   box     - Defines the position and size of the shape as the
 *             transformation matrix from the zero centered unit box.
 *
 * See Also:
 *   <painter_t>
 */
void mesh_op(mesh_t *mesh, const painter_t *painter, const float box[4][4]);

// XXX: to cleanup.
void mesh_extrude(mesh_t *mesh,
                  const float plane[4][4],
                  const float box[4][4]);

/* Function: mesh_blit
 *
 * Blit voxel data into a mesh.
 * This is the fastest way to quickly put data into a mesh.
 *
 * Parameters:
 *   mesh - The mesh we blit into.
 *   data - Pointer to voxel data (RGBA values, in xyz order).
 *   x    - X pos.
 *   y    - Y pos.
 *   z    - Z pos.
 *   w    - Width of the data.
 *   h    - Height of the data.
 *   d    - Depth of the data.
 *   iter - Optional iterator for optimized access.
 */
void mesh_blit(mesh_t *mesh, const uint8_t *data,
               int x, int y, int z, int w, int h, int d,
               mesh_iterator_t *iter);

void mesh_move(mesh_t *mesh, const float mat[4][4]);

void mesh_shift_alpha(mesh_t *mesh, int v);

// Compute the selection mask for a given condition.
int mesh_select(const mesh_t *mesh,
                const int start_pos[3],
                int (*cond)(const uint8_t value[4],
                            const uint8_t neighboors[6][4],
                            const uint8_t mask[6],
                            void *user),
                void *user, mesh_t *selection);

/*
 * Function: mesh_merge
 * Merge a mesh into an other using a given blending function.
 *
 * Parameters:
 *   mesh   - The destination mesh we merge into.
 *   other  - The source mesh we merge.  Unchanged by this function.
 *   mode   - The blending function used.  One of the <MODE> enum values.
 *   color  - A color to apply to the source mesh before merging.  Can be
 *            set to NULL.
 */
void mesh_merge(mesh_t *mesh, const mesh_t *other, int mode,
                const uint8_t color[4]);

/*
 * Function: mesh_generate_vertices
 * Generate a vertice array for rendering a mesh block.
 *
 * Parameters:
 *   mesh       - Input mesh.
 *   block_pos  - Position of the mesh block to render.
 *   effects    - Effect flags.
 *   out        - Output array.
 *   size       - Output the size of a single face.
 *                4 for quads and 3 for triangles.  Normal mesh uses quad
 *                but marching cube effect return triangle arrays.
 *   subdivide  - Ouput the number of subdivisions used for a voxel.  Normal
 *                render uses 1 unit per voxel, but marching cube rendering
 *                can use more.
 */
int mesh_generate_vertices(const mesh_t *mesh, const int block_pos[3],
                           int effects, voxel_vertex_t *out,
                           int *size, int *subdivide);

// XXX: use int[2][3] for the box?
void mesh_crop(mesh_t *mesh, const float box[4][4]);

/* Function: mesh_crc64
 * Compute the crc64 of the mesh data as an array of xyz rgba values.
 *
 * This is only used in the tests, to make sure that we can still open
 * old file formats.
 */
uint64_t mesh_crc64(const mesh_t *mesh);

// #### Renderer ###############

enum {
    EFFECT_RENDER_POS       = 1 << 1,
    EFFECT_SMOOTH           = 1 << 2,
    EFFECT_BORDERS          = 1 << 3,
    EFFECT_BORDERS_ALL      = 1 << 4,
    EFFECT_SEMI_TRANSPARENT = 1 << 5,
    EFFECT_SEE_BACK         = 1 << 6,
    EFFECT_MARCHING_CUBES   = 1 << 7,
    EFFECT_SHADOW_MAP       = 1 << 8,
    EFFECT_FLAT             = 1 << 9,

    // For render box.
    EFFECT_NO_SHADING       = 1 << 10,
    EFFECT_STRIP            = 1 << 11,
    EFFECT_WIREFRAME        = 1 << 12,
    EFFECT_GRID             = 1 << 13,

    EFFECT_PROJ_SCREEN      = 1 << 14, // Image project in screen.
    EFFECT_ANTIALIASING     = 1 << 15,
};

typedef struct {
    float ambient;
    float diffuse;
    float specular;
    float shininess;
    float smoothness;
    float shadow;
    int   effects;
    float border_shadow;
} render_settings_t;

typedef struct renderer renderer_t;
typedef struct render_item_t render_item_t;
struct renderer
{
    float view_mat[4][4];
    float proj_mat[4][4];
    int    fbo;     // The renderer target framebuffer.
    float  scale;   // For retina display.

    struct {
        float  pitch;
        float  yaw;
        bool   fixed;       // If set, then the light moves with the view.
        float  intensity;
    } light;

    render_settings_t settings;

    render_item_t    *items;
};

void render_init(void);
void render_deinit(void);
void render_mesh(renderer_t *rend, const mesh_t *mesh, int effects);
void render_grid(renderer_t *rend, const float plane[4][4],
                 const uint8_t color[4], const float clip_box[4][4]);
void render_line(renderer_t *rend, const float a[3], const float b[3],
                 const uint8_t color[4]);
void render_box(renderer_t *rend, const float box[4][4],
                const uint8_t color[4], int effects);
void render_sphere(renderer_t *rend, const float mat[4][4]);
void render_img(renderer_t *rend, texture_t *tex, const float mat[4][4],
                int efffects);

/*
 * Function: render_img2
 * Render an image directly from it's pixel data.
 *
 * XXX: this is experimental: eventually I think we should remove render_img
 * and only user render_img2 (renamed to render_img).
 */
void render_img2(renderer_t *rend,
                 const uint8_t *data, int w, int h, int bpp,
                 const float mat[4][4], int effects);

void render_rect(renderer_t *rend, const float plane[4][4], int effects);
// Flushes all the queued render items.  Actually calls opengl.
//  rect: the viewport rect (passed to glViewport).
//  clear_color: clear the screen with this first.
void render_submit(renderer_t *rend, const int rect[4],
                   const uint8_t clear_color[4]);
int render_get_default_settings(int i, char **name, render_settings_t *out);
// Compute the light direction in the model coordinates (toward the light)
void render_get_light_dir(const renderer_t *rend, float out[3]);

// Ugly function that return the position of the block at a given id
// when the mesh is rendered with render_mesh.
void render_get_block_pos(renderer_t *rend, const mesh_t *mesh,
                          int id, int pos[3]);


/* ##############################
 * Section: Model3d
 *
 * Functions to render 3d vertex models, like cube, plane, etc.
 */

/*
 * Type: model_vertex_t
 * Container for a single vertex shader data.
 */
typedef struct {
     float    pos[3]    __attribute__((aligned(4)));
     float    normal[3] __attribute__((aligned(4)));
     uint8_t  color[4]  __attribute__((aligned(4)));
     float    uv[2]     __attribute__((aligned(4)));
} model_vertex_t;

/*
 * Type: model3d_t
 * Define a 3d model.
 */
typedef struct {
    int              nb_vertices;
    model_vertex_t   *vertices;
    bool             solid;
    bool             cull;

    // Rendering buffers.
    // XXX: move this into the renderer, like for block_t
    GLuint  vertex_buffer;
    int     nb_lines;
    bool    dirty;
} model3d_t;

/*
 * Function: model3d_init
 * Should be called once before any mode3d functions.
 */
void model3d_init(void);

/*
 * Function: model3d_cube
 * Create a 3d cube from (0, 0, 0) to (1, 1, 1)
 */
model3d_t *model3d_cube(void);

/*
 * Function: model3d_wire_cube
 * Create a 3d wire cube from (0, 0, 0) to (1, 1, 1)
 */
model3d_t *model3d_wire_cube(void);

/*
 * Function: model3d_sphere
 * Create a sphere of radius 1, centered at (0, 0).
 */
model3d_t *model3d_sphere(int slices, int stacks);

/*
 * Function: model3d_grid
 * Create a grid plane.
 */
model3d_t *model3d_grid(int nx, int ny);

/*
 * Function: model3d_line
 * Create a line from (-0.5, 0, 0) to (+0.5, 0, 0).
 */
model3d_t *model3d_line(void);

/*
 * Function: model3d_line
 * Create a 2d rect on xy plane, of size 1x1 centered on the origin.
 */
model3d_t *model3d_rect(void);

/*
 * Function: model3d_line
 * Create a 2d rect on xy plane, of size 1x1 centered on the origin.
 */
model3d_t *model3d_wire_rect(void);

/*
 * Function: model3d_render
 * Render a 3d model using OpenGL calls.
 */
void model3d_render(model3d_t *model3d,
                    const float model[4][4],
                    const float view[4][4],
                    const float proj[4][4],
                    const uint8_t color[4],
                    const texture_t *tex,
                    const float light[3],
                    const float clip_box[4][4],
                    int   effects);

// #### Palette ################

typedef struct {
    uint8_t  color[4];
    char     name[32];
} palette_entry_t;

typedef struct palette palette_t;
struct palette {
    palette_t *next, *prev; // For the global list of palettes.
    char    name[128];
    int     columns;
    int     size;
    palette_entry_t *entries;
};

// Load all the available palettes into a list.
void palette_load_all(palette_t **list);

// Generate an optimal palette whith a fixed number of colors from a mesh.
void quantization_gen_palette(const mesh_t *mesh, int nb,
                              uint8_t (*palette)[4]);

/*
 * Function: palette_search
 * Search a given color in a palette
 *
 * Parameters:
 *   palette    - A palette.
 *   col        - The color we are looking for.
 *   exact      - If set to true, return -1 if no color is found, else
 *                return the closest color.
 *
 * Return:
 *   The index of the color in the palette.
 */
int palette_search(const palette_t *palette, const uint8_t col[4],
                   bool exact);

// #############################


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

// A finger touch or mouse click state.
// `down` represent each button in the mouse.  For touch events only the
// first element is set.
typedef struct {
    float   pos[2];
    bool    down[3];
} touch_t;

typedef struct inputs
{
    int         window_size[2];
    float       scale;
    bool        keys[512]; // Table of all the pressed keys.
    uint32_t    chars[16];
    touch_t     touches[4];
    float       mouse_wheel;
    int         framebuffer; // Screen framebuffer
} inputs_t;

// Conveniance function to add a char in the inputs.
void inputs_insert_char(inputs_t *inputs, uint32_t c);


/* #########################################
 * Section: Mouse gestures
 * Detect 2d mouse or touch gestures.
 *
 * The way it works is that we have to create one instance of
 * <gesture_t> per gesture we can recognise, and then use the
 * <gesture_update> function to update their states and call the callback
 * functions of active gestures.
 */

/* XXX: This part of the code is not very clear */

/*
 * Enum: GESTURE_TYPES
 * Define the different types of recognised gestures.
 *
 * GESTURE_DRAG     - Click and drag the mouse.
 * GESTURE_CLICK    - Single click.
 * GESTURE_PINCH    - Two fingers pinch.
 * GESTURE_HOVER    - Move the mouse without clicking.
 */
enum {
    GESTURE_DRAG        = 1 << 0,
    GESTURE_CLICK       = 1 << 1,
    GESTURE_PINCH       = 1 << 2,
    GESTURE_HOVER       = 1 << 3,
};

/*
 * Enum: GESTURE_STATES
 * Define the states a gesture can be in.
 *
 * GESTURE_POSSIBLE     - The gesture is not recognised yet (default state).
 * GESTURE_RECOGNISED   - The gesture has been recognised.
 * GESTURE_BEGIN        - The gesture has begun.
 * GESTURE_UPDATE       - The gesture is in progress.
 * GESTURE_END          - The testure has ended.
 * GESTURE_TRIGGERED    - For click gestures: the gesture has occured.
 * GESTURE_FAILED       - The gesture has failed.
 */
enum {
    GESTURE_POSSIBLE = 0,
    GESTURE_RECOGNISED,
    GESTURE_BEGIN,
    GESTURE_UPDATE,
    GESTURE_END,
    GESTURE_TRIGGERED,
    GESTURE_FAILED,
};

/*
 * Type: gesture_t
 * Structure used to handle a given gesture.
 */
typedef struct gesture gesture_t;
struct gesture
{
    int     type;
    int     button;
    int     state;
    float   viewport[4];
    float   pos[2];
    float   start_pos[2][2];
    float   pinch;
    float   rotation;
    int     (*callback)(const gesture_t *gest, void *user);
};

/*
 * Function: gesture_update
 * Update the state of a list of gestures, and call the gestures
 * callbacks as needed.
 *
 * Parameters:
 *   nb         - Number of gestures.
 *   gestures   - A pointer to an array of <gesture_t> instances.
 *   inputs     - The inputs structure.
 *   viewport   - Current viewport rect.
 *   user       - User data pointer, passed to the callbacks.
 */
int gesture_update(int nb, gesture_t *gestures[],
                   const inputs_t *inputs, const float viewport[4],
                   void *user);


/* #############################
 * Section: Camera
 * Camera manipulation functions.
 */

/* Type: camera_t
 * Camera structure.
 *
 * The actual position of the camera is constructed from a distance, a
 * rotation and an offset:
 *
 * Pos = ofs * rot * dist
 *
 * Attributes:
 *   next, prev - Used for linked list of cameras in an image.
 *   name       - Name to show in the GUI.
 *   ortho      - Set to true for orthographic projection.
 *   dist       - Distance used to compute the position.
 *   rot        - Camera rotation quaternion.
 *   ofs        - Lateral offset of the camera position.
 *   fovy       - Field of view in y direction.
 *   aspect     - Aspect ratio.
 *   view_mat   - Modelview transformation matrix (auto computed).
 *   proj_mat   - Projection matrix (auto computed).
 */
typedef struct camera camera_t;
struct camera
{
    camera_t  *next, *prev; // List of camera in an image.
    char   name[128];  // 127 chars max.
    bool   ortho; // Set to true for orthographic projection.
    float  dist;
    float  rot[4]; // Quaternion.
    float  ofs[3];
    float  fovy;
    float  aspect;

    // Auto computed from other values:
    float view_mat[4][4];    // Model to view transformation.
    float proj_mat[4][4];    // Proj transform from camera coordinates.
};

/*
 * Function: camera_new
 * Create a new camera.
 *
 * Parameters:
 *   name - The name of the camera.
 *
 * Return:
 *   A newly instanciated camera.
 */
camera_t *camera_new(const char *name);

/*
 * Function: camera_delete
 * Delete a camera
 */
void camera_delete(camera_t *camera);

/*
 * Function: camera_set
 * Set a camera position from an other camera.
 */
void camera_set(camera_t *camera, const camera_t *other);

/*
 * Function: camera_update
 * Update camera matrices.
 */
void camera_update(camera_t *camera);

/*
 * Function: camera_set_target
 * Adjust the camera settings so that the rotation works for a given
 * position.
 */
void camera_set_target(camera_t *camera, const float pos[3]);

/*
 * Function: camera_get_ray
 * Get the raytracing ray of the camera at a given screen position.
 *
 * Parameters:
 *   win   - Pixel position in screen coordinates.
 *   view  - Viewport rect: [min_x, min_y, max_x, max_y].
 *   o     - Output ray origin.
 *   d     - Output ray direction.
 */
void camera_get_ray(const camera_t *camera, const float win[2],
                    const float viewport[4], float o[3], float d[3]);

/*
 * Function: camera_fit_box
 * Move a camera so that a given box is entirely visible.
 */
void camera_fit_box(camera_t *camera, const float box[4][4]);

/*
 * Function: camera_get_key
 * Return a value that is guarantied to change when the camera change.
 */
uint64_t camera_get_key(const camera_t *camera);

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

// ##### Procedural rendering ########################

// The possible states of the procedural program.
enum {
    PROC_INIT,
    PROC_PARSE_ERROR,
    PROC_READY,
    PROC_RUNNING,
    PROC_DONE,
};

typedef struct proc {
    struct proc_node *prog; // AST of the program.
    struct proc_ctx  *ctxs; // Rendering stack during execution.
    int              state;
    int              frame; // Rendering frame.
    bool             in_frame; // Set if the current frame is not finished.
    struct {
        char         *str;  // Set in case of parsing or execution error.
        int          line;
    } error;
} gox_proc_t;

int proc_parse(const char *txt, gox_proc_t *proc);
void proc_release(gox_proc_t *proc);
int proc_start(gox_proc_t *proc, const float box[4][4]);
int proc_stop(gox_proc_t *proc);
int proc_iter(gox_proc_t *proc, mesh_t *mesh, const painter_t *painter);

// Represent a 3d cursor.
// The program keeps track of two cursors, that are then used by the tools.

enum {
    // The state flags of the cursor.
    CURSOR_PRESSED      = 1 << 0,
    CURSOR_SHIFT        = 1 << 1,
    CURSOR_CTRL         = 1 << 2,

    CURSOR_OUT          = 1 << 3, // Outside of sensing area.
};

typedef struct gesture3d gesture3d_t;

typedef struct cursor {
    float  pos[3];
    float  normal[3];
    int    snap_mask;
    int    snaped;
    int    flags; // Union of CURSOR_* values.
    float  snap_offset; // XXX: fix this.
} cursor_t;

// #### 3d gestures
struct gesture3d
{
    int         type;
    int         state;
    int         buttons; // CURSOR_SHIFT | CURSOR_CTRL
    cursor_t    *cursor;
    int         (*callback)(gesture3d_t *gest, void *user);
};

int gesture3d(gesture3d_t *gest, cursor_t *curs, void *user);

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

/* ################################
 * Section: Gui
 */

void gui_release(void);
void gui_iter(const inputs_t *inputs);
void gui_render(void);

// Gui widgets:
bool gui_collapsing_header(const char *label);
void gui_text(const char *label, ...);
bool gui_button(const char *label, float w, int icon);
bool gui_button_right(const char *label, int icon);
void gui_group_begin(const char *label);
void gui_group_end(void);
bool gui_checkbox(const char *label, bool *v, const char *hint);
bool gui_input_int(const char *label, int *v, int minv, int maxv);
bool gui_input_float(const char *label, float *v, float step,
                     float minv, float maxv, const char *format);
bool gui_angle(const char *id, float *v, int vmin, int vmax);
bool gui_bbox(float box[4][4]);
bool gui_quat(const char *label, float q[4]);
bool gui_action_button(const char *id, const char *label, float size,
                       const char *sig, ...);
bool gui_action_checkbox(const char *id, const char *label);
bool gui_selectable(const char *name, bool *v, const char *tooltip, float w);
bool gui_selectable_toggle(const char *name, int *v, int set_v,
                           const char *tooltip, float w);
bool gui_selectable_icon(const char *name, bool *v, int icon);
bool gui_color(const char *label, uint8_t color[4]);
bool gui_color_small(const char *label, uint8_t color[4]);
bool gui_input_text(const char *label, char *buf, int size);
bool gui_input_text_multiline(const char *label, char *buf, int size,
                              float width, float height);
void gui_input_text_multiline_highlight(int line);
bool gui_combo(const char *label, int *v, const char **names, int nb);

float gui_get_avail_width(void);
void gui_same_line(void);
void gui_enabled_begin(bool enabled);
void gui_enabled_end(void);
// Add an icon in top left corner of last item.
void gui_floating_icon(int icon);

void gui_alert(const char *title, const char *msg);

void gui_columns(int count);
void gui_next_column(void);
void gui_separator(void);

void gui_push_id(const char *id);
void gui_pop_id(void);

enum {
    GUI_POPUP_FULL      = 1 << 0,
    GUI_POPUP_RESIZE    = 1 << 1,
};


/*
 * Function: gui_open_popup
 * Open a modal popup.
 *
 * Parameters:
 *    title - The title of the popup.
 *    flags - Union of <GUI_POPUP_FLAGS> values.
 *    data  - Data passed to the popup.  It will be automatically released
 *            by the gui.
 *    func  - The popup function, that render the popup gui.  Should return
 *            true to close the popup.
 */
void gui_open_popup(const char *title, int flags, void *data,
                    bool (*func)(void *data));
void gui_popup_body_begin(void);
void gui_popup_body_end(void);

// #############################

void wavefront_export(const mesh_t *mesh, const char *path);
void ply_export(const mesh_t *mesh, const char *path);

// ##### Assets manager ########################
// All the assets are saved in binary directly in the code, using
// tool/create_assets.py.

const void *assets_get(const char *url, int *size);

// List all the assets in a given asset dir.
// Return the number of assets.
// If f returns not 0, the asset is skipped.
int assets_list(const char *url, void *user,
                int (*f)(int i, const char *path, void *user));

// Basic mustache templates support
// Check povray.c for an example of usage.

typedef struct mustache mustache_t;
mustache_t *mustache_root(void);
mustache_t *mustache_add_dict(mustache_t *m, const char *key);
mustache_t *mustache_add_list(mustache_t *m, const char *key);
void mustache_add_str(mustache_t *m, const char *key, const char *fmt, ...);
int mustache_render(const mustache_t *m, const char *templ, char *out);
void mustache_free(mustache_t *m);

// ####### Cache manager #########################
// Allow to cache blocks merge operations.
typedef struct cache cache_t;
// Create a new cache with a given max size (in byte).
cache_t *cache_create(int size);
// Add an item into the cache.
// Inputs:
//  key, keylen     Define the unique key for the cache item.
//  data            Pointer to the item data.  The cache takes ownership.
//  cost            Cost of the data used to compute the cache usage.
//                  It doesn't have to be the size.
//  delfunc         Function that the cache can use to free the data.
void cache_add(cache_t *cache, const void *key, int keylen, void *data,
               int cost, int (*delfunc)(void *data));
// Return an item from the cache.
// Returns
//  The data owned by the cache, or NULL if no item with this key is in
//  the cache.
void *cache_get(cache_t *cache, const void *key, int keylen);

// ####### Sound #################################
void sound_init(void);
void sound_play(const char *sound, float volume, float pitch);
void sound_iter(void);

bool sound_is_enabled(void);
void sound_set_enabled(bool v);

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
