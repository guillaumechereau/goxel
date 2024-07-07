/* Goxel 3D voxels editor
 *
 * copyright (c) 2022 Guillaume Chereau <guillaume@noctua-software.com>
 * All right reserved.
 */

#include "goxel.h"

typedef struct {
    tool_t tool;
    int mode; // MODE_REPLACE, MODE_OVER, MODE_SUB
    float rect[4];
} tool_rect_select_t;

static void apply(const float rect_[4], int mode)
{
    int vp[3];
    float p[4], rect[4];
    uint8_t value[4];
    uint8_t color[4] = {255, 255, 255, 255};
    float view_proj_mat[4][4];
    image_t *img = goxel.image;
    volume_t *volume = img->active_layer->volume;
    volume_iterator_t iter;
    const camera_t *cam = img->active_camera;

    rect[0] = min(rect_[0], rect_[2]);
    rect[1] = min(rect_[1], rect_[3]);
    rect[2] = max(rect_[0], rect_[2]);
    rect[3] = max(rect_[1], rect_[3]);

    mat4_mul(cam->proj_mat, cam->view_mat, view_proj_mat);
    if (img->selection_mask == NULL) img->selection_mask = volume_new();
    if (mode == MODE_SUB)
        memset(color, 0, sizeof(color));

    if (mode == MODE_REPLACE)
        volume_clear(img->selection_mask);

    // XXX: very slow implementation!

    iter = volume_get_iterator(volume,
            VOLUME_ITER_VOXELS | VOLUME_ITER_SKIP_EMPTY);
    while (volume_iter(&iter, vp)) {
        volume_get_at(volume, &iter, vp, value);
        if (value[3] == 0) continue;
        vec4_set(p, vp[0] + 0.5, vp[1] + 0.5, vp[2] + 0.5, 1.0);
        mat4_mul_vec4(view_proj_mat, p, p);
        vec3_mul(p, 1 / p[3], p);
        if (    p[0] < rect[0] || p[0] > rect[2] ||
                p[1] < rect[1] || p[1] > rect[3])
            continue;
        volume_set_at(img->selection_mask, NULL, vp, color);
    }
}

static int on_drag(gesture3d_t *gest)
{
    tool_rect_select_t *tool = gest->user;
    float pos[4];
    const camera_t *cam = goxel.image->active_camera;
    int mode = tool->mode;

    vec4_set(pos, gest->pos[0], gest->pos[1], gest->pos[2], 1.0);
    mat4_mul_vec4(cam->view_mat, pos, pos);
    mat4_mul_vec4(cam->proj_mat, pos, pos);
    vec3_mul(pos, 1 / pos[3], pos);

    if (gest->state == GESTURE3D_STATE_BEGIN)
        vec2_copy(pos, tool->rect);
    vec2_copy(pos, &tool->rect[2]);

    if (gest->state == GESTURE3D_STATE_END) {
        if (gest->flags & GESTURE3D_FLAG_SHIFT)
            mode = MODE_OVER;
        else if (gest->flags & GESTURE3D_FLAG_CTRL)
            mode = MODE_SUB;
        apply(tool->rect, mode);
        vec4_set(tool->rect, 0, 0, 0, 0);
    }

    return 0;
}

static void init(tool_t *tool_)
{
    tool_rect_select_t *tool = (void*)tool_;
    tool->mode = MODE_REPLACE;
}

static int iter(tool_t *tool_, const painter_t *painter,
                const float viewport[4])
{
    float plane[4][4], w, h, center[2];
    tool_rect_select_t *tool = (tool_rect_select_t*)tool_;

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE3D_TYPE_DRAG,
        .snap_mask = SNAP_CAMERA,
        .callback = on_drag,
        .user = tool,
    });

    if (    tool->rect[0] == 0 && tool->rect[1] == 0 &&
            tool->rect[2] == 0 && tool->rect[3] == 0) {
        return tool->tool.state;
    }

    mat4_set_identity(plane);
    w = (tool->rect[2] - tool->rect[0]) / 2 * viewport[2];
    h = (tool->rect[3] - tool->rect[1]) / 2 * viewport[3];
    center[0] = ((tool->rect[2] + tool->rect[0]) / 2 + 1) / 2;
    center[1] = ((tool->rect[3] + tool->rect[1]) / 2 + 1) / 2;
    mat4_itranslate(plane, center[0] * viewport[2], center[1] * viewport[3], 0);
    mat4_iscale(plane, w, h, 0);

    render_rect(&goxel.rend, plane, EFFECT_STRIP);

    return tool->tool.state;
}

static int gui(tool_t *tool_)
{
    tool_rect_select_t *tool = (void*)tool_;
    tool_gui_mask_mode(&tool->mode);
    return 0;
}

TOOL_REGISTER(TOOL_RECT_SELECT, rect_select, tool_rect_select_t,
              .name = STR_RECT_SELECT,
              .init_fn = init,
              .iter_fn = iter,
              .gui_fn = gui,
              .flags = TOOL_REQUIRE_CAN_EDIT | TOOL_SHOW_MASK,
)
