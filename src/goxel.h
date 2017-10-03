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

#ifndef GOXEL_H
#define GOXEL_H

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#ifndef NOMINMAX
#   define NOMINMAX
#endif

#include "vec.h"
#include "utlist.h"
#include "uthash.h"
#include "utarray.h"
#include "ivec.h"
#include "noc_file_dialog.h"
#include "block_def.h"
#include <float.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define GOXEL_VERSION_STR "0.7.1"

// #### Set the DEBUG macro ####
#ifndef DEBUG
#   if !defined(NDEBUG)
#       define DEBUG 1
#   else
#       define DEBUG 0
#   endif
#endif
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
       if (check_gl_errors(__FILE__, __LINE__)) assert(false);          \
   })
#else
#  define GL(line) line
#endif
// #############################



// ### Some useful inline functions / macros.

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define SWAP(x0, x) {typeof(x0) tmp = x0; x0 = x; x = tmp;}

// Used to pass and get values to callback 'void *user' arguments.
#define USER_PASS(...) ((const void*[]){__VA_ARGS__})
#define USER_GET(var, n) (((void**)var)[n])

// IS_IN(x, ...): returns true if x is equal to any of the other arguments.
#define IS_IN(x, ...) ({ \
        bool _ret = false; \
        const typeof(x) _V[] = {__VA_ARGS__}; \
        int _i; \
        for (_i = 0; _i < (int)ARRAY_SIZE(_V); _i++) \
            if (x == _V[_i]) _ret = true; \
        _ret; \
    })

static inline uvec4b_t HEXCOLOR(uint32_t v)
{
    return uvec4b((v >> 24) & 0xff,
                  (v >> 16) & 0xff,
                  (v >>  8) & 0xff,
                  (v >>  0) & 0xff);
}

static inline vec4_t uvec4b_to_vec4(uvec4b_t v)
{
    return vec4(v.x / 255., v.y / 255., v.z / 255., v.w / 255.);
}

// Convertion between radian and degree.
#define DR2D (180 / M_PI)
#define DD2R (M_PI / 180)

#define KB 1024
#define MB (1024 * KB)
#define GB (1024 * MB)

#define min(a, b) ({ \
      __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a < _b ? _a : _b; \
      })

#define max(a, b) ({ \
      __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
      _a > _b ? _a : _b; \
      })

#define max3(x, y, z) (max((x), max((y), (z))))
#define min3(x, y, z) (min((x), min((y), (z))))

#define clamp(x, a, b) (min(max(x, a), b))

#define sign(x) ({ \
      __typeof__ (x) _x = (x); \
      (_x > 0) ? +1 : (_x < 0)? -1 : 0; \
      })

#define cmp(a, b) ({ \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    (_a > _b) ? +1 : (_a < _b) ? -1 : 0; \
})

static inline float smoothstep(float edge0, float edge1, float x)
{
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static inline float mix(float x, float y, float t)
{
    return (1.0 - t) * x + t * y;
}

static inline void set_flag(int *x, int flag, bool v)
{
    v ? (*x |= flag) : (*x &= ~flag);
}

// #############################

// XXX: I should clean up a but the code of vec.h so that I can put those on
// top.
#include "box.h"
#include "plane.h"


// #### Utils ##################

char *read_file(const char *path, int *size);

// Used internally by the LOG macro
void dolog(int level, const char *msg,
           const char *func, const char *file, int line, ...);
int check_gl_errors(const char *file, int line);

// Enum of all the gl extensions we care for.
enum {
    GOX_GL_QCOM_tiled_rendering,
    GOX_GL_OES_packed_depth_stencil,
    GOX_GL_EXT_discard_framebuffer,

    GOX_GL_EXTENSIONS_COUNT
};
bool _has_gl_extension(int extension);
#define has_gl_extension(x) (_has_gl_extension(GOX_##x))

int create_program(const char *vertex_shader, const char *fragment_shader,
                   const char *include);
void delete_program(int prog);
uint8_t *img_read(const char *path, int *width, int *height, int *bpp);
uint8_t *img_read_from_mem(const char *data, int size,
                           int *w, int *h, int *bpp);
void img_write(const uint8_t *img, int w, int h, int bpp, const char *path);
uint8_t *img_write_to_mem(const uint8_t *img, int w, int h, int bpp,
                          int *size);
void img_downsample(const uint8_t *img, int w, int h, int bpp,
                    uint8_t *out);
bool str_endswith(const char *str, const char *end);
bool str_startswith(const char *s1, const char *s2);
// Get gregorian date from unix time.
int unix_to_dtf(double t, int *iy, int *im, int *id, int *h, int *m, int *s);
int utf_16_to_8(const wchar_t *in16, char *out8, size_t size8);

static inline bool str_equ(const char *s1, const char *s2) {
    return strcmp(s1, s2) == 0;
}

// List all the files in a directory.
// flags: unused for the moment.
// Return the number of directory found.
int list_dir(const char *url, int flags, void *user,
             int (*f)(int i, const char *path, void *user));

// Convert from screen coordinate to world coordinates.
// Similar to gluUnproject.
vec3_t unproject(const vec3_t *win, const mat4_t *model,
                 const mat4_t *proj, const vec4_t *view);

// #############################



// #### System #################
void sys_log(const char *msg);
// List all the files in a directory.
int sys_list_dir(const char *dir,
                 int (*callback)(const char *dir, const char *name,
                                 void *user),
                 void *user);
const char *sys_get_user_dir(void);
int sys_make_dir(const char *path);

const char *sys_get_clipboard_text(void* user);
void sys_set_clipboard_text(void *user, const char *text);
GLuint sys_get_screen_framebuffer(void);
double sys_get_time(void); // Unix time.
// #############################


// #### Dialogs ################
enum {
    DIALOG_FLAG_SAVE    = 1 << 0,
    DIALOG_FLAG_OPEN    = 1 << 1,
    DIALOG_FLAG_DIR     = 1 << 2,
};

// #### Texture ################
enum {
    TF_DEPTH    = 1 << 0,
    TF_STENCIL  = 1 << 1,
    TF_MIPMAP   = 1 << 2,
    TF_KEEP     = 1 << 3,
    TF_RGB      = 1 << 4,
    TF_RGB_565  = 1 << 5,
    TF_HAS_TEX  = 1 << 6,
    TF_HAS_FB   = 1 << 7,
    TF_NEAREST  = 1 << 8,
};

typedef struct texture texture_t;
struct texture {
    texture_t   *next;      // All the textures are in a global list.
    int         ref;        // For reference copy.
    char        *path;      // Only for image textures.

    int         format;
    GLuint      tex;
    int tex_w, tex_h;       // The actual OpenGL texture size.
    int x, y, w, h;         // Position of the sub texture.
    int flags;
    // This is only used for buffer textures.
    GLuint framebuffer, depth, stencil;
};

texture_t *texture_new_image(const char *path, int flags);
texture_t *texture_new_surface(int w, int h, int flags);
texture_t *texture_new_buffer(int w, int h, int flags);
void texture_get_data(const texture_t *tex, int w, int h, int bpp,
                      uint8_t *buf);
void texture_save_to_file(const texture_t *tex, const char *path);

texture_t *texture_copy(texture_t *tex);
void texture_delete(texture_t *tex);
// #############################


// #### Action #################

// We support some basic reflexion of functions.  We do this by registering the
// functions with the ACTION_REGISTER macro.  Once a function has been
// registered, it is possible to query it (action_get) and call it
// (action_exec).
// Check the end of image.c to see some examples.  The idea is that this will
// make it much easier to add meta information to functions, like
// documentation, shortcuts.  Also in theory this should allow to add a
// scripting engine on top of goxel quite easily.

// XXX: this is still pretty experimental.  This might change in the future.

typedef struct astack astack_t;

astack_t *stack_create(void);
void      stack_delete(astack_t *s);

void  stack_clear(astack_t *s);
int   stack_size(const astack_t *s);
char  stack_type(const astack_t *s, int i);
void  stack_push_i(astack_t *s, int i);
void  stack_push_p(astack_t *s, void *p);
void  stack_push_b(astack_t *s, bool b);
int   stack_get_i(const astack_t *s, int i);
void *stack_get_p(const astack_t *s, int i);
bool  stack_get_b(const astack_t *s, int i);
void  stack_pop(astack_t *s);

enum {
    ACTION_TOUCH_IMAGE    = 1 << 0,  // Push the undo history.
    // Toggle actions accept and return a boolean value.
    ACTION_TOGGLE         = 1 << 1,
};

// Represent an action.
typedef struct action action_t;
struct action {
    const char      *id;            // Globally unique id.
    const char      *help;          // Help text.
    int             flags;
    const char      *shortcut;      // Optional shortcut.
    int             icon;           // Optional icon id.
    int             (*func)(const action_t *a, astack_t *s);
    void            *data;

    // cfunc and csig can be used to directly call any function.
    void            *cfunc;
    const char      *csig;

    // Used for export / import actions.
    struct {
        const char  *name;
        const char  *ext;
    } file_format;
};

void action_register(const action_t *action);
const action_t *action_get(const char *id);
int action_exec(const action_t *action, const char *sig, ...);
int action_execv(const action_t *action, const char *sig, va_list ap);
void actions_iter(int (*f)(const action_t *action, void *user), void *user);

// Convenience macro to call action_exec directly from an action id.
#define action_exec2(id, sig, ...) \
    action_exec(action_get(id), sig, ##__VA_ARGS__)


// Convenience macro to register an action from anywere in a c file.
#define ACTION_REGISTER(id_, ...) \
    static const action_t GOX_action_##id_ = {.id = #id_, __VA_ARGS__}; \
    static void GOX_register_action_##id_() __attribute__((constructor)); \
    static void GOX_register_action_##id_() { \
        action_register(&GOX_action_##id_); \
    }

// #############################


// All the icons positions inside icon.png (as Y*8 + X + 1).
enum {
    ICON_NULL = 0,

    ICON_TOOL_BRUSH = 1,
    ICON_TOOL_SHAPE = 2,
    ICON_TOOL_LASER = 3,
    ICON_TOOL_PLANE = 4,
    ICON_TOOL_MOVE = 5,
    ICON_TOOL_PICK = 6,
    ICON_TOOL_SELECTION = 7,
    ICON_TOOL_PROCEDURAL = 8,

    ICON_MODE_ADD = 9,
    ICON_MODE_SUB = 10,
    ICON_MODE_PAINT = 11,
    ICON_TOOL_EXTRUDE = 12,

    ICON_SHAPE_SPHERE = 17,
    ICON_SHAPE_CUBE = 18,
    ICON_SHAPE_CYLINDER = 19,

    ICON_ADD = 25,
    ICON_REMOVE = 26,
    ICON_ARROW_DOWNWARD = 27,
    ICON_ARROW_UPWARD = 28,
    ICON_VISIBILITY = 29,
    ICON_VISIBILITY_OFF = 30,
    ICON_EDIT = 31,
    ICON_LINK = 32,
};


// #### Tool/Operation/Painter #
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
    float (*func)(const vec3_t *p, const vec3_t *s, float smoothness);
} shape_t;

void shapes_init(void);
extern shape_t shape_sphere;
extern shape_t shape_cube;
extern shape_t shape_cylinder;


// The painting context, including the tool, brush, mode, radius,
// color, etc...
typedef struct painter {
    int             mode;
    const shape_t   *shape;
    uvec4b_t        color;
    float           smoothness;
    int             symmetry; // bitfield X Y Z
    box_t           *box;     // Clipping box (can be null)
} painter_t;

// #### Block ##################
// The block size can only be 16.
#define BLOCK_SIZE 16
#define VOXEL_TEXTURE_SIZE 8
// Number of sub position per voxel in the marching
// cube rendering.
#define MC_VOXEL_SUB_POS 4 // XXX: try to make it higher (up to 16!)

// Structure used for the OpenGL array data of blocks.
// XXX: we can probably make it smaller.
typedef struct voxel_vertex
{
    int8_t   pos[3]                     __attribute__((aligned(4)));
    int8_t   normal[3]                  __attribute__((aligned(4)));
    uint8_t  color[4]                   __attribute__((aligned(4)));
    uint32_t pos_data                   __attribute__((aligned(4)));
    uint8_t  uv[2]                      __attribute__((aligned(4)));
    uint8_t  bshadow_uv[2]              __attribute__((aligned(4)));
    uint8_t  bump_uv[2]                 __attribute__((aligned(4)));
} voxel_vertex_t;


// #### Mesh ###################

typedef struct mesh mesh_t;
typedef struct block block_t;

// Fast iterator of all the mesh voxel.
typedef struct {
    block_t *block;
    int pos[3];

    union {
        bool finished;
        bool found;
    };
} mesh_iterator_t;

mesh_t *mesh_new(void);
void mesh_clear(mesh_t *mesh);
void mesh_delete(mesh_t *mesh);
mesh_t *mesh_copy(const mesh_t *mesh);
void mesh_set(mesh_t *mesh, const mesh_t *other);
box_t mesh_get_box(const mesh_t *mesh, bool exact);
void mesh_op(mesh_t *mesh, painter_t *painter, const box_t *box);
void mesh_merge(mesh_t *mesh, const mesh_t *other, int op);
void mesh_move(mesh_t *mesh, const mat4_t *mat);
void mesh_get_at(const mesh_t *mesh, const int pos[3],
                 mesh_iterator_t *iter, uint8_t out[4]);
uint8_t mesh_get_alpha_at(const mesh_t *mesh, const int pos[3],
                          mesh_iterator_t *iter);
void mesh_set_at(mesh_t *mesh, const int pos[3], const uint8_t v[4],
                 mesh_iterator_t *iter);
void mesh_remove_empty_blocks(mesh_t *mesh);
bool mesh_is_empty(const mesh_t *mesh);
// XXX: to cleanup.
void mesh_extrude(mesh_t *mesh, const plane_t *plane, const box_t *box);

void mesh_blit(mesh_t *mesh, const uint8_t *data,
               int x, int y, int z, int w, int h, int d,
               mesh_iterator_t *iter);

void mesh_shift_alpha(mesh_t *mesh, int v);

// Compute the selection mask for a given condition.
int mesh_select(const mesh_t *mesh,
                const int start_pos[3],
                int (*cond)(const uint8_t value[4],
                            const uint8_t neighboors[6][4],
                            const uint8_t mask[6],
                            void *user),
                void *user, mesh_t *selection);

bool mesh_iter_voxels(const mesh_t *mesh, mesh_iterator_t *it,
                      int pos[3], uint8_t value[4]);

bool mesh_iter_blocks(const mesh_t *mesh, mesh_iterator_t *it,
                      int pos[3], uint64_t *data_id, int *block_id,
                      block_t **block);
uint64_t mesh_get_id(const mesh_t *mesh);
void *mesh_get_block_data(const mesh_t *mesh, const block_t *block);


int mesh_generate_vertices(const mesh_t *mesh, const block_t *block,
                           const int block_pos[3],
                           int effects, int block_id, voxel_vertex_t *out);

#define MESH_ITER_BLOCKS(m, pos, data_id, id, b) \
    for (mesh_iterator_t it_ = {0}; \
            mesh_iter_blocks(m, &it_, pos, data_id, id, &b);)

// Convenience macro to iter all the voxels of a mesh.
// Given:
//    m         The mesh pointer.
//    x, y, z   integer, set to the position of the voxel.
//    v         uvec4b_t, set to the color of the voxel.
#define MESH_ITER_VOXELS(m, x, y, z, v_) \
    for (struct {mesh_iterator_t it; int p[3]; uint8_t v[4];} i_ = {0}; \
         mesh_iter_voxels(m, &i_.it, i_.p, i_.v);) \
        if (i_.v[3]) \
            if (x = i_.p[0], y = i_.p[1], z = i_.p[2], memcpy(v_, i_.v, 4), 1)

// #############################



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
    mat4_t view_mat;
    mat4_t proj_mat;
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
void render_plane(renderer_t *rend, const plane_t *plane,
                  const uint8_t color[4]);
void render_line(renderer_t *rend, const vec3_t *a, const vec3_t *b,
                 const uint8_t color[4]);
void render_box(renderer_t *rend, const box_t *box,
                const uint8_t color[4], int effects);
void render_sphere(renderer_t *rend, const mat4_t *mat);
void render_img(renderer_t *rend, texture_t *tex, const mat4_t *mat,
                int efffects);
void render_rect(renderer_t *rend, const plane_t *plane, int effects);
// Flushes all the queued render items.  Actually calls opengl.
//  rect: the viewport rect (passed to glViewport).
//  clear_color: clear the screen with this first.
void render_render(renderer_t *rend, const int rect[4],
                   const uint8_t clear_color[4]);
int render_get_default_settings(int i, char **name, render_settings_t *out);
// Compute the light direction in the model coordinates (toward the light)
vec3_t render_get_light_dir(const renderer_t *rend);

// #############################




// #### Model3d ################

typedef struct {
     vec3_t   pos       __attribute__((aligned(4)));
     vec3_t   normal    __attribute__((aligned(4)));
     uvec4b_t color     __attribute__((aligned(4)));
     vec2_t   uv        __attribute__((aligned(4)));
} model_vertex_t;

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

void model3d_init(void);
model3d_t *model3d_cube(void);
model3d_t *model3d_wire_cube(void);
model3d_t *model3d_sphere(int slices, int stacks);
model3d_t *model3d_grid(int nx, int ny);
model3d_t *model3d_line(void);
model3d_t *model3d_rect(void);
model3d_t *model3d_wire_rect(void);
void model3d_render(model3d_t *model3d,
                    const mat4_t *model, const mat4_t *proj,
                    const uint8_t color[4],
                    const texture_t *tex,
                    const vec3_t *light,
                    int   effects);

// #### Palette ################

typedef struct {
    uvec4b_t color;
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
    vec2_t  pos;
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



// #### Mouse gestures #####################################3

enum {
    // Supported type of gestures.
    GESTURE_DRAG        = 1 << 0,
    GESTURE_CLICK       = 1 << 1,
    GESTURE_PINCH       = 1 << 2,
    GESTURE_HOVER       = 1 << 3,

    // Gesture states.
    GESTURE_POSSIBLE = 0,
    GESTURE_RECOGNISED,
    GESTURE_BEGIN,
    GESTURE_UPDATE,
    GESTURE_END,
    GESTURE_TRIGGERED,
    GESTURE_FAILED,
};

typedef struct gesture gesture_t;
struct gesture
{
    int     type;
    int     button;
    int     state;
    vec4_t  view;
    vec2_t  pos;
    vec2_t  start_pos[2];
    float   pinch;
    float   rotation;
    int     (*callback)(const gesture_t *gest, void *user);
};

int gesture_update(int nb, gesture_t *gestures[],
                   const inputs_t *inputs, const vec4_t *view, void *user);


// #############################

typedef struct camera camera_t;
struct camera
{
    camera_t  *next, *prev; // List of camera in an image.
    char   name[128];  // 127 chars max.
    bool   ortho; // Set to true for orthographic projection.
    float  dist;
    quat_t rot;
    vec3_t ofs;
    float  fovy;
    float  aspect;

    // Auto computed from other values:
    mat4_t view_mat;    // Model to view transformation.
    mat4_t proj_mat;    // Proj transform from camera coordinates.
};

camera_t *camera_new(const char *name);
void camera_delete(camera_t *camera);
void camera_set(camera_t *camera, const camera_t *other);
void camera_update(camera_t *camera);
// Adjust the camera settings so that the rotation works for a given
// position.
void camera_set_target(camera_t *camera, const vec3_t *pos);

// Get the raytracing ray of the camera at a given screen position.
// win:     pixel position in screen coordinates.
// view:    viewport rect: [min_x, min_y, max_x, max_y].
// o:       output ray origin.
// d:       output ray direction.
void camera_get_ray(const camera_t *camera, const vec2_t *win,
                    const vec4_t *view, vec3_t *o, vec3_t *d);

typedef struct history history_t;

typedef struct layer layer_t;
struct layer {
    layer_t     *next, *prev;
    mesh_t      *mesh;
    int         id;         // Uniq id in the image (for clones).
    bool        visible;
    char        name[128];  // 127 chars max.
    mat4_t      mat;
    // For 2d image layers.
    texture_t   *image;
    // For clone layers:
    int         base_id;
    uint64_t    base_mesh_id;
};

typedef struct image image_t;
struct image {
    layer_t *layers;
    layer_t *active_layer;
    camera_t *cameras;
    camera_t *active_camera;
    box_t    box;

    // For saving.
    char    *path;
    int     export_width;
    int     export_height;

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
int proc_start(gox_proc_t *proc, const box_t *box);
int proc_stop(gox_proc_t *proc);
int proc_iter(gox_proc_t *proc);

// Get the list of programs saved in data/procs.
int proc_list_examples(void (*f)(int index,
                                 const char *name, const char *code,
                                 void *user), void *user);


// Represent a 3d cursor.
// The program keeps track of two cursors, that are then used by the tools.

enum {
    // The state flags of the cursor.
    CURSOR_PRESSED      = 1 << 0,
    CURSOR_SHIFT        = 1 << 1,
    CURSOR_CTRL         = 1 << 2,
};

typedef struct cursor {
    vec3_t pos;
    vec3_t normal;
    int    snap_mask;
    int    snaped;
    int    flags; // Union of CURSOR_* values.
    float  snap_offset; // XXX: fix this.
} cursor_t;

// #### 3d gestures
typedef struct gesture3d gesture3d_t;
struct gesture3d
{
    int         type;
    int         state;
    cursor_t    *cursor;
    int         (*callback)(gesture3d_t *gest, void *user);
};

int gesture3d(gesture3d_t *gest, cursor_t *curs, void *user);

enum {
    // Tools flags.
    TOOL_REQUIRE_CAN_EDIT = 1 << 0, // Set to tools that can edit the layer.
    TOOL_REQUIRE_CAN_MOVE = 1 << 1, // Set to tools that can move the layer.
};

// Tools
typedef struct tool tool_t;
struct tool {
    int id;
    const char *action_id;
    int (*iter_fn)(tool_t *tool, const vec4_t *view);
    int (*gui_fn)(tool_t *tool);
    const char *shortcut;
    int state; // XXX: to be removed I guess.
    int flags;
};

#define TOOL_REGISTER(id_, name_, klass_, ...) \
    static klass_ GOX_tool_##id_ = {\
            .tool = { \
                .id = id_, .action_id = "tool_set_" #name_, __VA_ARGS__ \
            } \
        }; \
    static void GOX_register_tool_##tool_() __attribute__((constructor)); \
    static void GOX_register_tool_##tool_() { \
        tool_register_(&GOX_tool_##id_.tool); \
    }

void tool_register_(const tool_t *tool);
tool_t *tool_get(int id);
int tool_iter(tool_t *tool, const vec4_t *view);
int tool_gui(tool_t *tool);

int tool_gui_snap(void);
int tool_gui_mode(void);
int tool_gui_shape(void);
int tool_gui_radius(void);
int tool_gui_smoothness(void);
int tool_gui_color(void);
int tool_gui_symmetry(void);



typedef struct goxel
{
    int        screen_size[2];
    float      screen_scale;
    image_t    *image;

    mesh_t     *layers_mesh; // All the layers combined.
    mesh_t     *pick_mesh;   // Used for picking (always layers_mesh?)
    mesh_t     *tool_mesh;   // Preview of the tool action.
    mesh_t     *render_mesh; // All the layers + tool mesh.

    struct     {
        mesh_t *mesh;
        box_t  box;
    } clipboard;

    history_t  *history;     // Undo/redo history.
    int        snap_mask;    // Global snap mask (can edit in the GUI).
    float      snap_offset;  // Only for brush tool, remove that?

    plane_t    plane;         // The snapping plane.
    bool       plane_hidden;  // Set to true to hide the plane.
    bool       show_export_viewport;

    camera_t   camera;

    uvec4b_t   back_color;
    uvec4b_t   grid_color;
    uvec4b_t   image_box_color;

    texture_t  *pick_fbo;
    painter_t  painter;
    renderer_t rend;

    cursor_t   cursor;

    tool_t     *tool;
    float      tool_radius;

    // Some state for the tool iter functions.
    plane_t    tool_plane;
    bool       tool_shape_two_steps; // Param of the shape tool.

    box_t      selection;   // The selection box.

    struct {
        quat_t rotation;
        vec2_t pos;
        vec3_t camera_ofs;
    } move_origin;

    gox_proc_t proc;        // The current procedural rendering (if any).

    palette_t  *palettes;   // The list of all the palettes
    palette_t  *palette;    // The current color palette
    char       *help_text;  // Seen in the bottom of the screen.
    char       *hint_text;  // Seen in the bottom of the screen.

    int        frame_count;       // Global frames counter.
    double     frame_time;        // Clock time at beginning of the frame.

    // Global uid counter.
    uint64_t   next_uid;

    int        block_count; // Counter for the number of block data.
    bool       quit;        // Set to true to quit the application.

    struct {
        gesture_t drag;
        gesture_t pan;
        gesture_t rotate;
        gesture_t hover;
    } gestures;

} goxel_t;

// the global goxel instance.
extern goxel_t *goxel;

void goxel_init(goxel_t *goxel);
void goxel_release(goxel_t *goxel);
void goxel_iter(goxel_t *goxel, inputs_t *inputs);
void goxel_render(goxel_t *goxel);
void goxel_render_view(goxel_t *goxel, const vec4_t *rect);
// Called by the gui when the mouse hover a 3D view.
// XXX: change the name since we also call it when the mouse get out of
// the view.
void goxel_mouse_in_view(goxel_t *goxel, const vec4_t *view,
                         const inputs_t *inputs);

int goxel_unproject(goxel_t *goxel, const vec4_t *view,
                    const vec2_t *pos, int snap_mask, float offset,
                    vec3_t *out, vec3_t *normal);

bool goxel_unproject_on_mesh(goxel_t *goxel, const vec4_t *view,
                     const vec2_t *pos, mesh_t *mesh,
                     vec3_t *out, vec3_t *normal);

bool goxel_unproject_on_plane(goxel_t *goxel, const vec4_t *view,
                     const vec2_t *pos, const plane_t *plane,
                     vec3_t *out, vec3_t *normal);
bool goxel_unproject_on_box(goxel_t *goxel, const vec4_t *view,
                     const vec2_t *pos, const box_t *box, bool inside,
                     vec3_t *out, vec3_t *normal, int *face);
// Recompute the meshes.  mask from MESH_ enum.
void goxel_update_meshes(goxel_t *goxel, int mask);

void goxel_set_help_text(goxel_t *goxel, const char *msg, ...);
void goxel_set_hint_text(goxel_t *goxel, const char *msg, ...);

void goxel_import_image_plane(goxel_t *goxel, const char *path);

// Render the view into an RGBA buffer.
void goxel_render_to_buf(uint8_t *buf, int w, int h);

// #############################

void save_to_file(goxel_t *goxel, const char *path);
void load_from_file(goxel_t *goxel, const char *path);


// #### Colors functions #######
void hsl_to_rgb(const uint8_t hsl[3], uint8_t rgb[3]);
void rgb_to_hsl(const uint8_t rgb[3], uint8_t hsl[3]);

// #### Gui ####################

#define THEME_SIZES(X) \
    X(item_height) \
    X(icons_height) \
    X(item_padding_h) \
    X(item_rounding) \
    X(item_spacing_h) \
    X(item_spacing_v) \
    X(item_inner_spacing_h)


enum {
    THEME_GROUP_BASE,
    THEME_GROUP_WIDGET,
    THEME_GROUP_TAB,
    THEME_GROUP_MENU,
    THEME_GROUP_COUNT
};

enum {
    THEME_COLOR_BACKGROUND,
    THEME_COLOR_OUTLINE,
    THEME_COLOR_INNER,
    THEME_COLOR_INNER_SELECTED,
    THEME_COLOR_TEXT,
    THEME_COLOR_TEXT_SELECTED,
    THEME_COLOR_COUNT
};

typedef struct {
    const char *name;
    int parent;
    bool colors[THEME_COLOR_COUNT];
} theme_group_info_t;
extern theme_group_info_t THEME_GROUP_INFOS[THEME_GROUP_COUNT];

typedef struct {
    const char *name;
} theme_color_info_t;
extern theme_color_info_t THEME_COLOR_INFOS[THEME_COLOR_COUNT];

typedef struct {
    uint8_t colors[THEME_COLOR_COUNT][4];
} theme_group_t;

typedef struct theme theme_t;
struct theme {
    char name[64];

    struct {
        int  item_height;
        int  icons_height;
        int  item_padding_h;
        int  item_rounding;
        int  item_spacing_h;
        int  item_spacing_v;
        int  item_inner_spacing_h;
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

void gui_release(void);
void gui_iter(goxel_t *goxel, const inputs_t *inputs);
void gui_render(void);

// Gui widgets:
void gui_text(const char *label);
bool gui_button(const char *label, float w, int icon);
void gui_group_begin(const char *label);
void gui_group_end(void);
bool gui_checkbox(const char *label, bool *v, const char *hint);
bool gui_input_int(const char *label, int *v, int minv, int maxv);
bool gui_input_float(const char *label, float *v, float step,
                     float minv, float maxv, const char *format);
bool gui_angle(const char *id, float *v, int vmin, int vmax);
bool gui_quat(const char *label, quat_t *q);
bool gui_action_button(const char *id, const char *label, float size,
                       const char *sig, ...);
bool gui_action_checkbox(const char *id, const char *label);
bool gui_selectable(const char *name, bool *v, const char *tooltip, float w);
bool gui_selectable_icon(const char *name, bool *v, int icon);
bool gui_color(const char *label, uvec4b_t *color);
bool gui_input_text(const char *label, char *buf, int size);
bool gui_input_text_multiline(const char *label, char *buf, int size,
                              float width, float height);
void gui_input_text_multiline_highlight(int line);
bool gui_combo(const char *label, int *v, const char **names, int nb);

float gui_get_avail_width(void);
void gui_same_line(void);
void gui_enabled_begin(bool enabled);
void gui_enabled_end(void);

enum {
    GUI_POPUP_FULL      = 1 << 0,
    GUI_POPUP_RESIZE    = 1 << 1,
};

void gui_open_popup(const char *title, bool (*func)(void), int flags);
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

cache_t *cache_create(int size);
void cache_add(cache_t *cache, const void *key, int keylen, void *data,
               int cost, int (*delfunc)(void *data));
void *cache_get(cache_t *cache, const void *key, int keylen);

// ####### Sound #################################
void sound_init(void);
void sound_play(const char *sound);
void sound_iter(void);


#endif // GOXEL_H
