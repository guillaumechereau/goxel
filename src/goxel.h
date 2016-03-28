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
#include <float.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define GOXEL_VERSION_STR "0.2.1"

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

static inline float smoothstep(float edge0, float edge1, float x)
{
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static inline float mix(float x, float y, float t)
{
    return (1.0 - t) * x + t * y;
}

// #############################

// XXX: I should clean up a but the code of vec.h so that I can put those on
// top.
#include "box.h"
#include "plane.h"


// #### Utils ##################

// Get the clock in nanoseconds
int64_t get_clock(void);

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

static inline bool str_equ(const char *s1, const char *s2) {
    return strcmp(s1, s2) == 0;
}

// List all the files in a directory.
// flags: unused for the moment.
// Return the number of directory found.
int list_dir(const char *url, int flags, void *user,
             int (*f)(int i, const char *path, void *user));

// #############################



// #### System #################
void sys_log(const char *msg);
const char *sys_get_data_dir(void);
bool sys_asset_exists(const char *path);
char *sys_read_asset(const char *path, int *size);
const char *sys_get_clipboard_text(void);
void sys_set_clipboard_text(const char *text);
GLuint sys_get_screen_framebuffer(void);
// #############################


// #### Dialogs ################
enum {
    DIALOG_FLAG_SAVE    = 1 << 0,
    DIALOG_FLAG_OPEN    = 1 << 1,
    DIALOG_FLAG_DIR     = 1 << 2,
};
bool dialog_open(int flags, const char *filter, char **out);
// #############################


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

texture_t *texture_new_image(const char *path);
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
// (action_exec).  There is some basic support for default and named arguments.
// Check the end of image.c to see some examples.  The idea is that this will
// make it much easier to add meta information to functions, like
// documentation, shortcuts.  Also in theory this should allow to add a
// scripting engine on top of goxel quite easily.

// XXX: this is still pretty experimental.  This might change in the future.

// For reflexion, we need to keep an id for all the types we can pass to
// actions.
enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_STRING,
    TYPE_GOXEL,
    TYPE_LAYER,
    TYPE_IMAGE,
    TYPE_FILE_PATH,
    TYPE_BOX,
};

// Structure used both to define an action argument signature (name, type),
// or an action call argument value (name, value).  In that case the type
// can be inferred from the action signature.
typedef struct {
    const char  *name;
    union {
        long        type;
        long        value;
        const char  *s;
    };
} arg_t;

#define ARG(n, v) {n, {(long)v}}
#define ARGS(...) (const arg_t[]){__VA_ARGS__, ARG(0, 0)}

// Represent a function signature with return type and arguments.
typedef struct {
    int         ret;
    int         nb_args;
    const arg_t *args;      // Terminated by a argument of type VOID
} action_sig_t;

// Convenience macro to create a function signature:
// SIG(ret_type, ARG(arg1_name, arg1_type), ARG(arg2_name, arg2_type), ...)
#define SIG(ret_, ...) { \
        ret_, \
        ARRAY_SIZE(((arg_t[]){__VA_ARGS__})), \
        (const arg_t[]){__VA_ARGS__, ARG(0, 0)} \
    }

enum {
    ACTION_NO_CHANGE    = 1 << 0,  // The action does not touch the image.
};

// Represent an action.
typedef struct action action_t;
struct action {
    const char      *id;    // Globally unique id.
    const char      *help;  // Help text.
    void            *func;  // Pointer to the function to call.
    action_sig_t    sig;    // Signature of the function.
    int             flags;
};

void action_register(const action_t *action);
const action_t *action_get(const char *id);
void *action_exec(const action_t *action, const arg_t *args);

// Convenience macro to call action_exec directly from an id and a list of
// arguments.
// I have to add a ARG(0, 0) at the beginning of the arg list so that the
// macro works even with no action arguments.  Maybe I should add a nb_args
// argument to action_exec to prevent that.
#define action_exec2(id, ...) ({ \
        const arg_t args[] = {ARG(0, 0), ##__VA_ARGS__, ARG(0, 0)}; \
        action_exec(action_get(id), args + 1); \
    })



// Convenience macro to register an action from anywere in a c file.
#define ACTION_REGISTER(id_, ...) \
    static const action_t action_##id_ = {.id = #id_, __VA_ARGS__}; \
    static void register_action_##id_() __attribute__((constructor)); \
    static void register_action_##id_() { \
        action_register(&action_##id_); \
    }

// #############################

// #### Tool/Operation/Painter #
enum {
    OP_NULL,
    OP_ADD,
    OP_SUB,
    OP_PAINT,
    OP_INTERSECT,
};

enum {
    TOOL_NONE = 0,
    TOOL_BRUSH,
    TOOL_CUBE,
    TOOL_LASER,
    TOOL_SET_PLANE,
    TOOL_MOVE,
    TOOL_PICK_COLOR,
    TOOL_SELECTION,
    TOOL_PROCEDURAL,
};

typedef struct shape {
    const char *id;
    float (*func)(const vec3_t *p, const vec3_t *s, float smoothness);
} shape_t;

void shapes_init(void);
extern shape_t shape_sphere;
extern shape_t shape_cube;
extern shape_t shape_cylinder;


// The painting context, including the tool, brush, operation, radius,
// color, etc...
typedef struct painter {
    int             op;
    const shape_t   *shape;
    uvec4b_t        color;
    float           smoothness;
} painter_t;

// #### Block ##################
// The block size can only be 16.
#define BLOCK_SIZE 16
#define VOXEL_TEXTURE_SIZE 8
// Number of sub position per voxel in the marching
// cube rendering.
#define MC_VOXEL_SUB_POS 8

// Structure used for the OpenGL array data of blocks.
// XXX: we can probably make it smaller.
typedef struct voxel_vertex
{
    vec3b_t  pos        __attribute__((aligned(4)));
    vec3b_t  normal     __attribute__((aligned(4)));
    uvec4b_t color      __attribute__((aligned(4)));
    uvec2b_t pos_data   __attribute__((aligned(4)));
    uvec2b_t uv         __attribute__((aligned(4)));
    uvec2b_t bshadow_uv __attribute__((aligned(4)));
    uvec2b_t bump_uv    __attribute__((aligned(4)));
} voxel_vertex_t;

// We use copy on write for the block data, so that it is cheap to copy
// blocks.
typedef struct block_data block_data_t;
struct block_data
{
    int         ref;
    int         id;
    uvec4b_t    voxels[BLOCK_SIZE * BLOCK_SIZE * BLOCK_SIZE]; // RGBA voxels.
};

typedef struct block block_t;
struct block
{
    UT_hash_handle  hh;     // The hash table of pos -> blocks in a mesh.
    block_data_t    *data;
    vec3_t          pos;
    int             id;
};
block_t *block_new(const vec3_t *pos, block_data_t *data);
void block_delete(block_t *block);
block_t *block_copy(const block_t *other);
box_t block_get_box(const block_t *block, bool exact);
void block_fill(block_t *block,
                uvec4b_t (*get_color)(const vec3_t *pos, void *user_data),
                void *user_data);
int block_generate_vertices(const block_data_t *data, int effects,
                            voxel_vertex_t *out);
void block_op(block_t *block, painter_t *painter, const box_t *box);
bool block_is_empty(const block_t *block, bool fast);
void block_merge(block_t *block, const block_t *other);
uvec4b_t block_get_at(const block_t *block, const vec3_t *pos);

// XXX: I think we should clean up this one.
void block_blit(block_t *block, uvec4b_t *data,
                int x, int y, int z, int w, int h, int d);
void block_shift_alpha(block_t *block, int v);
// #############################



// #### Mesh ###################
typedef struct mesh mesh_t;
struct mesh
{
    block_t *blocks;
    int next_block_id;
    int *ref;   // Used to implement copy on write of the blocks.
};
mesh_t *mesh_new(void);
void mesh_clear(mesh_t *mesh);
void mesh_delete(mesh_t *mesh);
mesh_t *mesh_copy(const mesh_t *mesh);
void mesh_set(mesh_t **mesh, const mesh_t *other);
box_t mesh_get_box(const mesh_t *mesh, bool exact);
void mesh_fill(mesh_t *mesh,
               uvec4b_t (*get_color)(const vec3_t *pos, void *user_data),
               void *user_data);
void mesh_op(mesh_t *mesh, painter_t *painter, const box_t *box);
void mesh_merge(mesh_t *mesh, const mesh_t *other);
void mesh_add_block(mesh_t *mesh, block_data_t *data, const vec3_t *pos);
void mesh_move(mesh_t *mesh, const mat4_t *mat);
uvec4b_t mesh_get_at(const mesh_t *mesh, const vec3_t *pos);

// XXX: clean up this.  We should use a struct to represent a data cube.
void mesh_blit(mesh_t *mesh, uvec4b_t *data,
               int x, int y, int z,
               int w, int h, int d);
void mesh_shift_alpha(mesh_t *mesh, int v);

#define MESH_ITER_BLOCKS(m, b) for (b = m->blocks; b; b = b->hh.next)

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
};

typedef struct {
    float ambient;
    float diffuse;
    float specular;
    float shininess;
    float smoothness;
    int   effects;
    float border_shadow;
} render_settings_t;

typedef struct renderer renderer_t;
typedef struct render_item_t render_item_t;
struct renderer
{
    mat4_t view_mat;
    mat4_t proj_mat;

    struct {
        vec3_t direction;
        bool   fixed;
        float  intensity;
    } light;

    render_settings_t settings;

    render_item_t    *items;
};

void render_init(void);
void render_deinit(void);
void render_mesh(renderer_t *rend, const mesh_t *mesh, int effects);
void render_plane(renderer_t *rend, const plane_t *plane,
                  const uvec4b_t *color);
void render_line(renderer_t *rend, const vec3_t *a, const vec3_t *b,
                 const uvec4b_t *color);
void render_box(renderer_t *rend, const box_t *box, bool solid,
                const uvec4b_t *color, bool strip);
void render_sphere(renderer_t *rend, const mat4_t *mat);
void render_img(renderer_t *rend, texture_t *tex, const mat4_t *mat);
void render_render(renderer_t *rend);//, const mat4_t *view, const mat4_t *proj);
int render_get_default_settings(int i, char **name, render_settings_t *out);

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
void model3d_render(model3d_t *model3d,
                    const mat4_t *model, const mat4_t *proj,
                    const uvec4b_t *color,
                    const texture_t *tex,
                    bool  strip,
                    float fade, const vec3_t *fade_center);


// #### Palette ################

typedef struct palette {
   int      size;
   uvec4b_t *values;
} palette_t;

palette_t *palette_get();

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
    KEY_SHIFT       = 340,
    KEY_CONTROL     = 341,
};

// Flags to set where the mouse snap.  This might change in the future.
enum {
    SNAP_MESH  = 1 << 0,
    SNAP_PLANE = 1 << 1,
};

typedef struct inputs
{
    int         window_size[2];
    bool        keys[512]; // Table of all the pressed keys.
    uint32_t    chars[16];
    vec2_t      mouse_pos;
    bool        mouse_down[3];
    float       mouse_wheel;
} inputs_t;

typedef struct history history_t;

typedef struct layer layer_t;
struct layer {
    layer_t     *next, *prev;
    mesh_t      *mesh;
    bool        visible;
    char        name[128];  // 127 chars max.
    // mat and image can be used to render a 2d image on top of the layer.
    // This is convenient when want to draw something using a picture model.
    mat4_t      mat;
    texture_t   *image;
};

typedef struct image image_t;
struct image {
    layer_t *layers;
    layer_t *active_layer;

    // For saving.
    char    *path;
    int     export_width;
    int     export_height;

    image_t *history;
    image_t *history_next, *history_prev, *history_current;
};

image_t *image_new(void);
image_t *image_copy(image_t *img);
void image_delete(image_t *img);
layer_t *image_add_layer(image_t *img);
void image_delete_layer(image_t *img, layer_t *layer);
void image_move_layer(image_t *img, layer_t *layer, int d);
layer_t *image_duplicate_layer(image_t *img, layer_t *layer);
void image_merge_visible_layers(image_t *img);
void image_history_push(image_t *img);
void image_undo(image_t *img);
void image_redo(image_t *img);

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
int proc_start(gox_proc_t *proc);
int proc_stop(gox_proc_t *proc);
int proc_iter(gox_proc_t *proc);

// Get the list of programs saved in data/procs.
int proc_list_examples(void (*f)(int index,
                                 const char *name, const char *code));

// #############################

typedef struct goxel
{
    vec2i_t    screen_size;
    image_t    *image;

    mesh_t     *layers_mesh; // All the layers combined.
    mesh_t     *pick_mesh;

    history_t  *history;     // Undo/redo history.
    int        snap;
    float      snap_offset;  // Only for brush tool.

    plane_t    plane;         // The snapping plane.
    bool       plane_hidden;  // Set to true to hide the plane.

    // For the moment we only support a single view, so the matrix and
    // projection are here.  At some point I need to move this out in a
    // separate structure.
    struct {
        vec3_t ofs;
        quat_t rot;
        float  dist;
        float  zoom;
        vec4_t view;
        // If set, we smoothly update the offset to reach goxel->last_pos.
        bool   move_to_last_pos;

        // Auto computed from other values:
        mat4_t view_mat;    // Model to view transformation.
        mat4_t proj_mat;    // Proj transform from camera coordinates.
    } camera;
    vec3_t     last_pos;    // The last pos we painted to.

    uvec4b_t   back_color;
    uvec4b_t   grid_color;

    texture_t  *pick_fbo;
    painter_t  painter;
    renderer_t rend;

    int        tool;
    // Used when we change the effective tool with ctrl.
    int        prev_tool;
    float      tool_radius;

    // Some state for the tool iter functions.
    // XXX: move this into tool.c
    int        tool_state;
    int        tool_t;
    int        tool_snape_face;
    mesh_t     *tool_origin_mesh;
    // Structure used to skip rendering when don't move the mouse.
    struct     {
        vec3_t     pos;
        bool       pressed;
        int        op;
    }          tool_last_op;
    vec3_t     tool_start_pos;
    plane_t    tool_plane;

    box_t      selection;   // The selection box.

    struct {
        quat_t rotation;
        vec2_t pos;
        vec3_t camera_ofs;
    } move_origin;

    bool       painting;    // Set to true when we are in a painting operation.
    bool       moving;      // Set to true while in a movement operation.
    gox_proc_t proc;        // The current procedural rendering (if any).

    palette_t  *palette;    // The current color palette
    char       *help_text;  // Seen in the bottom of the screen.

    int        frame_count;       // Global frames counter.
    int64_t    frame_clock;       // Clock time at beginning of the frame.
    int        block_next_id;

    int        block_count; // Counter for the number of block data.
} goxel_t;
goxel_t *goxel(void);
void goxel_init(goxel_t *goxel);
void goxel_iter(goxel_t *goxel, inputs_t *inputs);
void goxel_render(goxel_t *goxel);
void goxel_render_view(goxel_t *goxel, const vec4_t *rect);
// Called by the gui when the mouse hover a 3D view.
// XXX: change the name since we also call it when the mouse get out of
// the view.
void goxel_mouse_in_view(goxel_t *goxel, const vec2_t *view_size,
                         const inputs_t *inputs, bool inside);
int goxel_unproject(goxel_t *goxel, const vec2_t *view_size,
                    const vec2_t *pos, vec3_t *out, vec3_t *normal);
bool goxel_unproject_on_screen(goxel_t *goxel, const vec2_t *view_size,
                               const vec2_t *pos,
                               vec3_t *out, vec3_t *normal);
bool goxel_unproject_on_mesh(goxel_t *goxel, const vec2_t *view_size,
                     const vec2_t *pos, mesh_t *mesh,
                     vec3_t *out, vec3_t *normal);
bool goxel_unproject_on_plane(goxel_t *goxel, const vec2_t *view_size,
                     const vec2_t *pos, const plane_t *plane,
                     vec3_t *out, vec3_t *normal);
bool goxel_unproject_on_box(goxel_t *goxel, const vec2_t *view_size,
                     const vec2_t *pos, const box_t *box,
                     vec3_t *out, vec3_t *normal, int *face);
void goxel_update_meshes(goxel_t *goxel, bool pick);

void goxel_set_help_text(goxel_t *goxel, const char *msg, ...);
void goxel_undo(goxel_t *goxel);
void goxel_redo(goxel_t *goxel);
void goxel_import_image_plane(goxel_t *goxel, const char *path);
// #############################

void save_to_file(goxel_t *goxel, const char *path);
void load_from_file(goxel_t *goxel, const char *path);


int tool_iter(goxel_t *goxel, int tool, const inputs_t *inputs, int state,
              const vec2_t *view_size, bool inside);
void tool_cancel(goxel_t *goxel, int tool, int state);

// #### Colors functions #######
uvec3b_t hsl_to_rgb(uvec3b_t hsl);
uvec3b_t rgb_to_hsl(uvec3b_t rgb);

// #### Gui ####################

void gui_init(void);
void gui_iter(goxel_t *goxel, const inputs_t *inputs);
void gui_render(void);

// #############################

void wavefront_export(const mesh_t *mesh, const char *path);
void ply_export(const mesh_t *mesh, const char *path);


// #### DICOM files support ####

typedef struct {
    int     instance_number;
    float   slice_location;
    int     samples_per_pixel;
    int     rows;
    int     columns;
    int     bits_allocated;
    int     bits_stored;
    int     high_bit;

    int     data_size;
    char    *path;      // If you set it, you have to remember to free it.
} dicom_t;

void dicom_load(const char *path, dicom_t *dicom,
                char *out_buffer, int buffer_size);
void dicom_import(const char *dir);


// #############################

void qubicle_import(const char *path);
void qubicle_export(const mesh_t *mesh, const char *path);

// #### Profiler ###############

typedef struct profiler_block profiler_block_t;
struct profiler_block
{
    const char          *name;
    profiler_block_t    *next;   // All the blocks are put in a list.
    profiler_block_t    *parent; // The block that called this block.
    int                 depth;   // How many time are we inside this block.
    int                 count;

    // In nanoseconds
    int64_t             tot_time;
    int64_t             self_time;
    int64_t             enter_time;
};
void profiler_start(void);
void profiler_stop(void);
void profiler_tick(void);
profiler_block_t *profiler_get_blocks();

void profiler_enter_(profiler_block_t *block);
void profiler_exit_(profiler_block_t *block);
static inline void profiler_cleanup_(profiler_block_t **p)
{
    profiler_exit_(*p);
}

#ifndef PROFILER
    #define PROFILER DEBUG
#endif
#if PROFILER
    #define PROFILED2(name_) \
        static profiler_block_t block_ = {name_}; \
        profiler_enter_(&block_); \
        profiler_block_t *block_ref_ \
                __attribute__((cleanup(profiler_cleanup_))); \
        block_ref_ = &block_;
#else
    #define PROFILED2(name_)
#endif
#define PROFILED PROFILED2(__func__)

// ##### Assets manager ########################
// All the assets are saved in binary directly in the code, using
// tool/create_assets.py.

const void *assets_get(const char *url, int *size);

// List all the assets in a given asset dir.
// Return the number of assets.
// If f returns not 0, the asset is skipped.
int assets_list(const char *url, void *user,
                int (*f)(int i, const char *path, void *user));

#endif // GOXEL_H
