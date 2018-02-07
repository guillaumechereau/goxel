/* Goxel 3D voxels editor
 *
 * copyright (c) 2016 Guillaume Chereau <guillaume@noctua-software.com>
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

// Procedural rendering inspired by ContextFree.org.
// Need to write a doc about it.

#include "goxel.h"
#include <stdarg.h>

#define NODES \
    X(PROG) \
    X(SHAPE) \
    X(RULE) \
    X(BLOCK) \
    X(CALL) \
    X(ARGS) \
    X(TRANSF) \
    X(OP) \
    X(VALUE) \
    X(ID) \
    X(LOOP) \
    X(TRANSFB) \
    X(IF) \
    X(RETURN) \
    X(SET) \
    X(EXPR) \
    X(FUN_CALL) \

// List of operators and the number of args they support.
#define OPS \
    X(s,    1, 3) \
    X(sx,   1) \
    X(sy,   1) \
    X(sz,   1) \
    X(sn,   0, 1) \
    X(x,    1, 2, 3) \
    X(y,    1, 2) \
    X(z,    1) \
    X(rx,   1) \
    X(ry,   1) \
    X(rz,   1) \
    X(light,1, 2) \
    X(sat,  1, 2) \
    X(hue,  1, 2) \
    X(sub,  0) \
    X(paint,0) \
    X(seed, 1) \
    X(wait, 1) \
    X(life, 1) \
    X(antialiased, 1) \

enum {
#define X(x) NODE_##x,
    NODES
#undef X
};

static const char *TYPES_STR[] = {
#define X(x) [NODE_##x] = #x,
    NODES
#undef X
};

enum {
#define X(id, ...) OP_##id,
    OPS
#undef X
    OP_COUNT
};

typedef struct op_info {
    const char *id;
    int        nb[3];
} op_info_t;

static op_info_t OP_INFOS[] = {
#define X(id, ...) [OP_##id]= {#id, {__VA_ARGS__}},
    OPS
#undef X
};

static const shape_t *SHAPES[] = {
    &shape_cube,
    &shape_sphere,
    &shape_cylinder,
};

typedef struct proc_node node_t;
struct proc_node {
    int         type;
    char        *id;
    float       v;
    int         size;
    node_t      *children, *next, *prev, *parent;
    // Pos in the source code.
    int         line;
};

typedef struct proc_ctx ctx_t;
struct proc_ctx {
    ctx_t       *next, *prev;
    box_t       box;
    int         mode;
    vec4_t      color;
    bool        antialiased;
    uint32_t    seed;
    node_t      *prog;
    float       vars[4];
    int         wait;
    int         life;
    bool        last;   // Mark the end of a frame.
};

// Move a value toward a target.  If v is positive, move the value toward
// range of v.  If v is negative move the value toward 0 of -v.
// For example:
//
// float x = 0;
// move_value(&x, 0.5, 10) // x = 5
// move_value(&x, -1, 10)  // x = 0
static void move_value(float *x, float v, float range)
{
    float dst = v >= 0 ? range : 0;
    v = fabs(v);
    *x = mix(*x, dst, v);
}

static inline float mod(float x, float y)
{
    while (x < 0) x += y;
    return fmod(x, y);
}

static void node_free(node_t *node)
{
    node_t *c, *tmp;
    if (!node) return;
    DL_FOREACH_SAFE(node->children, c, tmp) {
        DL_DELETE(node->children, c);
        node_free(c);
    }
    free(node->id);
    free(node);
}

static void ctxs_free(ctx_t *ctx)
{
    ctx_t *c, *tmp;
    if (!ctx) return;
    DL_FOREACH_SAFE(ctx, c, tmp) {
        DL_DELETE(ctx, c);
        free(c);
    }
}

static int error(gox_proc_t *proc, node_t *node, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    free(proc->error.str);
    vasprintf(&proc->error.str, msg, args);
    va_end(args);
    proc->error.line = node ? node->line : 0;
    proc->state = PROC_DONE;
    return -1;
}

#define TRY(f) ({ int r = f; if (r) return r; r;})

// Just for debugging.
static void visit(node_t *node, int lev)
{
    node_t *child;
    assert(node);
    if (node->id)
        LOG_D("%*s%s: '%s'", lev * 4, "", TYPES_STR[node->type], node->id);
    else if (node->type == NODE_VALUE)
        LOG_D("%*s%s: %f", lev * 4, "", TYPES_STR[node->type], node->v);
    else
        LOG_D("%*s%s", lev * 4, "", TYPES_STR[node->type]);
    DL_FOREACH(node->children, child) {
        visit(child, lev + 1);
    }
}

static void set_seed(float v, uint32_t *seed)
{
    memcpy(seed, &v, 4);
}

#define SIMPLE_RAND_MAX 32767

uint32_t simple_rand(uint32_t *seed) {
    *seed = *seed * 1103515245 + 12345;
    return (uint32_t)(*seed / 65536) % 32768;
}

static float frand(float a, float b, uint32_t *seed)
{
    return a + simple_rand(seed) / (float)SIMPLE_RAND_MAX * (b - a);
}

static float plusmin(float a, float b, uint32_t *seed)
{
    return frand(a - b, a + b, seed);
}

// Evaluate an expression node.
static float evaluate(node_t *node, ctx_t *ctx)
{
    float a, b, c;
    if (node->type == NODE_VALUE) return node->v;
    assert(node->type == NODE_EXPR);

    if (str_equ(node->id, "var")) {
        assert(node->v > 0);
        return ctx->vars[(int)node->v - 1];
    }
    a = evaluate(node->children, ctx);

    if (str_equ(node->id, "int"))
        return round(a);

    b = evaluate(node->children->next, ctx);
    if (str_equ(node->id, "+"))
        return a + b;
    if (str_equ(node->id, "-"))
        return a - b;
    if (str_equ(node->id, "*"))
        return a * b;
    if (str_equ(node->id, "/"))
        return a / b;
    if (str_equ(node->id, "+-"))
        return plusmin(a, b, &ctx->seed);
    if (str_equ(node->id, "=="))
        return (a == b) ? 1 : 0;
    if (str_equ(node->id, "!="))
        return (a != b) ? 1 : 0;
    if (str_equ(node->id, "<"))
        return (a < b) ? 1 : 0;
    if (str_equ(node->id, ">"))
        return (a > b) ? 1 : 0;
    if (str_equ(node->id, "<="))
        return (a <= b) ? 1 : 0;
    if (str_equ(node->id, ">="))
        return (a >= b) ? 1 : 0;
    if (str_equ(node->id, "||"))
        return (a || b) ? 1 : 0;
    if (str_equ(node->id, "&&"))
        return (a && b) ? 1 : 0;

    c = evaluate(node->children->next->next, ctx);
    if (str_equ(node->id, "?:"))
        return a ? b : c;

    assert(false);
    return 0;
}


static node_t *get_rule(node_t *prog, const char *id, ctx_t *ctx)
{
    node_t *node, *c;
    float tot = 0;
    DL_FOREACH(prog->children, node) {
        if (    node->type == NODE_SHAPE &&
                strcmp(node->id, id) == 0) break;
    }
    if (!node) return NULL;
    if (node->children->type == NODE_BLOCK)
        return node->children;
    // Get the total proba.
    DL_FOREACH(node->children, c) {
        assert(c->type == NODE_RULE);
        tot += c->v;
    }
    // Now pick one rule randomly.
    DL_FOREACH(node->children, c) {
        if (frand(0, 1, &ctx->seed) <= c->v / tot)
            return c->children;
        tot -= c->v;
    }
    assert(false);
    return NULL;
}

static void scale_normalize(mat4_t *mat, float v)
{
    float x, y, z, m;
    x = vec3_norm(mat4_mul_vec(*mat, vec4(1, 0, 0, 0)).xyz.v);
    y = vec3_norm(mat4_mul_vec(*mat, vec4(0, 1, 0, 0)).xyz.v);
    z = vec3_norm(mat4_mul_vec(*mat, vec4(0, 0, 1, 0)).xyz.v);
    m = min3(x, y, z);
    if (v) m = v / 2;
    mat4_iscale(mat, m / x, m / y, m / z);
}

static int apply_transf(gox_proc_t *proc, node_t *node, ctx_t *ctx)
{
    node_t *c;
    float v[3] = {0};
    int i, n, op;
    assert(node->type == NODE_TRANSF || node->type == NODE_OP);

    if (node->type == NODE_TRANSF) {
        DL_FOREACH(node->children, c) {
            TRY(apply_transf(proc, c, ctx));
        }
        return 0;
    }

    for (op = 0; op < OP_COUNT; op++) {
        if (strcmp(OP_INFOS[op].id, node->id) == 0)
            break;
    }
    if (op == OP_COUNT) return error(proc, node, "No op '%s'", node->id);
    DL_COUNT(node->children, c, n);
    if (!(  (!n && n == OP_INFOS[op].nb[0]) ||
            ( n && n == OP_INFOS[op].nb[0]) ||
            ( n && n == OP_INFOS[op].nb[1]) ||
            ( n && n == OP_INFOS[op].nb[2])))
        return error(proc, node,
                     "Op '%s' does not accept %d arguments", node->id, n);

    for (i = 0, c = node->children; i < n; i++, c = c->next)
        v[i] = evaluate(c, ctx);

    switch (op) {

    case OP_sx:
        mat4_iscale(&ctx->box.mat, v[0], 1, 1);
        break;
    case OP_sy:
        mat4_iscale(&ctx->box.mat, 1, v[0], 1);
        break;
    case OP_sz:
        mat4_iscale(&ctx->box.mat, 1, 1, v[0]);
        break;
    case OP_s:
        if (n == 1) v[1] = v[2] = v[0];
        mat4_iscale(&ctx->box.mat, v[0], v[1], v[2]);
        break;
    case OP_sn:
        scale_normalize(&ctx->box.mat, v[0]);
        break;
    case OP_x:
        mat4_itranslate(&ctx->box.mat, 2 * v[0], 2 * v[1], 2 * v[2]);
        break;
    case OP_y:
        mat4_itranslate(&ctx->box.mat, 0, 2 * v[0], 2 * v[1]);
        break;
    case OP_z:
        mat4_itranslate(&ctx->box.mat, 0, 0, 2 * v[0]);
        break;
    case OP_rx:
        mat4_irotate(&ctx->box.mat, v[0] / 180 * M_PI, 1, 0, 0);
        break;
    case OP_ry:
        mat4_irotate(&ctx->box.mat, v[0] / 180 * M_PI, 0, 1, 0);
        break;
    case OP_rz:
        mat4_irotate(&ctx->box.mat, v[0] / 180 * M_PI, 0, 0, 1);
        break;
    case OP_hue:
        if (n == 1) ctx->color.x = mod(ctx->color.x + v[0], 360);
        else move_value(&ctx->color.x, v[0], v[1]);
        break;
    case OP_sat:
        if (n == 1) v[1] = 1;
        move_value(&ctx->color.y, v[0], v[1]);
        break;
    case OP_light:
        if (n == 1) v[1] = 1;
        move_value(&ctx->color.z, v[0], v[1]);
        break;
    case OP_sub:
        ctx->mode = MODE_SUB;
        break;
    case OP_paint:
        ctx->mode = MODE_PAINT;
        break;
    case OP_seed:
        set_seed(v[0], &ctx->seed);
        break;
    case OP_wait:
        ctx->wait += v[0];
        break;
    case OP_life:
        ctx->life += v[0];
        break;
    case OP_antialiased:
        ctx->antialiased = (bool)v[0];
        break;
    }
    return 0;
}

static int set_args(gox_proc_t *proc, node_t *node, ctx_t *ctx)
{
    assert(node->type == NODE_ARGS);
    node_t *c;
    int i = 0;
    DL_FOREACH(node->children, c) {
        ctx->vars[i] = evaluate(c, ctx);
        i++;
    }
    return 0;
}

static void call_shape(const ctx_t *ctx, const shape_t *shape)
{
    mesh_t *mesh = goxel->image->active_layer->mesh;
    uint8_t hsl[3] = {ctx->color.x / 360 * 255,
                      ctx->color.y * 255,
                      ctx->color.z * 255};
    hsl_to_rgb(hsl, goxel->painter.color);
    goxel->painter.shape = shape;
    goxel->painter.mode = ctx->mode;
    goxel->painter.smoothness = ctx->antialiased ? 1 : 0;
    mesh_op(mesh, &goxel->painter, &ctx->box);
}

// Iter the program once.
static int iter(gox_proc_t *proc, ctx_t *ctx)
{
    const float max_op_volume = 512 * 512 * 512;
    float v;
    int n, i;
    float volume, volume_tot = 0;
    ctx_t ctx2, *new_ctx;
    node_t *expr, *rule;

    if (ctx->life > 0) {
        ctx->life--;
        if (ctx->life <= 0)
            return 0;
    }
    if (ctx->wait > 0) {
        new_ctx = calloc(1, sizeof(*new_ctx));
        *new_ctx = *ctx;
        new_ctx->wait--;
        DL_APPEND(proc->ctxs, new_ctx);
        return 0;
    }

    // XXX: find a better stopping condition.
    if (vec3_norm2(ctx->box.w.v) < 0.2 ||
        vec3_norm2(ctx->box.h.v) < 0.2 ||
        vec3_norm2(ctx->box.d.v) < 0.2) goto end;

    DL_FOREACH(ctx->prog->children, expr) {
        if (expr->type == NODE_LOOP) {
            // loop n tranf block
            n = evaluate(expr->children, ctx);
            ctx2 = *ctx;
            ctx2.prog = expr->children->next->next;
            for (i = 0; i < n; i++) {
                simple_rand(&ctx2.seed);
                new_ctx = calloc(1, sizeof(*new_ctx));
                *new_ctx = ctx2;
                if (expr->v) {
                    new_ctx->vars[(int)expr->v - 1] = i;
                }
                DL_APPEND(proc->ctxs, new_ctx);
                TRY(apply_transf(proc, expr->children->next, &ctx2));
            }
        }
        if (expr->type == NODE_TRANSFB) {
            new_ctx = calloc(1, sizeof(*new_ctx));
            *new_ctx = *ctx;
            new_ctx->prog = expr->children->next;
            TRY(apply_transf(proc, expr->children, new_ctx));
            DL_APPEND(proc->ctxs, new_ctx);
        }
        if (expr->type == NODE_IF) {
            v = evaluate(expr->children, ctx);
            if (v) {
                ctx2 = *ctx;
                ctx2.prog = expr->children->next;
                iter(proc, &ctx2);
            }
        }
        if (expr->type == NODE_SET) {
            ctx->vars[(int)expr->v - 1] = evaluate(expr->children, ctx);
        }
        if (expr->type == NODE_RETURN) {
            break;
        }
        if (expr->type == NODE_TRANSF) {
            TRY(apply_transf(proc, expr, ctx));
        }
        if (expr->type == NODE_CALL) {
            simple_rand(&ctx->seed);
            ctx2 = *ctx;
            TRY(apply_transf(proc, expr->children, &ctx2));
            // Is it a basic shape?
            for (i = 0; i < ARRAY_SIZE(SHAPES); i++) {
                if (str_equ(expr->id, SHAPES[i]->id)) {
                    volume = box_get_volume(ctx2.box);
                    if (volume > max_op_volume)
                        return error(proc, expr, "abort: volume too big!");
                    volume_tot += volume;
                    call_shape(&ctx2, SHAPES[i]);
                    break;
                }
            }
            if (i < ARRAY_SIZE(SHAPES)) continue;

            TRY(set_args(proc, expr->children->next, &ctx2));
            rule = get_rule(proc->prog, expr->id, &ctx2);
            if (!rule)
                return error(proc, NULL, "Cannot find rule %s", expr->id);
            ctx2.prog = rule;
            new_ctx = calloc(1, sizeof(*new_ctx));
            *new_ctx = ctx2;

            DL_APPEND(proc->ctxs, new_ctx);
        }
    }
end:
    return 0;
}

// Defined in procedural.leg
static node_t *parse(const char *txt, int *err_line);

int proc_parse(const char *txt, gox_proc_t *proc)
{
    proc_release(proc);
    proc->prog = parse(txt, &proc->error.line);
    if (!proc->prog) {
        proc->state = PROC_PARSE_ERROR;
        free(proc->error.str);
        asprintf(&proc->error.str, "Parse error");
        return -1;
    }
    proc->state = PROC_READY;
    if ((0)) visit(proc->prog, 0);
    return 0;
}

void proc_release(gox_proc_t *proc)
{
    node_free(proc->prog);
    proc->prog = NULL;
    ctxs_free(proc->ctxs);
    proc->ctxs = NULL;
    free(proc->error.str);
    proc->error.str = NULL;
    proc->error.line = 0;
}

int proc_start(gox_proc_t *proc, const box_t *box)
{
    // Reinit the context to a single ctx_t pointing at the main shape.
    ctx_t *ctx;
    assert(proc->state >= PROC_READY);
    ctxs_free(proc->ctxs);
    proc->ctxs = NULL;
    proc->frame = 0;
    ctx = calloc(1, sizeof(*ctx));
    ctx->box = box ? *box : bbox_from_extents(vec3_zero, 0.5, 0.5, 0.5);
    ctx->color = vec4(0, 0, 1, 1);
    ctx->mode = MODE_OVER;
    ctx->prog = get_rule(proc->prog, "main", ctx);
    set_seed(rand(), &ctx->seed);
    DL_APPEND(proc->ctxs, ctx);
    if (!ctx->prog) return error(proc, NULL, "No 'main' shape");
    proc->state = PROC_RUNNING;
    return 0;
}

int proc_stop(gox_proc_t *proc)
{
    proc->state = PROC_DONE;
    return 0;
}

int proc_iter(gox_proc_t *proc)
{
    int r;
    bool last;
    ctx_t *ctx;

    if (proc->state != PROC_RUNNING) return 0;

    if (!proc->ctxs) {
        proc->state = PROC_DONE;
        return 0;
    }

    // Mark the last context of this frame.
    // XXX: This is for animations, but I think I should change the way it
    // works anyway: we should only animate when 'wait' is used in the code,
    // even for recursive shapes.
    if (!proc->in_frame)
        proc->ctxs->prev->last = true;

    proc->in_frame = false;

    while (true) {
        ctx = proc->ctxs;
        if (!ctx) {
            proc->state = 2;
            break;
        }
        DL_DELETE(proc->ctxs, ctx);
        last = ctx->last;
        ctx->last = false;
        r = iter(proc, ctx);
        free(ctx);
        if (r != 0) {
            proc->state = PROC_DONE;
            break;
        }
        if (last) break;
        if (sys_get_time() - goxel->frame_time > 16 / 1000.) {
            proc->in_frame = true;
            return 0;
        }
    }
    proc->frame++;
    return 0;
}

static int list_saved_on_path(int i, const char *path, void *user_)
{
   const char *data, *name;
   void (*f)(int, const char*, const char*, void*) = USER_GET(user_, 0);
   void *user = USER_GET(user_, 1);
   if (!str_endswith(path, ".goxcf")) return -1;
   if (f) {
       data = assets_get(path, NULL);
       name = strrchr(path, '/') + 1;
       f(i, name, data, user);
   }
   return 0;
}

// List all the programs in data/progs.  Not sure if this works on
// windows.
int proc_list_examples(void (*f)(int index,
                                 const char *name, const char *code,
                                 void *user), void *user)
{
    return assets_list("data/progs", USER_PASS(f, user), list_saved_on_path);
}

// The actual parser code come here, generated from procedural.leg

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "procedural.inl"
