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
    X(TRANSF) \
    X(OP) \
    X(VALUE) \
    X(ID) \
    X(LOOP) \
    X(EXPR) \

// List of operators and the number of args they support.
#define OPS \
    X(s,    1, 3) \
    X(sx,   1) \
    X(sy,   1) \
    X(sz,   1) \
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

typedef struct proc_node node_t;
struct proc_node {
    int         type;
    const char  *id;
    double      v;
    int         size;
    node_t      *children, *next, *prev, *parent;
    // Pos in the source code.
    int         line;
};

typedef struct proc_ctx ctx_t;
struct proc_ctx {
    ctx_t       *next, *prev;
    box_t       box;
    int         op;
    vec4_t      color;
    node_t      *prog;
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

static int error(proc_t *proc, node_t *node, const char *msg, ...)
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

// XXX: I should use my own rand implementation, so that the rendering
// stays the same on all the machines.
static float frand(float min, float max)
{
    return min + (rand() / (float)RAND_MAX) * (max - min);
}

static float plusmin(float a, float b)
{
    return frand(a - b, a + b);
}

// Evaluate an expression node.
static float evaluate(node_t *node)
{
    float a, b;
    if (node->type == NODE_VALUE) return node->v;
    assert(node->type == NODE_EXPR);
    if (strcmp(node->id, "+-") == 0) {
        a = evaluate(node->children);
        b = evaluate(node->children->next);
        return plusmin(a, b);
    }
    assert(false);
    return 0;
}


static node_t *get_rule(node_t *prog, const char *id)
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
        if (frand(0, 1) <= c->v / tot)
            return c->children;
        tot -= c->v;
    }
    assert(false);
    return NULL;
}

static int apply_transf(proc_t *proc, node_t *node, ctx_t *ctx)
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
        v[i] = evaluate(c);

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
    case OP_x:
        mat4_itranslate(&ctx->box.mat, 2 * v[0], 2 * v[1], 2 * v[2]);
        break;
    case OP_y:
        mat4_itranslate(&ctx->box.mat, 0, 2 * v[0], 2 * v[1]);
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
        ctx->op = OP_SUB;
        break;
    }
    return 0;
}

static void call_shape(const ctx_t *ctx, shape_t *shape)
{
    mesh_t *mesh = goxel()->image->active_layer->mesh;
    uvec3b_t hsl = uvec3b(ctx->color.x / 360 * 255,
                          ctx->color.y * 255,
                          ctx->color.z * 255);
    goxel()->painter.color.rgb = hsl_to_rgb(hsl);
    goxel()->painter.shape = shape;
    goxel()->painter.op = ctx->op;
    mesh_op(mesh, &goxel()->painter, &ctx->box);
}

static int iter(proc_t *proc, ctx_t *ctx)
{
    int n, i, nb_ops = 0;
    ctx_t ctx2, *new_ctx;
    node_t *expr, *rule;

    // XXX: find a better stopping condition.
    if (vec3_norm2(ctx->box.w) < 0.2 ||
        vec3_norm2(ctx->box.h) < 0.2 ||
        vec3_norm2(ctx->box.d) < 0.2) goto end;

    DL_FOREACH(ctx->prog->children, expr) {
        if (expr->type == NODE_LOOP) {
            n = expr->v;
            ctx2 = *ctx;
            ctx2.prog = expr->children->next;
            for (i = 0; i < n; i++) {
                new_ctx = calloc(1, sizeof(*new_ctx));
                *new_ctx = ctx2;
                DL_APPEND(proc->ctxs, new_ctx);
                TRY(apply_transf(proc, expr->children, &ctx2));
            }
        }
        if (expr->type == NODE_TRANSF) {
            TRY(apply_transf(proc, expr, ctx));
        }
        if (expr->type == NODE_CALL) {
            ctx2 = *ctx;
            TRY(apply_transf(proc, expr->children, &ctx2));
            nb_ops++;
            if (strcmp(expr->id, "cube") == 0)
                call_shape(&ctx2, &shape_cube);
            else if (strcmp(expr->id, "cylinder") == 0)
                call_shape(&ctx2, &shape_cylinder);
            else if (strcmp(expr->id, "sphere") == 0)
                call_shape(&ctx2, &shape_sphere);
            else {
                nb_ops--;
                rule = get_rule(proc->prog, expr->id);
                if (!rule)
                    return error(proc, NULL, "Cannot find rule %s", expr->id);
                ctx2.prog = rule;
                new_ctx = calloc(1, sizeof(*new_ctx));
                *new_ctx = ctx2;
                DL_APPEND(proc->ctxs, new_ctx);
            }
        }
    }
end:
    return nb_ops;
}

// Defined in procedural.leg
static node_t *parse(const char *txt, int *err_line);

int proc_parse(const char *txt, proc_t *proc)
{
    node_free(proc->prog);
    ctxs_free(proc->ctxs);
    proc->ctxs = NULL;
    free(proc->error.str);
    proc->error.str = NULL;
    proc->error.line = 0;

    proc->prog = parse(txt, &proc->error.line);
    if (!proc->prog) {
        LOG_E("Parse error");
        proc->state = PROC_PARSE_ERROR;
        free(proc->error.str);
        asprintf(&proc->error.str, "Parse error");
        return -1;
    }
    proc->state = PROC_READY;
    visit(proc->prog, 0);
    return 0;
}

int proc_start(proc_t *proc)
{
    // Reinit the context to a single ctx_t pointing at the main shape.
    ctx_t *ctx;
    assert(proc->state >= PROC_READY);
    ctxs_free(proc->ctxs);
    proc->ctxs = NULL;
    ctx = calloc(1, sizeof(*ctx));
    ctx->box = bbox_from_extents(vec3_zero, 0.5, 0.5, 0.5);
    ctx->color = vec4(0, 0, 0, 1);
    ctx->op = OP_ADD;
    ctx->prog = get_rule(proc->prog, "main");
    DL_APPEND(proc->ctxs, ctx);
    if (!ctx->prog) return error(proc, NULL, "No 'main' shape");
    proc->state = PROC_RUNNING;
    return 0;
}

int proc_stop(proc_t *proc)
{
    proc->state = PROC_DONE;
    return 0;
}

// For the moment only max_op allows to limit the number of drawing operations
// being done, so that we don't block the machine too long.  This is not
// a good solution, instead I should look at the actual time spent in the
// loop.
int proc_iter(proc_t *proc, int max_op)
{
    int n, nb_ops = 0;
    ctx_t *ctx, *last_ctx;

    if (proc->state != PROC_RUNNING) return 0;

    if (!proc->ctxs) {
        proc->state = PROC_DONE;
        return 0;
    }

    last_ctx = proc->ctxs->prev;
    while (true) {
        ctx = proc->ctxs;
        if (!ctx) {
            proc->state = 2;
            break;
        }
        DL_DELETE(proc->ctxs, ctx);
        n = iter(proc, ctx);
        nb_ops += n;
        free(ctx);
        if (n == -1) {
            proc->state = PROC_DONE;
            break;
        }
        if (nb_ops >= max_op) break;
        if (ctx == last_ctx) break;
    }
    return 0;
}

// List all the programs in data/progs.  Not sure if this works on
// windows.
int proc_list_saved(void *user, void (*f)(int index,
                                const char *name, const char *code,
                                void *user))
{
    int nb = 0;
    int callback(const char *path, void *user) {
        char *data, *name;
        if (!str_endswith(path, ".goxcf")) return 0;
        if (f) {
            data = read_file(path, NULL, false);
            name = strrchr(path, '/') + 1;
            f(nb, name, data, user);
            free(data);
        }
        nb++;
        return 0;
    }
    list_dir("data/progs", 0, user, callback);
    return nb;
}

// The actual parser code come here, generated from procedural.leg

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "procedural.inc"
