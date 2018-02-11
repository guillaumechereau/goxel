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

extern "C" {
#include "goxel.h"
}

#define IM_VEC4_CLASS_EXTRA \
        ImVec4(const uint8_t f[4]) { \
            x = f[0] / 255.; \
            y = f[1] / 255.; \
            z = f[2] / 255.; \
            w = f[3] / 255.; }     \

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "imgui.h"
#include "imgui_internal.h"

extern "C" {
    bool gui_settings_popup(void);
}

static inline ImVec4 color_lighten(ImVec4 c, float k)
{
    c.x *= k;
    c.y *= k;
    c.z *= k;
    return c;
}

namespace ImGui {
    bool GoxInputFloat(const char *label, float *v, float step = 0.1,
                       float minv = -FLT_MAX, float maxv = FLT_MAX,
                       const char *format = "%.1f");

    void GoxBox(ImVec2 pos, ImVec2 size, bool selected,
                int rounding_corners_flags = ~0);
    void GoxBox2(ImVec2 pos, ImVec2 size, ImVec4 color, bool fill,
                 int rounding_corners_flags = ~0);
};

static texture_t *g_tex_icons = NULL;

static const char *VSHADER =
    "                                                               \n"
    "attribute vec3 a_pos;                                          \n"
    "attribute vec2 a_tex_pos;                                      \n"
    "attribute vec4 a_color;                                        \n"
    "                                                               \n"
    "uniform mat4 u_proj_mat;                                       \n"
    "                                                               \n"
    "varying vec2 v_tex_pos;                                        \n"
    "varying vec4 v_color;                                          \n"
    "                                                               \n"
    "void main()                                                    \n"
    "{                                                              \n"
    "    gl_Position = u_proj_mat * vec4(a_pos, 1.0);               \n"
    "    v_tex_pos = a_tex_pos;                                     \n"
    "    v_color = a_color;                                         \n"
    "}                                                              \n"
;

static const char *FSHADER =
    "                                                               \n"
    "#ifdef GL_ES                                                   \n"
    "precision mediump float;                                       \n"
    "#endif                                                         \n"
    "                                                               \n"
    "uniform sampler2D u_tex;                                       \n"
    "                                                               \n"
    "varying vec2 v_tex_pos;                                        \n"
    "varying vec4 v_color;                                          \n"
    "                                                               \n"
    "void main()                                                    \n"
    "{                                                              \n"
    "    gl_FragColor = v_color * texture2D(u_tex, v_tex_pos);      \n"
    "}                                                              \n"
;

typedef struct {
    float  rect[4];
} view_t;

typedef struct {
    GLuint prog;

    GLuint a_pos_l;
    GLuint a_tex_pos_l;
    GLuint a_color_l;

    GLuint u_tex_l;
    GLuint u_proj_mat_l;
} prog_t;

typedef struct gui_t {
    prog_t  prog;
    GLuint  array_buffer;
    GLuint  index_buffer;

    int     current_panel;
    view_t  view;
    int     min_panel_size;
    struct {
        gesture_t drag;
        gesture_t hover;
    }       gestures;
    bool    mouse_in_view;
    bool    capture_mouse;
    int     group;

    struct {
        const char *title;
        bool      (*func)(void);
        int         flags;
    } popup;

    // Textures for the color editor.
    texture_t *hsl_tex;
    texture_t *hue_tex;
} gui_t;

static gui_t *gui = NULL;

// Notify the gui that we want the panel size to be at least as large as
// the last item.
static void auto_adjust_panel_size(void) {
    float w = ImGui::GetItemRectMax().x;
    ImGuiStyle& style = ImGui::GetStyle();
    if (ImGui::GetScrollMaxY() > 0) w += style.ScrollbarSize;
    gui->min_panel_size = max(gui->min_panel_size, w);
}

static void on_click(void) {
    if (DEFINED(GUI_SOUND))
        sound_play("click");
}

static bool isCharPressed(int c)
{
    ImGuiContext& g = *GImGui;
    return g.IO.InputCharacters[0] == c;
}

#define COLOR(g, c, s) ({ \
        uint8_t c_[4]; \
        theme_get_color(THEME_GROUP_##g, THEME_COLOR_##c, (s), c_); \
        ImVec4 ret_ = c_; ret_; })

static void init_prog(prog_t *p)
{
    p->prog = create_program(VSHADER, FSHADER, NULL);
    GL(glUseProgram(p->prog));
#define UNIFORM(x) p->x##_l = glGetUniformLocation(p->prog, #x);
#define ATTRIB(x) p->x##_l = glGetAttribLocation(p->prog, #x)
    UNIFORM(u_proj_mat);
    UNIFORM(u_tex);
    ATTRIB(a_pos);
    ATTRIB(a_tex_pos);
    ATTRIB(a_color);
#undef ATTRIB
#undef UNIFORM
    GL(glUniform1i(p->u_tex_l, 0));
}


static void render_prepare_context(void)
{
    #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
    GL(glEnable(GL_BLEND));
    GL(glBlendEquation(GL_FUNC_ADD));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL(glDisable(GL_CULL_FACE));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glEnable(GL_SCISSOR_TEST));
    GL(glActiveTexture(GL_TEXTURE0));

    // Setup orthographic projection matrix
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    const float ortho_projection[4][4] =
    {
        { 2.0f/width,	0.0f,			0.0f,		0.0f },
        { 0.0f,			2.0f/-height,	0.0f,		0.0f },
        { 0.0f,			0.0f,			-1.0f,		0.0f },
        { -1.0f,		1.0f,			0.0f,		1.0f },
    };
    GL(glUseProgram(gui->prog.prog));
    GL(glUniformMatrix4fv(gui->prog.u_proj_mat_l, 1, 0, &ortho_projection[0][0]));

    GL(glBindBuffer(GL_ARRAY_BUFFER, gui->array_buffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gui->index_buffer));
    // This could probably be done only at init time.
    GL(glEnableVertexAttribArray(gui->prog.a_pos_l));
    GL(glEnableVertexAttribArray(gui->prog.a_tex_pos_l));
    GL(glEnableVertexAttribArray(gui->prog.a_color_l));
    GL(glVertexAttribPointer(gui->prog.a_pos_l, 2, GL_FLOAT, false,
                             sizeof(ImDrawVert),
                             (void*)OFFSETOF(ImDrawVert, pos)));
    GL(glVertexAttribPointer(gui->prog.a_tex_pos_l, 2, GL_FLOAT, false,
                             sizeof(ImDrawVert),
                             (void*)OFFSETOF(ImDrawVert, uv)));
    GL(glVertexAttribPointer(gui->prog.a_color_l, 4, GL_UNSIGNED_BYTE,
                             true, sizeof(ImDrawVert),
                             (void*)OFFSETOF(ImDrawVert, col)));
    #undef OFFSETOF
}

static void ImImpl_RenderDrawLists(ImDrawData* draw_data)
{
    const float height = ImGui::GetIO().DisplaySize.y;
    const float scale = ImGui::GetIO().DisplayFramebufferScale.y;
    render_prepare_context();
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;
        if (cmd_list->VtxBuffer.size())
            GL(glBufferData(GL_ARRAY_BUFFER,
                    (GLsizeiptr)cmd_list->VtxBuffer.size() * sizeof(ImDrawVert),
                    (GLvoid*)&cmd_list->VtxBuffer.front(), GL_DYNAMIC_DRAW));

        if (cmd_list->IdxBuffer.size())
            GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    (GLsizeiptr)cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx),
                    (GLvoid*)&cmd_list->IdxBuffer.front(), GL_DYNAMIC_DRAW));

        for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++)
        {
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
                render_prepare_context(); // Restore context.
            }
            else
            {
                GL(glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId));
                GL(glScissor((int)pcmd->ClipRect.x * scale,
                             (int)(height - pcmd->ClipRect.w) * scale,
                             (int)(pcmd->ClipRect.z - pcmd->ClipRect.x) * scale,
                             (int)(pcmd->ClipRect.w - pcmd->ClipRect.y) * scale));
                GL(glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount,
                                  GL_UNSIGNED_SHORT, idx_buffer_offset));
            }
            idx_buffer_offset += pcmd->ElemCount;
        }
    }
    GL(glDisable(GL_SCISSOR_TEST));
}

static void load_fonts_texture()
{
    ImGuiIO& io = ImGui::GetIO();

    float scale = io.DisplayFramebufferScale.y;
    unsigned char* pixels;
    int width, height;
    const void *data;
    int data_size;
    ImFontConfig conf;

    const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x25A0, 0x25FF, // Geometric shapes
        0
    };
    conf.FontDataOwnedByAtlas = false;

    data = assets_get("asset://data/fonts/DejaVuSans-light.ttf", &data_size);
    assert(data);
    io.Fonts->AddFontFromMemoryTTF((void*)data, data_size, 14 * scale,
                                   &conf, ranges);
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    io.Fonts->TexID = (void *)(intptr_t)tex_id;
}

static void init_ImGui(const inputs_t *inputs)
{
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f/60.0f;

    io.KeyMap[ImGuiKey_Tab]         = KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow]   = KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow]  = KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow]     = KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow]   = KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp]      = KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown]    = KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home]        = KEY_HOME;
    io.KeyMap[ImGuiKey_End]         = KEY_END;
    io.KeyMap[ImGuiKey_Delete]      = KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace]   = KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter]       = KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape]      = KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A]           = 'A';
    io.KeyMap[ImGuiKey_C]           = 'C';
    io.KeyMap[ImGuiKey_V]           = 'V';
    io.KeyMap[ImGuiKey_X]           = 'X';
    io.KeyMap[ImGuiKey_Y]           = 'Y';
    io.KeyMap[ImGuiKey_Z]           = 'Z';
    io.DisplayFramebufferScale = ImVec2(inputs->scale, inputs->scale);
    io.FontGlobalScale = 1 / inputs->scale;

    if (DEFINED(__linux__)) {
        io.SetClipboardTextFn = sys_set_clipboard_text;
        io.GetClipboardTextFn = sys_get_clipboard_text;
    }

    io.RenderDrawListsFn = ImImpl_RenderDrawLists;
    load_fonts_texture();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.WindowPadding = ImVec2(4, 4);
}

// Create an Sat/Hue bitmap with all the value for a given hue.
static void hsl_bitmap(int hue, uint8_t *buffer, int w, int h)
{
    int x, y;
    uint8_t hsl[3], rgb[3];
    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
        hsl[0] = hue;
        hsl[1] = 256 * (h - y - 1) / h;
        hsl[2] = 256 * x / w;
        hsl_to_rgb(hsl, rgb);
        memcpy(&buffer[(y * w + x) * 3], rgb, 3);
    }
}

static void hue_bitmap(uint8_t *buffer, int w, int h)
{
    int x, y;
    uint8_t rgb[3], hsl[3] = {0, 255, 127};
    for (y = 0; y < h; y++) {
        hsl[0] = 256 * (h - y - 1) / h;
        hsl_to_rgb(hsl, rgb);
        for (x = 0; x < w; x++) {
            memcpy(&buffer[(y * w + x) * 3], rgb, 3);
        }
    }
}

static int create_hsl_texture(int hue) {
    uint8_t *buffer;
    if (!gui->hsl_tex) {
        gui->hsl_tex = texture_new_surface(256, 256, TF_RGB);
    }
    buffer = (uint8_t*)malloc(256 * 256 * 3); 
    hsl_bitmap(hue, buffer, 256, 256);

    glBindTexture(GL_TEXTURE_2D, gui->hsl_tex->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0,
            GL_RGB, GL_UNSIGNED_BYTE, buffer);

    free(buffer);
    return gui->hsl_tex->tex;
}

static int create_hue_texture(void)
{
    uint8_t *buffer;
    if (!gui->hue_tex) {
        gui->hue_tex = texture_new_surface(32, 256, TF_RGB);
        buffer = (uint8_t*)malloc(32 * 256 * 3);
        hue_bitmap(buffer, 32, 256);
        glBindTexture(GL_TEXTURE_2D, gui->hue_tex->tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 256, 0,
                GL_RGB, GL_UNSIGNED_BYTE, buffer);
        free(buffer);
    }
    return gui->hue_tex->tex;
}

static bool color_edit(const char *name, uint8_t color[4]) {
    bool ret = false;
    ImVec2 c_pos;
    ImGuiIO& io = ImGui::GetIO();
    uint8_t hsl[4];
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::Text("Edit color");
    ImVec4 c = color;
    rgb_to_hsl(color, hsl);
    hsl[3] = color[3];

    c_pos = ImGui::GetCursorScreenPos();
    int tex_size = 256;
    ImGui::Image((void*)(uintptr_t)create_hsl_texture(hsl[0]),
            ImVec2(tex_size, tex_size));
    // Draw lines.
    draw_list->AddLine(c_pos + ImVec2(hsl[2] * tex_size / 255, 0),
            c_pos + ImVec2(hsl[2] * tex_size / 255, 256),
            0xFFFFFFFF, 2.0f);
    draw_list->AddLine(c_pos + ImVec2(0,   256 - hsl[1] * tex_size / 255),
            c_pos + ImVec2(256, 256 - hsl[1] * tex_size / 255),
            0xFFFFFFFF, 2.0f);

    draw_list->AddLine(c_pos + ImVec2(hsl[2] * tex_size / 255, 0),
            c_pos + ImVec2(hsl[2] * tex_size / 255, 256),
            0xFF000000, 1.0f);
    draw_list->AddLine(c_pos + ImVec2(0,   256 - hsl[1] * tex_size / 255),
            c_pos + ImVec2(256, 256 - hsl[1] * tex_size / 255),
            0xFF000000, 1.0f);

    if (ImGui::IsItemHovered() && io.MouseDown[0]) {
        ImVec2 pos = ImVec2(ImGui::GetIO().MousePos.x - c_pos.x,
                ImGui::GetIO().MousePos.y - c_pos.y);
        hsl[1] = 255 - pos.y;
        hsl[2] = pos.x;
        hsl_to_rgb(hsl, color);
        c = color;
        ret = true;
    }
    ImGui::SameLine();
    c_pos = ImGui::GetCursorScreenPos();
    ImGui::Image((void*)(uintptr_t)create_hue_texture(), ImVec2(32, 256));
    draw_list->AddLine(c_pos + ImVec2( 0, 255 - hsl[0] * 256 / 255),
            c_pos + ImVec2(31, 255 - hsl[0] * 256 / 255),
            0xFFFFFFFF, 2.0f);

    if (ImGui::IsItemHovered() && io.MouseDown[0]) {
        ImVec2 pos = ImVec2(ImGui::GetIO().MousePos.x - c_pos.x,
                ImGui::GetIO().MousePos.y - c_pos.y);
        hsl[0] = 255 - pos.y;
        hsl_to_rgb(hsl, color);
        c = color;
        ret = true;
    }

    ret = ImGui::ColorEdit3(name, (float*)&c) || ret;
    if (ret) {
        color[0] = c.x * 255;
        color[1] = c.y * 255;
        color[2] = c.z * 255;
        color[3] = c.w * 255;
    }
    return ret;
}



static int on_gesture(const gesture_t *gest, void *user)
{
    gui_t *gui = (gui_t*)user;
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(gest->pos[0], gest->pos[1]);
    io.MouseDown[0] = (gest->type == GESTURE_DRAG) &&
                      (gest->state != GESTURE_END);
    if (gest->state == GESTURE_BEGIN && !gui->mouse_in_view)
        gui->capture_mouse = true;
    if (gest->state == GESTURE_END)
        gui->capture_mouse = false;
    return 0;
}


static void gui_init(const inputs_t *inputs)
{
    gui = (gui_t*)calloc(1, sizeof(*gui));
    init_prog(&gui->prog);
    GL(glGenBuffers(1, &gui->array_buffer));
    GL(glGenBuffers(1, &gui->index_buffer));
    init_ImGui(inputs);
    gui->gestures.drag.type = GESTURE_DRAG;
    gui->gestures.drag.callback = on_gesture;
    gui->gestures.hover.type = GESTURE_HOVER;
    gui->gestures.hover.callback = on_gesture;

    g_tex_icons = texture_new_image("asset://data/icons.png", 0);
    GL(glBindTexture(GL_TEXTURE_2D, g_tex_icons->tex));
}

void gui_release(void)
{
    ImGui::Shutdown();
}

// XXX: Move this somewhere else.
void render_axis_arrows(goxel_t *goxel, const float view_size[2])
{
    const float AXIS[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    int i;
    const int d = 40;  // Distance to corner of the view.
    float spos[2] = {d, d};
    float pos[3], normal[3], b[3];
    uint8_t c[4];
    float s = 1;
    float view[4] = {0, 0, view_size[0], view_size[1]};
    camera_get_ray(&goxel->camera, spos, view, pos, normal);
    if (goxel->camera.ortho)
        s = goxel->camera.dist / 32;
    else
        vec3_iaddk(pos, normal, 100);

    for (i = 0; i < 3; i++) {
        vec3_addk(pos, AXIS[i], 2.0 * s, b);
        vec4_set(c, AXIS[i][0] * 255,
                    AXIS[i][1] * 255,
                    AXIS[i][2] * 255, 255);
        render_line(&goxel->rend, pos, b, c);
    }
}

void render_view(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    float scale = ImGui::GetIO().DisplayFramebufferScale.y;
    view_t *view = (view_t*)cmd->UserCallbackData;
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    int rect[4] = {(int)view->rect[0],
                   (int)(height - view->rect[1] - view->rect[3]),
                   (int)view->rect[2],
                   (int)view->rect[3]};
    goxel_render_view(goxel, view->rect);
    render_submit(&goxel->rend, rect, goxel->back_color);
    GL(glViewport(0, 0, width * scale, height * scale));
}

// XXX: better replace this by something more automatic.
static void auto_grid(int nb, int i, int col)
{
    if ((i + 1) % col != 0) ImGui::SameLine();
}

static void tools_panel(goxel_t *goxel)
{
    const struct {
        int         tool;
        const char  *tool_id;
        const char  *name;
        int         icon;
    } values[] = {
        {TOOL_BRUSH,        "brush",     "Brush",        ICON_TOOL_BRUSH},
        {TOOL_SHAPE,        "shape",     "Shape",        ICON_TOOL_SHAPE},
        {TOOL_LASER,        "laser",     "Laser",        ICON_TOOL_LASER},
        {TOOL_SET_PLANE,    "plane",     "Plane",        ICON_TOOL_PLANE},
        {TOOL_MOVE,         "move",      "Move",         ICON_TOOL_MOVE},
        {TOOL_PICK_COLOR,   "pick_color","Pick Color",   ICON_TOOL_PICK},
        {TOOL_SELECTION,    "selection", "Selection",    ICON_TOOL_SELECTION},
        {TOOL_EXTRUDE,      "extrude",   "Extrude",      ICON_TOOL_EXTRUDE},
        {TOOL_PROCEDURAL,   "procedural","Procedural",   ICON_TOOL_PROCEDURAL},
    };
    const int nb = ARRAY_SIZE(values);
    int i;
    bool v;
    char action_id[64];
    char label[64];
    const action_t *action = NULL;

    gui_group_begin(NULL);
    for (i = 0; i < nb; i++) {
        v = goxel->tool->id == values[i].tool;
        sprintf(label, "%s", values[i].name);
        if (values[i].tool_id) {
            sprintf(action_id, "tool_set_%s", values[i].tool_id);
            action = action_get(action_id);
            assert(action);
            if (action->shortcut)
                sprintf(label, "%s (%s)", values[i].name, action->shortcut);
        }
        if (gui_selectable_icon(label, &v, values[i].icon)) {
            action_exec(action, "");
        }
        auto_grid(nb, i, 4);
    }
    gui_group_end();
    auto_adjust_panel_size();

    ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Once);
    if (ImGui::CollapsingHeader("Tool Options"))
        tool_gui(goxel->tool);
}

static void toggle_layer_only_visible(goxel_t *goxel, layer_t *layer)
{
    layer_t *other;
    bool others_all_invisible = true;
    DL_FOREACH(goxel->image->layers, other) {
        if (other == layer) continue;
        if (other->visible) {
            others_all_invisible = false;
            break;
        }
    }
    DL_FOREACH(goxel->image->layers, other)
        other->visible = others_all_invisible;
    layer->visible = true;
}

static bool layer_item(int i, int icon, bool *visible, bool *edit,
                       char *name, int len)
{
    const theme_t *theme = theme_get();
    bool ret = false;
    bool edit_ = *edit;
    static char *edit_name = NULL;
    static bool start_edit;
    float font_size = ImGui::GetFontSize();
    ImVec2 center;
    ImVec2 uv0, uv1;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::PushID(i);
    ImGui::PushStyleColor(ImGuiCol_Button, COLOR(WIDGET, INNER, *edit));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            color_lighten(COLOR(WIDGET, INNER, *edit), 1.2));
    if (visible) {
        if (gui_selectable_icon("##visible", &edit_,
                *visible ? ICON_VISIBILITY : ICON_VISIBILITY_OFF)) {
            *visible = !*visible;
            ret = true;
        }
        ImGui::SameLine();
    }

    if (edit_name != name) {
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5));
        if (icon != -1) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                    ImVec2(theme->sizes.icons_height / 1.5, 0));
        }
        if (ImGui::Button(name, ImVec2(-1, theme->sizes.icons_height))) {
            *edit = true;
            ret = true;
        }
        if (icon != -1) ImGui::PopStyleVar();
        if (icon > 0) {
            center = ImGui::GetItemRectMin() +
                ImVec2(theme->sizes.icons_height / 2 / 1.5,
                       theme->sizes.icons_height / 2);
            uv0 = ImVec2(((icon - 1) % 8) / 8.0, ((icon - 1) / 8) / 8.0);
            uv1 = ImVec2(uv0.x + 1. / 8, uv0.y + 1. / 8);
            draw_list->AddImage(
                    (void*)(intptr_t)g_tex_icons->tex,
                    center - ImVec2(12, 12),
                    center + ImVec2(12, 12),
                    uv0, uv1, ImGui::GetColorU32(COLOR(WIDGET, TEXT, 0)));
        }
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            edit_name = name;
            start_edit = true;
        }
    } else {
        if (start_edit) ImGui::SetKeyboardFocusHere();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            ImVec2(theme->sizes.item_padding_h,
                            (theme->sizes.icons_height - font_size) / 2));
        ImGui::InputText("##name_edit", name, len,
                         ImGuiInputTextFlags_AutoSelectAll);
        if (!start_edit && !ImGui::IsItemActive()) edit_name = NULL;
        start_edit = false;
        ImGui::PopStyleVar();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopID();
    return ret;
}

static void layers_panel(goxel_t *goxel)
{
    layer_t *layer;
    int i = 0, icon, bbox[2][3];
    bool current, visible, bounded;
    gui_group_begin(NULL);
    DL_FOREACH(goxel->image->layers, layer) {
        current = goxel->image->active_layer == layer;
        visible = layer->visible;
        icon = layer->base_id ? ICON_LINK : -1;
        layer_item(i, icon, &visible, &current,
                   layer->name, sizeof(layer->name));
        if (current && goxel->image->active_layer != layer) {
            goxel->image->active_layer = layer;
            goxel_update_meshes(goxel, -1);
        }
        if (visible != layer->visible) {
            layer->visible = visible;
            if (ImGui::IsKeyDown(KEY_LEFT_SHIFT))
                toggle_layer_only_visible(goxel, layer);
            goxel_update_meshes(goxel, -1);
        }
        i++;
        auto_adjust_panel_size();
    }
    gui_group_end();
    gui_action_button("img_new_layer", NULL, 0, "");
    ImGui::SameLine();
    gui_action_button("img_del_layer", NULL, 0, "");
    ImGui::SameLine();
    gui_action_button("img_move_layer_up", NULL, 0, "");
    ImGui::SameLine();
    gui_action_button("img_move_layer_down", NULL, 0, "");

    layer = goxel->image->active_layer;
    bounded = !box_is_null(layer->box.mat);

    gui_group_begin(NULL);
    gui_action_button("img_duplicate_layer", "Duplicate", 1, "");
    gui_action_button("img_clone_layer", "Clone", 1, "");
    gui_action_button("img_merge_visible_layers", "Merge visible", 1, "");
    if (bounded && gui_button("Crop to box", 1, 0)) {
        mesh_crop(layer->mesh, &layer->box);
        goxel_update_meshes(goxel, -1);
    }
    gui_group_end();

    if (layer->base_id) {
        gui_group_begin(NULL);
        gui_action_button("img_unclone_layer", "Unclone", 1, "");
        gui_action_button("img_select_parent_layer", "Select parent", 1, "");
        gui_group_end();
    }
    if (ImGui::Checkbox("Bounded", &bounded)) {
        if (bounded) {
            mesh_get_bbox(layer->mesh, bbox, true);
            if (bbox[0][0] > bbox[1][0]) memset(bbox, 0, sizeof(bbox));
            bbox_from_aabb(layer->box.mat, bbox);
        } else {
            mat4_copy(mat4_zero, layer->box.mat);
        }
    }
    if (bounded) gui_bbox(layer->box.mat);
}


static bool render_palette_entry(const uint8_t color[4], uint8_t target[4])
{
    bool ret;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const theme_t *theme = theme_get();

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ret = ImGui::Button("", ImVec2(theme->sizes.item_height,
                                   theme->sizes.item_height));
    if (memcmp(color, target, 4) == 0) {
        draw_list->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                           0xFFFFFFFF, 0, 0, 1);
    }
    ImGui::PopStyleColor(2);
    if (ret) memcpy(target, color, 4);
    return ret;
}


static void palette_panel(goxel_t *goxel)
{
    palette_t *p;
    int i, current, nb = 0, nb_col = 8;
    const char **names;

    DL_COUNT(goxel->palettes, p, nb);
    names = (const char**)calloc(nb, sizeof(*names));

    i = 0;
    DL_FOREACH(goxel->palettes, p) {
        if (p == goxel->palette) current = i;
        names[i++] = p->name;
    }
    ImGui::PushItemWidth(-1);
    if (gui_combo("##palettes", &current, names, nb)) {
        goxel->palette = goxel->palettes;
        for (i = 0; i < current; i++) goxel->palette = goxel->palette->next;
    }
    ImGui::PopItemWidth();
    free(names);

    p = goxel->palette;

    for (i = 0; i < p->size; i++) {
        ImGui::PushID(i);
        render_palette_entry(p->entries[i].color, goxel->painter.color);
        auto_adjust_panel_size();
        if ((i + 1) % nb_col && i != p->size - 1) ImGui::SameLine();
        ImGui::PopID();
    }
}

static void render_advanced_panel(goxel_t *goxel)
{
    float v;
    ImVec4 c;
    int i;
    const struct {
        uint8_t    *color;
        const char *label;
    } COLORS[] = {
        {goxel->back_color, "Back color"},
        {goxel->grid_color, "Grid color"},
        {goxel->image_box_color, "Box color"},
    };

    ImGui::PushID("render_advanced");
    ImGui::Text("Light");
    gui_angle("Pitch", &goxel->rend.light.pitch, -90, +90);
    gui_angle("Yaw", &goxel->rend.light.yaw, 0, 360);
    gui_checkbox("Fixed", &goxel->rend.light.fixed, NULL);

    gui_group_begin(NULL);
    v = goxel->rend.settings.border_shadow;
    if (gui_input_float("bshadow", &v, 0.1, 0.0, 1.0, NULL)) {
        v = clamp(v, 0, 1); \
        goxel->rend.settings.border_shadow = v;
    }
#define MAT_FLOAT(name, min, max) \
    v = goxel->rend.settings.name;  \
    if (gui_input_float(#name, &v, 0.1, min, max, NULL)) { \
        v = clamp(v, min, max); \
        goxel->rend.settings.name = v; \
    }

    MAT_FLOAT(ambient, 0, 1);
    MAT_FLOAT(diffuse, 0, 1);
    MAT_FLOAT(specular, 0, 1);
    MAT_FLOAT(shininess, 0.1, 10);
    MAT_FLOAT(smoothness, 0, 1);

#undef MAT_FLOAT
    gui_group_end();

    ImGui::CheckboxFlags("Borders",
            (unsigned int*)&goxel->rend.settings.effects, EFFECT_BORDERS);
    ImGui::CheckboxFlags("Borders all",
            (unsigned int*)&goxel->rend.settings.effects, EFFECT_BORDERS_ALL);
    ImGui::CheckboxFlags("See back",
            (unsigned int*)&goxel->rend.settings.effects, EFFECT_SEE_BACK);
    if (ImGui::CheckboxFlags("Marching Cubes",
            (unsigned int*)&goxel->rend.settings.effects, EFFECT_MARCHING_CUBES)) {
        goxel->rend.settings.smoothness = 1;
    }
    if (goxel->rend.settings.effects & EFFECT_MARCHING_CUBES)
        ImGui::CheckboxFlags("Flat",
            (unsigned int*)&goxel->rend.settings.effects, EFFECT_FLAT);

    ImGui::Text("Other");
    for (i = 0; i < (int)ARRAY_SIZE(COLORS); i++) {
        ImGui::PushID(COLORS[i].label);
        c = COLORS[i].color;
        ImGui::ColorButton(c);
        if (ImGui::BeginPopupContextItem("color context menu", 0)) {
            color_edit("##edit", COLORS[i].color);
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::Text("%s", COLORS[i].label);
        ImGui::PopID();
    }
    gui_action_checkbox("grid_visible", "Show grid");
    ImGui::PopID();
}


static void render_panel(goxel_t *goxel)
{
    int i, current = -1;
    int nb = render_get_default_settings(0, NULL, NULL);
    float v;
    char *name;
    const char **names;
    render_settings_t settings;

    ImGui::Checkbox("Ortho", &goxel->camera.ortho);
    names = (const char**)calloc(nb, sizeof(*names));
    for (i = 0; i < nb; i++) {
        render_get_default_settings(i, &name, &settings);
        names[i] = name;
        if (memcmp(&goxel->rend.settings, &settings,
                         sizeof(settings)) == 0)
            current = i;
    }
    gui_text("Presets:");
    if (gui_combo("##Presets", &current, names, nb)) {
        render_get_default_settings(current, NULL, &settings);
        goxel->rend.settings = settings;
    }
    free(names);

    v = goxel->rend.settings.shadow;
    if (gui_input_float("shadow", &v, 0.1, 0, 0, NULL)) {
        goxel->rend.settings.shadow = clamp(v, 0, 1);
    }
    ImGui::SetNextTreeNodeOpen(false, ImGuiSetCond_Once);
    if (ImGui::CollapsingHeader("Render Advanced"))
        render_advanced_panel(goxel);
}

static void export_panel(goxel_t *goxel)
{
    int i;
    int maxsize;
    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize));
    maxsize /= 2; // Because png export already double it.
    goxel->show_export_viewport = true;
    gui_group_begin(NULL);
    i = goxel->image->export_width;
    if (gui_input_int("width", &i, 1, maxsize))
        goxel->image->export_width = clamp(i, 1, maxsize);
    i = goxel->image->export_height;
    if (gui_input_int("height", &i, 1, maxsize))
        goxel->image->export_height = clamp(i, 1, maxsize);
    gui_group_end();
}

static void image_panel(goxel_t *goxel)
{
    bool bounded;
    image_t *image = goxel->image;
    box_t *box = &image->box;

    bounded = !box_is_null(box->mat);
    if (ImGui::Checkbox("Bounded", &bounded)) {
        if (bounded)
            bbox_from_extents(box->mat, vec3_zero, 16, 16, 16);
        else
            mat4_copy(mat4_zero, box->mat);
    }
    if (bounded) gui_bbox(box->mat);
}

static void cameras_panel(goxel_t *goxel)
{
    camera_t *cam;
    int i = 0;
    bool current;
    gui_group_begin(NULL);
    DL_FOREACH(goxel->image->cameras, cam) {
        current = goxel->image->active_camera == cam;
        if (layer_item(i, -1, NULL, &current, cam->name, sizeof(cam->name))) {
            if (current) {
                camera_set(&goxel->camera, cam);
                goxel->image->active_camera = cam;
            } else {
                goxel->image->active_camera = NULL;
            }
        }
        i++;
    }
    gui_group_end();
    gui_action_button("img_new_camera", NULL, 0, "");
    ImGui::SameLine();
    gui_action_button("img_del_camera", NULL, 0, "");
    ImGui::SameLine();
    gui_action_button("img_move_camera_up", NULL, 0, "");
    ImGui::SameLine();
    gui_action_button("img_move_camera_down", NULL, 0, "");

    cam = &goxel->camera;
    gui_input_float("dist", &cam->dist, 10.0, 0, 0, NULL);

    gui_group_begin("Offset");
    gui_input_float("x", &cam->ofs[0], 1.0, 0, 0, NULL);
    gui_input_float("y", &cam->ofs[1], 1.0, 0, 0, NULL);
    gui_input_float("z", &cam->ofs[2], 1.0, 0, 0, NULL);
    gui_group_end();

    gui_quat("Rotation", cam->rot);

    gui_group_begin("Set");
    gui_action_button("view_left", "left", 0.5, ""); ImGui::SameLine();
    gui_action_button("view_right", "right", 1.0, "");
    gui_action_button("view_front", "front", 0.5, ""); ImGui::SameLine();
    gui_action_button("view_top", "top", 1.0, "");
    gui_action_button("view_default", "default", 1.0, "");
    gui_group_end();
}

static void debug_panel(goxel_t *goxel)
{
    ImGui::Text("FPS: %d", (int)round(goxel->fps));
}

static void import_image_plane(goxel_t *goxel)
{
    const char *path;
    path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
            "png\0*.png\0jpg\0*.jpg;*.jpeg\0", NULL, NULL);
    if (!path) return;
    goxel_import_image_plane(goxel, path);
}

static bool shift_alpha_popup(void)
{
    static int v = 0;
    static mesh_t *original_mesh = NULL;
    mesh_t *mesh;
    mesh = goxel->image->active_layer->mesh;
    if (!original_mesh)
        original_mesh = mesh_copy(mesh);
    if (ImGui::InputInt("shift", &v, 1)) {
        mesh_set(mesh, original_mesh);
        mesh_shift_alpha(mesh, v);
        goxel_update_meshes(goxel, -1);
    }
    if (ImGui::Button("OK")) {
        mesh_delete(original_mesh);
        original_mesh = NULL;
        return true;
    }
    return false;
}

static bool about_popup(void)
{
    using namespace ImGui;
    Text("Goxel " GOXEL_VERSION_STR);
    Text("Copyright © 2015-2017 Guillaume Chereau");
    Text("<guillaume@noctua-software.com>");
    Text("GPL 3 License");
    Text("http://guillaumechereau.github.io/goxel");

    SetNextTreeNodeOpen(true, ImGuiSetCond_Once);
    if (CollapsingHeader("Credits")) {
        Text("Code:");
        BulletText("Guillaume Chereau <guillaume@noctua-software.com>");
        BulletText("Dustin Willis Webber <dustin.webber@gmail.com>");
        BulletText("Pablo Hugo Reda <pabloreda@gmail.com>");
        BulletText("Othelarian (https://github.com/othelarian)");

        Text("Libraries:");
        BulletText("dear imgui (https://github.com/ocornut/imgui)");
        BulletText("stb (https://github.com/nothings/stb)");
        BulletText("uthash (https://troydhanson.github.io/uthash/)");
        BulletText("inih (https://github.com/benhoyt/inih)");

        Text("Design:");
        BulletText("Guillaume Chereau <guillaume@noctua-software.com>");
        BulletText("Michal (https://github.com/YarlBoro)");
    }
    return gui_button("OK", 0, 0);
}

static int check_action_shortcut(const action_t *action, void *user)
{
    ImGuiIO& io = ImGui::GetIO();
    const char *s = action->shortcut;
    bool check_key = true;
    bool check_char = true;
    if (!s) return 0;
    if (io.KeyCtrl) {
        if (!str_startswith(s, "Ctrl")) return 0;
        s += strlen("Ctrl ");
        check_char = false;
    }
    if (io.KeyShift) {
        check_key = false;
    }
    if (    (check_char && isCharPressed(s[0])) ||
            (check_key && ImGui::IsKeyPressed(s[0], false))) {
        action_exec(action, "");
        return 1;
    }
    return 0;
}

static int import_menu_action_callback(const action_t *a, void *user)
{
    if (!a->file_format.name) return 0;
    if (!str_startswith(a->id, "import_")) return 0;
    if (ImGui::MenuItem(a->file_format.name)) action_exec(a, "");
    return 0;
}

static int export_menu_action_callback(const action_t *a, void *user)
{
    if (!a->file_format.name) return 0;
    if (!str_startswith(a->id, "export_")) return 0;
    if (ImGui::MenuItem(a->file_format.name)) action_exec(a, "");
    return 0;
}

static bool render_menu_item(const char *id, const char *label)
{
    const action_t *action = action_get(id);
    assert(action);
    if (ImGui::MenuItem(label, action->shortcut)) {
        action_exec(action, "");
        return true;
    }
    return false;
}

static void render_menu(void)
{
    ImGui::PushStyleColor(ImGuiCol_PopupBg, COLOR(MENU, INNER, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, COLOR(MENU, TEXT, 0));

    if (!ImGui::BeginMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        render_menu_item("save", "Save");
        render_menu_item("save_as", "Save as");
        render_menu_item("open", "Open");
        if (ImGui::BeginMenu("Import...")) {
            if (ImGui::MenuItem("image plane")) import_image_plane(goxel);
            actions_iter(import_menu_action_callback, NULL);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Export As..")) {
            actions_iter(export_menu_action_callback, NULL);
            ImGui::EndMenu();
        }
        render_menu_item("quit", "Quit");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Clear", "Delete"))
            action_exec2("layer_clear", "");
        render_menu_item("undo", "Undo");
        render_menu_item("redo", "Redo");
        render_menu_item("copy", "Copy");
        render_menu_item("past", "Past");
        if (ImGui::MenuItem("Shift Alpha"))
            gui_open_popup("Shift Alpha", shift_alpha_popup, 0);
        if (ImGui::MenuItem("Settings"))
            gui_open_popup("Settings", gui_settings_popup,
                    GUI_POPUP_FULL | GUI_POPUP_RESIZE);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        render_menu_item("view_left", "Left");
        render_menu_item("view_right", "Right");
        render_menu_item("view_front", "Front");
        render_menu_item("view_top", "Top");
        render_menu_item("view_default", "Default");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About"))
            gui_open_popup("About", about_popup, 0);
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
    ImGui::PopStyleColor(2);
}

static bool render_tab(const char *label, bool *v)
{
    ImFont *font = GImGui->Font;
    const ImFont::Glyph *glyph;
    char c;
    bool ret;
    const theme_t *theme = theme_get();
    int pad = theme->sizes.item_padding_h;
    ImVec2 text_size = ImGui::CalcTextSize(label);
    int xpad = (theme->sizes.item_height - text_size.y) / 2;
    float scale = ImGui::GetIO().DisplayFramebufferScale.y;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImVec2 pos = window->DC.CursorPos + ImVec2(0, text_size.x + pad);

    ImGui::PushStyleColor(ImGuiCol_Button, COLOR(TAB, INNER, *v));

    ImGui::PushID(label);

    ret = ImGui::InvisibleButton("", ImVec2(theme->sizes.item_height,
                                            text_size.x + pad * 2));
    ImGui::GoxBox(ImGui::GetItemRectMin(), ImGui::GetItemRectSize(),
                  false, 0x09);

    ImGui::PopStyleColor();
    while ((c = *label++)) {
        glyph = font->FindGlyph(c);
        if (!glyph) continue;

        window->DrawList->PrimReserve(6, 4);
        window->DrawList->PrimQuadUV(
                pos + ImVec2(glyph->Y0 + xpad, -glyph->X0) / scale,
                pos + ImVec2(glyph->Y0 + xpad, -glyph->X1) / scale,
                pos + ImVec2(glyph->Y1 + xpad, -glyph->X1) / scale,
                pos + ImVec2(glyph->Y1 + xpad, -glyph->X0) / scale,

                ImVec2(glyph->U0, glyph->V0),
                ImVec2(glyph->U1, glyph->V0),
                ImVec2(glyph->U1, glyph->V1),
                ImVec2(glyph->U0, glyph->V1),
                ImGui::ColorConvertFloat4ToU32(COLOR(TAB, TEXT, *v)));
        pos.y -= glyph->XAdvance / scale;
    }
    ImGui::PopID();
    return ret;
}

static void render_left_panel(void)
{
    int i;
    const theme_t *theme = theme_get();
    float left_pane_width;
    const struct {
        const char *name;
        void (*fn)(goxel_t *goxel);
    } PANELS[] = {
        {"Tools", tools_panel},
        {"Palette", palette_panel},
        {"Layers", layers_panel},
        {"Render", render_panel},
        {"Cameras", cameras_panel},
        {"Image", image_panel},
        {"Export", export_panel},
        {"Debug", debug_panel},
    };
    ImDrawList* draw_list;

    left_pane_width = max(168, gui->min_panel_size);
    gui->min_panel_size = 0;
    ImGui::BeginChild("left pane", ImVec2(left_pane_width, 0), true);

    ImGui::BeginGroup();
    draw_list = ImGui::GetWindowDrawList();
    ImVec2 rmin = ImGui::GetCursorScreenPos() - ImVec2(4, 4);
    ImVec2 rmax = rmin + ImVec2(theme->sizes.item_height + 4,
                                ImGui::GetWindowHeight());
    draw_list->AddRectFilled(rmin, rmax,
                ImGui::ColorConvertFloat4ToU32(COLOR(TAB, BACKGROUND, 0)));

    for (i = 0; i < (int)ARRAY_SIZE(PANELS); i++) {
        bool b = (gui->current_panel == (int)i);
        if (render_tab(PANELS[i].name, &b)) {
            on_click();
            gui->current_panel = i;
        }
    }
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::BeginGroup();

    goxel->show_export_viewport = false;

    ImGui::PushID("panel");
    ImGui::PushID(PANELS[gui->current_panel].name);
    PANELS[gui->current_panel].fn(goxel);
    ImGui::PopID();
    ImGui::PopID();

    ImGui::EndGroup();
    ImGui::EndChild();
}

void gui_iter(goxel_t *goxel, const inputs_t *inputs)
{
    if (!gui) gui_init(inputs);
    unsigned int i;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw_list;
    ImGuiStyle& style = ImGui::GetStyle();
    const theme_t *theme = theme_get();
    gesture_t *gestures[] = {&gui->gestures.drag, &gui->gestures.hover};
    float display_rect[4] = {
        0, 0, goxel->screen_size[0], goxel->screen_size[1]};
    float font_size = ImGui::GetFontSize();

    io.DisplaySize = ImVec2((float)goxel->screen_size[0],
                            (float)goxel->screen_size[1]);

    io.DisplayFramebufferScale = ImVec2(goxel->screen_scale,
                                        goxel->screen_scale);
    io.DeltaTime = 1.0 / 60;
    gesture_update(2, gestures, inputs, display_rect, gui);
    io.MouseWheel = inputs->mouse_wheel;

    for (i = 0; i < ARRAY_SIZE(inputs->keys); i++)
        io.KeysDown[i] = inputs->keys[i];
    io.KeyShift = inputs->keys[KEY_LEFT_SHIFT] ||
                  inputs->keys[KEY_RIGHT_SHIFT];
    io.KeyCtrl = inputs->keys[KEY_CONTROL];
    for (i = 0; i < ARRAY_SIZE(inputs->chars); i++) {
        if (!inputs->chars[i]) break;
        io.AddInputCharacter(inputs->chars[i]);
    }

    // Setup theme.
    style.FramePadding = ImVec2(theme->sizes.item_padding_h,
                                (theme->sizes.item_height - font_size) / 2);
    style.FrameRounding = theme->sizes.item_rounding;
    style.ItemSpacing = ImVec2(theme->sizes.item_spacing_h,
                               theme->sizes.item_spacing_v);
    style.ItemInnerSpacing = ImVec2(theme->sizes.item_inner_spacing_h, 0);
    style.ScrollbarSize = theme->sizes.item_height;

    style.Colors[ImGuiCol_WindowBg] = COLOR(BASE, BACKGROUND, 0);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.38, 0.38, 0.38, 1.0);
    style.Colors[ImGuiCol_Header] = style.Colors[ImGuiCol_WindowBg];
    style.Colors[ImGuiCol_Text] = COLOR(BASE, TEXT, 0);
    style.Colors[ImGuiCol_Button] = COLOR(BASE, INNER, 0);
    style.Colors[ImGuiCol_FrameBg] = COLOR(BASE, INNER, 0);
    style.Colors[ImGuiCol_ButtonActive] = COLOR(BASE, INNER, 1);
    style.Colors[ImGuiCol_ButtonHovered] =
        color_lighten(COLOR(BASE, INNER, 0), 1.2);
    style.Colors[ImGuiCol_CheckMark] = COLOR(WIDGET, INNER, 1);
    style.Colors[ImGuiCol_ComboBg] = COLOR(WIDGET, INNER, 0);
    style.Colors[ImGuiCol_MenuBarBg] = COLOR(MENU, BACKGROUND, 0);
    style.Colors[ImGuiCol_Border] = COLOR(BASE, OUTLINE, 0);

    ImGui::NewFrame();

    // Create the root fullscreen window.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_MenuBar |
                                    ImGuiWindowFlags_NoCollapse;

    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::Begin("Goxel", NULL, window_flags);

    render_menu();
    render_left_panel();
    ImGui::SameLine();

    if (gui->popup.title) {
        ImGui::OpenPopup(gui->popup.title);
        int flags = ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoMove;
        if (gui->popup.flags & GUI_POPUP_FULL) {
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 40,
                                            io.DisplaySize.y - 40),
                        (gui->popup.flags & GUI_POPUP_RESIZE) ?
                            ImGuiSetCond_Once : 0);
        }
        if (gui->popup.flags & GUI_POPUP_RESIZE) {
            flags &= ~(ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_AlwaysAutoResize);
        }
        if (ImGui::BeginPopupModal(gui->popup.title, NULL, flags)) {
            typeof(gui->popup.func) func = gui->popup.func;
            if (func()) {
                ImGui::CloseCurrentPopup();
                // Only reset if there was no change.
                if (func == gui->popup.func) {
                    gui->popup.title = NULL;
                    gui->popup.func = NULL;
                }
            }
            ImGui::EndPopup();
        }
    }

    ImGui::BeginChild("3d view", ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollWithMouse);
    // ImGui::Text("3d view");
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    canvas_size.y -= 20; // Leave space for the help label.
    vec4_set(gui->view.rect, canvas_pos.x, canvas_pos.y,
                             canvas_size.x, canvas_size.y);
    draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCallback(render_view, &gui->view);
    // Invisible button so that we catch inputs.
    ImGui::InvisibleButton("canvas", canvas_size);
    gui->mouse_in_view = ImGui::IsItemHovered();
    float view_size[2] = {gui->view.rect[2], gui->view.rect[3]};
    float view_rect[4] = {canvas_pos.x,
                          io.DisplaySize.y - (canvas_pos.y + canvas_size.y),
                          canvas_size.x, canvas_size.y};
    // Call mouse_in_view with inputs in the view referential.
    if (!(!gui->mouse_in_view && inputs->mouse_wheel) &&
            !gui->capture_mouse) {
        inputs_t inputs2 = *inputs;
        for (i = 0; i < ARRAY_SIZE(inputs->touches); i++) {
            inputs2.touches[i].pos[1] =
                io.DisplaySize.y - inputs2.touches[i].pos[1];
        }
        goxel_mouse_in_view(goxel, view_rect, &inputs2);
    }

    render_axis_arrows(goxel, view_size);
    ImGui::Text("%s", goxel->hint_text ?: "");
    ImGui::SameLine(180);
    ImGui::Text("%s", goxel->help_text ?: "");

    ImGui::EndChild();
    ImGui::End();

    // Handle the shortcuts.  XXX: this should be done with actions.
    if (ImGui::IsKeyPressed(KEY_DELETE, false))
        action_exec2("layer_clear", "");
    if (ImGui::IsKeyPressed(' ', false) && goxel->painter.mode == MODE_OVER)
        goxel->painter.mode = MODE_SUB;
    if (ImGui::IsKeyReleased(' ') && goxel->painter.mode == MODE_SUB)
        goxel->painter.mode = MODE_OVER;
    if (ImGui::IsKeyReleased(' ') && goxel->painter.mode == MODE_SUB)
        goxel->painter.mode = MODE_OVER;
    float last_tool_radius = goxel->tool_radius;

    if (!io.WantCaptureKeyboard) {
        if (isCharPressed('[')) goxel->tool_radius -= 0.5;
        if (isCharPressed(']')) goxel->tool_radius += 0.5;
        if (goxel->tool_radius != last_tool_radius) {
            goxel->tool_radius = clamp(goxel->tool_radius, 0.5, 64);
        }

        // XXX: this won't map correctly to a French keyboard.  Unfortunately as
        // far as I can tell, GLFW3 does not allow to check for ctrl-Z on any
        // layout on Windows.  For the moment I just ignore the problem until I
        // either find a solution, either find a replacement for GLFW.
        // With the GLUT backend ctrl-z and ctrl-y are actually reported as the
        // key 25 and 26, which might makes more sense.  Here I test for both.
        if (isCharPressed(26)) action_exec2("undo", "");
        if (isCharPressed(25)) action_exec2("redo", "");
        // Check the action shortcuts.
        actions_iter(check_action_shortcut, NULL);
    }
}

void gui_render(void)
{
    ImGui::Render();
}

extern "C" {
using namespace ImGui;

static void stencil_callback(
        const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    int op = ((intptr_t)cmd->UserCallbackData);

    switch (op) {
    case 0: // Reset
        GL(glDisable(GL_STENCIL_TEST));
        GL(glStencilMask(0x00));
        break;
    case 1: // Write
        GL(glEnable(GL_STENCIL_TEST));
        GL(glStencilFunc(GL_ALWAYS, 1, 0xFF));
        GL(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));
        GL(glStencilMask(0xFF));
        break;
    case 2: // Filter
        GL(glEnable(GL_STENCIL_TEST));
        GL(glStencilFunc(GL_EQUAL, 1, 0xFF));
        GL(glStencilMask(0x00));
        break;
    default:
        assert(false);
        break;
    }
}

void gui_group_begin(const char *label)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (label) ImGui::Text("%s", label);
    ImGui::PushID(label ? label : "group");
    gui->group++;
    draw_list->ChannelsSplit(2);
    draw_list->ChannelsSetCurrent(1);
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
}

void gui_group_end(void)
{
    gui->group--;
    ImGui::PopID();
    ImGui::PopStyleVar(2);
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::EndGroup();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->ChannelsSetCurrent(0);
    ImVec2 pos = ImGui::GetItemRectMin();
    ImVec2 size = ImGui::GetItemRectMax() - pos;

    // Stencil write.
    draw_list->AddCallback(stencil_callback, (void*)(intptr_t)1);
    GoxBox2(pos + ImVec2(1, 1), size - ImVec2(2, 2),
            COLOR(WIDGET, OUTLINE, 0), true);
    // Stencil filter.
    draw_list->AddCallback(stencil_callback, (void*)(intptr_t)2);

    draw_list->ChannelsMerge();
    // Stencil reset.
    draw_list->AddCallback(stencil_callback, (void*)(intptr_t)0);
    GoxBox2(pos, size, COLOR(WIDGET, OUTLINE, 0), false);
}

bool gui_input_int(const char *label, int *v, int minv, int maxv)
{
    float minvf = minv;
    float maxvf = maxv;
    bool ret;
    float vf = *v;
    if (minv == 0 && maxv == 0) {
        minvf = -FLT_MAX;
        maxvf = +FLT_MAX;
    }
    ret = gui_input_float(label, &vf, 1, minvf, maxvf, "%.0f");
    if (ret) *v = vf;
    return ret;
}

bool gui_input_float(const char *label, float *v, float step,
                     float minv, float maxv, const char *format)
{
    bool self_group = false, ret;
    if (minv == 0.f && maxv == 0.f) {
        minv = -FLT_MAX;
        maxv = +FLT_MAX;
    }

    if (gui->group == 0) {
        gui_group_begin(NULL);
        self_group = true;
    }

    if (step == 0.f) step = 0.1f;
    if (!format) format = "%.1f";
    ret = ImGui::GoxInputFloat(label, v, step, minv, maxv, format);
    if (ret) on_click();
    if (self_group) gui_group_end();
    return ret;
}

bool gui_bbox(float box[4][4])
{
    int x, y, z, w, h, d;
    bool ret = false;
    float p[3];
    w = box[0][0] * 2;
    h = box[1][1] * 2;
    d = box[2][2] * 2;
    x = round(box[3][0] - box[0][0]);
    y = round(box[3][1] - box[1][1]);
    z = round(box[3][2] - box[2][2]);

    gui_group_begin("Origin");
    ret |= gui_input_int("x", &x, 0, 0);
    ret |= gui_input_int("y", &y, 0, 0);
    ret |= gui_input_int("z", &z, 0, 0);
    gui_group_end();
    gui_group_begin("Size");
    ret |= gui_input_int("w", &w, 1, 2048);
    ret |= gui_input_int("h", &h, 1, 2048);
    ret |= gui_input_int("d", &d, 1, 2048);
    gui_group_end();

    if (ret) {
        vec3_set(p, x + w / 2., y + h / 2., z + d / 2.);
        bbox_from_extents(box, p, w / 2., h / 2., d / 2.);
    }
    return ret;
}

bool gui_angle(const char *id, float *v, int vmin, int vmax)
{
    int a;
    bool ret;
    a = round(*v * DR2D);
    ret = gui_input_int(id, &a, vmin, vmax);
    if (ret) {
        if (vmin == 0 && vmax == 360) {
            while (a < 0) a += 360;
            a %= 360;
        }
        a = clamp(a, vmin, vmax);
        *v = (float)(a * DD2R);
    }
    return ret;
}

bool gui_action_button(const char *id, const char *label, float size,
                       const char *sig, ...)
{
    bool ret;
    const action_t *action;
    va_list ap;

    action = action_get(id);
    assert(action);
    PushID(action->id);
    ret = gui_button(label, size, action->icon);
    if (IsItemHovered()) goxel_set_help_text(goxel, action_get(id)->help);
    if (ret) {
        va_start(ap, sig);
        action_execv(action_get(id), sig, ap);
        va_end(ap);
    }
    PopID();
    return ret;
}

bool gui_action_checkbox(const char *id, const char *label)
{
    bool b;
    const action_t *action = action_get(id);
    action_exec(action, ">b", &b);
    if (ImGui::Checkbox(label, &b)) {
        action_exec(action, "b", b);
        return true;
    }
    if (ImGui::IsItemHovered()) {
        if (!action->shortcut)
            goxel_set_help_text(goxel, action->help);
        else
            goxel_set_help_text(goxel, "%s (%s)",
                    action->help, action->shortcut);
    }
    return false;
}

static bool _selectable(const char *label, bool *v, const char *tooltip,
                        float w, int icon)
{
    const theme_t *theme = theme_get();
    ImGuiWindow* window = GetCurrentWindow();
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImVec2 size;
    ImVec2 center;
    bool ret = false;
    bool default_v = false;
    ImVec2 uv0, uv1; // The position in the icon texture.
    uint8_t color[4];
    int group = THEME_GROUP_WIDGET;

    v = v ? v : &default_v;
    size = (icon != -1) ?
        ImVec2(theme->sizes.icons_height, theme->sizes.icons_height) :
        ImVec2(w, theme->sizes.item_height);

    if (!tooltip) {
        tooltip = label;
        while (*tooltip == '#') tooltip++;
    }
    ImGui::PushID(label);

    theme_get_color(group, THEME_COLOR_INNER, *v, color);
    PushStyleColor(ImGuiCol_Button, color);
    PushStyleColor(ImGuiCol_ButtonHovered, color_lighten(
                style.Colors[ImGuiCol_Button], 1.2));
    theme_get_color(group, THEME_COLOR_TEXT, *v, color);
    PushStyleColor(ImGuiCol_Text, color);

    if (icon != -1) {
        ret = ImGui::Button("", size);
        if (icon) {
            center = (ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) / 2;
            uv0 = ImVec2(((icon - 1) % 8) / 8.0, ((icon - 1) / 8) / 8.0);
            uv1 = uv0 + ImVec2(1. / 8, 1. / 8);
            window->DrawList->AddImage((void*)(intptr_t)g_tex_icons->tex,
                                       center - ImVec2(16, 16),
                                       center + ImVec2(16, 16),
                                       uv0, uv1,
                                       ImGui::GetColorU32(color));
        }
    } else {
        ret = ImGui::Button(label, size);
    }
    ImGui::PopStyleColor(3);
    if (ret) *v = !*v;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
        goxel_set_help_text(goxel, tooltip);
    }
    ImGui::PopID();

    if (ret) on_click();
    return ret;
}

bool gui_selectable(const char *name, bool *v, const char *tooltip, float w)
{
    return _selectable(name, v, tooltip, w, -1);
}

bool gui_selectable_icon(const char *name, bool *v, int icon)
{
    return _selectable(name, v, NULL, 0, icon);
}

float gui_get_avail_width(void)
{
    return GetContentRegionAvailWidth();
}

void gui_text(const char *label)
{
    Text("%s", label);
}

void gui_same_line(void)
{
    SameLine();
}

bool gui_color(const char *label, uint8_t color[4])
{
    ImGui::PushID(label);
    ImGui::ColorButton(color);
    if (ImGui::BeginPopupContextItem("color context menu", 0)) {
        color_edit("##edit", color);
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (label && label[0] != '#') {
        ImGui::SameLine();
        ImGui::Text("%s", label);
    }
    ImGui::PopID();
    return false;
}

bool gui_checkbox(const char *label, bool *v, const char *hint)
{
    bool ret;
    ret = Checkbox(label, v);
    if (hint && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hint);
    if (ret) on_click();
    return ret;
}

bool gui_button(const char *label, float size, int icon)
{
    bool ret;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 uv0, uv1;
    ImVec2 button_size;
    const theme_t *theme = theme_get();
    ImVec2 center;

    button_size = ImVec2(size * GetContentRegionAvailWidth(),
                         theme->sizes.item_height);
    if (size == -1) button_size.x = GetContentRegionAvailWidth();
    if (size == 0 && label == NULL) button_size.x = theme->sizes.item_height;
    label = label ?: "";
    ret = Button(label, button_size);
    if (icon) {
        center = (GetItemRectMin() + GetItemRectMax()) / 2;
        uv0 = ImVec2(((icon - 1) % 8) / 8.0, ((icon - 1) / 8) / 8.0);
        uv1 = ImVec2(uv0.x + 1. / 8, uv0.y + 1. / 8);
        draw_list->AddImage((void*)(intptr_t)g_tex_icons->tex,
                            center - ImVec2(16, 16),
                            center + ImVec2(16, 16),
                            uv0, uv1,
                            ImGui::GetColorU32(COLOR(WIDGET, TEXT, 0)));
    }
    if (ret) on_click();
    return ret;
}

bool gui_input_text(const char *label, char *txt, int size)
{
    return InputText(label, txt, size);
}

bool gui_input_text_multiline(const char *label, char *buf, int size,
                              float width, float height)
{
    // We set the frame color to a semi transparent value, because otherwise
    // we cannot render the error highlight.
    // XXX: fix that.
    bool ret;
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 col = style.Colors[ImGuiCol_FrameBg];
    style.Colors[ImGuiCol_FrameBg].w = 0.5;
    ret = InputTextMultiline(label, buf, size, ImVec2(width, height));
    style.Colors[ImGuiCol_FrameBg] = col;
    auto_adjust_panel_size();
    return ret;
}

bool gui_combo(const char *label, int *v, const char **names, int nb)
{
    const theme_t *theme = theme_get();
    bool ret;
    float font_size = ImGui::GetFontSize();

    PushStyleVar(ImGuiStyleVar_ItemSpacing,
                 ImVec2(0, (theme->sizes.item_height - font_size) / 2));
    ret = Combo(label, v, names, nb);
    PopStyleVar();
    return ret;
}

void gui_input_text_multiline_highlight(int line)
{
    float h = ImGui::CalcTextSize("").y;
    ImVec2 rmin = ImGui::GetItemRectMin();
    ImVec2 rmax = ImGui::GetItemRectMax();
    rmin.y = rmin.y + line * h + 2;
    rmax.y = rmin.y + h;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(rmin, rmax, 0xff0000ff);
}

void gui_enabled_begin(bool enabled)
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 color = style.Colors[ImGuiCol_Text];
    if (!enabled) color.w /= 2;
    ImGui::PushStyleColor(ImGuiCol_Text, color);
}

void gui_enabled_end(void)
{
    ImGui::PopStyleColor();
}

bool gui_quat(const char *label, float q[4])
{
    // Hack to prevent weird behavior when we change the euler angles.
    // We keep track of the last used euler angles value and reuse them if
    // the quaternion is the same.
    static struct {
        float quat[4];
        float eul[3];
    } last = {};
    float eul[3];
    bool ret = false;

    if (memcmp(q, &last.quat, sizeof(last.quat)) == 0)
        vec3_copy(last.eul, eul);
    else
        quat_to_eul(q, EULER_ORDER_DEFAULT, eul);
    gui_group_begin(label);
    if (gui_angle("x", &eul[0], -180, +180)) ret = true;
    if (gui_angle("y", &eul[1], -180, +180)) ret = true;
    if (gui_angle("z", &eul[2], -180, +180)) ret = true;
    gui_group_end();

    if (ret) {
        eul_to_quat(eul, EULER_ORDER_DEFAULT, q);
        quat_copy(q, last.quat);
        vec3_copy(eul, last.eul);
    }
    return ret;
}

void gui_open_popup(const char *title, bool (*func)(void), int flags)
{
    gui->popup.title = title;
    gui->popup.func = func;
    gui->popup.flags = flags;
}

void gui_popup_body_begin(void) {
    ImGui::BeginChild("body",
            ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()));
}

void gui_popup_body_end(void) {
    ImGui::EndChild();
}

}
