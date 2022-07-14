#include "goxel.h"


typedef struct {
    tool_t tool;
} tool_pick_color_t;


int tool_color_picker_iter(tool_t *tool, const painter_t *painter,
                           const float viewport[4])
{
    uint8_t color[4];
    const mesh_t *mesh = goxel_get_layers_mesh(goxel.image);
    cursor_t *curs = &goxel.cursor;
    int pi[3] = {floor(curs->pos[0]),
                 floor(curs->pos[1]),
                 floor(curs->pos[2])};
    curs->snap_mask = SNAP_MESH;
    curs->snap_offset = -0.5;

    goxel_set_help_text("Click on a voxel to pick the color");
    if (!curs->snaped) return 0;
    mesh_get_at(mesh, NULL, pi, color);
    color[3] = 255;
    goxel_set_help_text("%d %d %d", color[0], color[1], color[2]);
    if (curs->flags & CURSOR_PRESSED) vec4_copy(color, goxel.painter.color);
    return 0;
}

static int gui(tool_t *tool)
{
    tool_gui_color();
    return 0;
}

TOOL_REGISTER(TOOL_PICK_COLOR, pick_color, tool_pick_color_t,
             .name = "Color Picker",
             .iter_fn = tool_color_picker_iter,
             .gui_fn = gui,
             .default_shortcut = "C",
)
