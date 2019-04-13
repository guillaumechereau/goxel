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

void gui_layers_panel(void);
void gui_image_panel(void);
void gui_cameras_panel(void);
void gui_palette_panel(void);
void gui_material_panel(void);
void gui_tools_panel(void);
void gui_view_panel(void);
void gui_render_panel(void);
}

#ifndef typeof
#   define typeof __typeof__
#endif

#define IM_VEC4_CLASS_EXTRA \
        ImVec4(const uint8_t f[4]) { \
            x = f[0] / 255.; \
            y = f[1] / 255.; \
            z = f[2] / 255.; \
            w = f[3] / 255.; }     \

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_INCLUDE_IMGUI_USER_INL

#include "../ext_src/imgui/imgui.h"
#include "../ext_src/imgui/imgui_internal.h"

extern "C" {
    bool gui_settings_popup(void *data);
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
                 float thickness = 1,
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
    float   panel_width;
    int     panel_adjust_w; // Adjust size for scrollbar.
    struct {
        gesture_t drag;
        gesture_t hover;
    }       gestures;
    bool    mouse_in_view;
    bool    capture_mouse;
    int     group;

    struct {
        const char *title;
        bool      (*func)(void *data);
        void      (*on_closed)(void);
        int         flags;
        void       *data; // Automatically released when popup close.
        bool        opened;
    } popup[8]; // Stack of modal popups
    int popup_count;
} gui_t;

static gui_t *gui = NULL;

static void on_click(void) {
    if (DEFINED(GUI_SOUND))
        sound_play("click", 1.0, 1.0);
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

/*
 * Return the color that should be used to draw an icon depending on the
 * style and the icon.  Some icons shouldn't have their color change with
 * the style and some other do.
 */
static uint32_t get_icon_color(int icon, bool selected)
{
    return (icon >= ICON_COLORIZABLE_START && icon < ICON_COLORIZABLE_END) ?
            ImGui::GetColorU32(COLOR(WIDGET, TEXT, selected)) : 0xFFFFFFFF;
}

static void init_prog(prog_t *p)
{
    p->prog = gl_create_prog(VSHADER, FSHADER, NULL);
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
    ImGui::CreateContext();
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
    io.KeyMap[ImGuiKey_Space]       = ' ';
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

    load_fonts_texture();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.WindowPadding = ImVec2(4, 4);
}

static bool color_edit(const char *name, uint8_t color[4],
                       const uint8_t backup_color[4])
{
    bool ret = false;
    ImVec4 col = color;
    ImVec4 backup_col;
    if (backup_color) backup_col = backup_color;

    ImGui::Text("Pick Color");
    ImGui::Separator();
    ret |= ImGui::ColorPicker4("##picker", (float*)&col,
                        ImGuiColorEditFlags_NoSidePreview |
                        ImGuiColorEditFlags_NoSmallPreview |
                        ImGuiColorEditFlags_NoAlpha);
    gui_same_line();
    ImGui::BeginGroup();
    ImGui::Text("Current");
    ImGui::ColorButton("##current", col, ImGuiColorEditFlags_NoPicker,
                       ImVec2(60, 40));
    if (backup_color) {
        ImGui::Text("Previous");
        if (ImGui::ColorButton("##previous", backup_col,
                               ImGuiColorEditFlags_NoPicker,
                               ImVec2(60, 40))) {
            col = backup_col;
            ret = true;
        }
    }
    ImGui::EndGroup();

    if (ret) {
        color[0] = col.x * 255;
        color[1] = col.y * 255;
        color[2] = col.z * 255;
        color[3] = col.w * 255;
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
    if (gest->state == GESTURE_END || gest->type == GESTURE_HOVER)
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
    gui->panel_width = GUI_PANEL_WIDTH_NORMAL;

    g_tex_icons = texture_new_image("asset://data/images/icons.png", 0);
    GL(glBindTexture(GL_TEXTURE_2D, g_tex_icons->tex));
}

void gui_release(void)
{
    if (gui) ImGui::DestroyContext();
}

// XXX: Move this somewhere else.
void render_axis_arrows(const float view_size[2])
{
    const float AXIS[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    int i;
    const int d = 40;  // Distance to corner of the view.
    float spos[2] = {d, d};
    float pos[3], normal[3], b[3];
    uint8_t c[4];
    float s = 1;
    float view[4] = {0, 0, view_size[0], view_size[1]};
    camera_get_ray(&goxel.camera, spos, view, pos, normal);
    if (goxel.camera.ortho)
        s = goxel.camera.dist / 32;
    else
        vec3_iaddk(pos, normal, 100);

    for (i = 0; i < 3; i++) {
        vec3_addk(pos, AXIS[i], 2.0 * s, b);
        vec4_set(c, AXIS[i][0] * 255,
                    AXIS[i][1] * 255,
                    AXIS[i][2] * 255, 255);
        render_line(&goxel.rend, pos, b, c);
    }
}

void render_view(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    float scale = ImGui::GetIO().DisplayFramebufferScale.y;
    view_t *view = (view_t*)cmd->UserCallbackData;
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    bool render_mode;
    // XXX: 8 here means 'export' panel.  Need to use an enum or find a
    // better way!
    render_mode = gui->current_panel == 8 && goxel.pathtracer.status;
    goxel_render_view(view->rect, render_mode);
    GL(glViewport(0, 0, width * scale, height * scale));
}

// XXX: better replace this by something more automatic.
static void auto_grid(int nb, int i, int col)
{
    if ((i + 1) % col != 0) gui_same_line();
}

static void debug_panel(void)
{
    gui_text("FPS: %d", (int)round(goxel.fps));
    if (!DEFINED(GLES2))
        gui_checkbox("Show wireframe", &goxel.show_wireframe, NULL);
}

static void import_image_plane(void)
{
    const char *path;
    path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
            "png\0*.png\0jpg\0*.jpg;*.jpeg\0", NULL, NULL);
    if (!path) return;
    goxel_import_image_plane(path);
}

static bool shift_alpha_popup(void *data)
{
    static int v = 0;
    static mesh_t *original_mesh = NULL;
    mesh_t *mesh;
    mesh = goxel.image->active_layer->mesh;
    if (!original_mesh)
        original_mesh = mesh_copy(mesh);
    if (ImGui::InputInt("shift", &v, 1)) {
        mesh_set(mesh, original_mesh);
        mesh_shift_alpha(mesh, v);
        goxel_update_meshes(-1);
    }
    if (ImGui::Button("OK")) {
        mesh_delete(original_mesh);
        original_mesh = NULL;
        return true;
    }
    return false;
}

static bool about_popup(void *data)
{
    using namespace ImGui;
    Text("Goxel " GOXEL_VERSION_STR);
    Text("Copyright © 2015-2017 Guillaume Chereau");
    Text("<guillaume@noctua-software.com>");
    Text("GPL 3 License");
    Text("http://guillaumechereau.github.io/goxel");

    if (gui_collapsing_header("Credits", true)) {
        Text("Code:");
        BulletText("Guillaume Chereau <guillaume@noctua-software.com>");
        BulletText("Dustin Willis Webber <dustin.webber@gmail.com>");
        BulletText("Pablo Hugo Reda <pabloreda@gmail.com>");
        BulletText("Othelarian (https://github.com/othelarian)");

        Text("Libraries:");
        BulletText("dear imgui (https://github.com/ocornut/imgui)");
        BulletText("stb (https://github.com/nothings/stb)");
        BulletText("yocto-gl (https://github.com/xelatihy/yocto-gl)");
        BulletText("uthash (https://troydhanson.github.io/uthash/)");
        BulletText("inih (https://github.com/benhoyt/inih)");

        Text("Design:");
        BulletText("Guillaume Chereau <guillaume@noctua-software.com>");
        BulletText("Michal (https://github.com/YarlBoro)");
    }
    return gui_button("OK", 0, 0);
}

static bool alert_popup(void *data)
{
    if (data) gui_text((const char *)data);
    return gui_button("OK", 0, 0);
}

static int check_action_shortcut(action_t *action, void *user)
{
    ImGuiIO& io = ImGui::GetIO();
    const char *s = action->shortcut;
    bool check_key = true;
    bool check_char = true;
    if (!*s) return 0;
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

static int import_menu_action_callback(action_t *a, void *user)
{
    if (!a->file_format.name) return 0;
    if (!str_startswith(a->id, "import_")) return 0;
    if (ImGui::MenuItem(a->file_format.name)) action_exec(a, "");
    return 0;
}

static int export_menu_action_callback(action_t *a, void *user)
{
    if (!a->file_format.name) return 0;
    if (!str_startswith(a->id, "export_")) return 0;
    if (ImGui::MenuItem(a->file_format.name)) action_exec(a, "");
    return 0;
}

static bool render_menu_item(const char *id, const char *label, bool enabled)
{
    const action_t *action = action_get(id, true);
    assert(action);
    if (ImGui::MenuItem(label, action->shortcut, false, enabled)) {
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
        render_menu_item("save", "Save",
                image_get_key(goxel.image) != goxel.image->saved_key);
        render_menu_item("save_as", "Save as", true);
        render_menu_item("open", "Open", true);
        if (ImGui::BeginMenu("Import...")) {
            if (ImGui::MenuItem("image plane")) import_image_plane();
            actions_iter(import_menu_action_callback, NULL);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Export As..")) {
            actions_iter(export_menu_action_callback, NULL);
            ImGui::EndMenu();
        }
        render_menu_item("quit", "Quit", true);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Clear", "Delete"))
            action_exec2("layer_clear", "");
        render_menu_item("undo", "Undo", true);
        render_menu_item("redo", "Redo", true);
        render_menu_item("copy", "Copy", true);
        render_menu_item("past", "Past", true);
        if (ImGui::MenuItem("Shift Alpha"))
            gui_open_popup("Shift Alpha", 0, NULL, shift_alpha_popup);
        if (ImGui::MenuItem("Settings"))
            gui_open_popup("Settings", GUI_POPUP_FULL | GUI_POPUP_RESIZE,
                           NULL, gui_settings_popup);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        render_menu_item("view_left", "Left", true);
        render_menu_item("view_right", "Right", true);
        render_menu_item("view_front", "Front", true);
        render_menu_item("view_top", "Top", true);
        render_menu_item("view_default", "Default", true);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About"))
            gui_open_popup("About", 0, NULL, about_popup);
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
    ImGui::PopStyleColor(2);
}

static bool render_tab(const char *label, int icon, bool *v)
{
    bool ret;
    const theme_t *theme = theme_get();
    ImVec2 center;
    ImVec2 uv0, uv1; // The position in the icon texture.
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    ImGui::PushStyleColor(ImGuiCol_Button, COLOR(TAB, INNER, *v));
    ImGui::PushID(label);
    ret = ImGui::InvisibleButton("", ImVec2(theme->sizes.icons_height,
                                            theme->sizes.icons_height));
    ImGui::GoxBox(ImGui::GetItemRectMin(), ImGui::GetItemRectSize(),
                 false, 0x05);
    ImGui::PopStyleColor();

    center = (ImGui::GetItemRectMin() + ImGui::GetItemRectMax()) / 2;
    center.y += 0.5;
    uv0 = ImVec2(((icon - 1) % 8) / 8.0, ((icon - 1) / 8) / 8.0);
    uv1 = uv0 + ImVec2(1. / 8, 1. / 8);
    window->DrawList->AddImage((void*)(intptr_t)g_tex_icons->tex,
                               center - ImVec2(16, 16),
                               center + ImVec2(16, 16),
                               uv0, uv1, get_icon_color(icon, 0));

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", label);
        goxel_set_help_text(label);
    }

    ImGui::PopID();
    return ret;
}

static int render_mode_select(void)
{
    int i;
    bool v;
    struct {
        int        mode;
        const char *name;
        int        icon;
    } values[] = {
        {MODE_OVER,   "Add",  ICON_MODE_ADD},
        {MODE_SUB,    "Sub",  ICON_MODE_SUB},
        {MODE_PAINT,  "Paint", ICON_MODE_PAINT},
    };
    gui_group_begin(NULL);
    for (i = 0; i < (int)ARRAY_SIZE(values); i++) {
        v = goxel.painter.mode == values[i].mode;
        if (gui_selectable_icon(values[i].name, &v, values[i].icon)) {
            goxel.painter.mode = values[i].mode;
        }
        auto_grid(ARRAY_SIZE(values), i, 4);
    }
    gui_group_end();
    return 0;
}


static void render_top_bar(void)
{
    gui_action_button("undo", NULL, 0, "");
    gui_same_line();
    gui_action_button("redo", NULL, 0, "");
    gui_same_line();
    gui_action_button("layer_clear", NULL, 0, "");
    gui_same_line();
    render_mode_select();
    gui_same_line();
    gui_color("##color", goxel.painter.color);
}

static void render_left_panel(void)
{
    int i;
    const theme_t *theme = theme_get();
    float left_pane_width;
    const struct {
        const char *name;
        int icon;
        void (*fn)(void);
    } PANELS[] = {
        {NULL},
        {"Tools", ICON_TOOLS, gui_tools_panel},
        {"Palette", ICON_PALETTE, gui_palette_panel},
        {"Layers", ICON_LAYERS, gui_layers_panel},
        {"View", ICON_VIEW, gui_view_panel},
        {"Material", ICON_MATERIAL, gui_material_panel},
        {"Cameras", ICON_CAMERA, gui_cameras_panel},
        {"Image", ICON_IMAGE, gui_image_panel},
        {"Render", ICON_RENDER, gui_render_panel},
        {"Debug", ICON_DEBUG, debug_panel},
    };

    left_pane_width = (gui->current_panel ? gui->panel_width : 0) +
                       gui->panel_adjust_w + theme->sizes.icons_height + 4;
    ImGui::BeginChild("left pane", ImVec2(left_pane_width, 0), true);
    gui->panel_width = GUI_PANEL_WIDTH_NORMAL;

    // Small hack to adjust the size if the scrolling bar is visible.
    gui->panel_adjust_w = left_pane_width - ImGui::GetContentRegionAvailWidth();

    ImGui::BeginGroup();

    for (i = 1; i < (int)ARRAY_SIZE(PANELS); i++) {
        bool b = (gui->current_panel == (int)i);
        if (render_tab(PANELS[i].name, PANELS[i].icon, &b)) {
            on_click();
            if (i != gui->current_panel) {
                gui->current_panel = i;
            } else {
                gui->current_panel = 0;
            }
        }
    }
    ImGui::EndGroup();
    if (gui->current_panel) {
        gui_same_line();
        ImGui::BeginGroup();
        goxel.show_export_viewport = false;
        ImGui::PushID("panel");
        ImGui::PushID(PANELS[gui->current_panel].name);
        PANELS[gui->current_panel].fn();
        ImGui::PopID();
        ImGui::PopID();
        ImGui::EndGroup();
    }
    ImGui::EndChild();
}

static void render_popups(int index)
{
    typeof(gui->popup[0]) *popup;
    ImGuiIO& io = ImGui::GetIO();

    popup = &gui->popup[index];
    if (!popup->title) return;

    if (!popup->opened) {
        ImGui::OpenPopup(popup->title);
        popup->opened = true;
    }
    int flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;
    if (popup->flags & GUI_POPUP_FULL) {
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 40,
                                        io.DisplaySize.y - 40),
                (popup->flags & GUI_POPUP_RESIZE) ?  ImGuiCond_Once : 0);
    }
    if (popup->flags & GUI_POPUP_RESIZE) {
        flags &= ~(ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_AlwaysAutoResize);
    }
    if (ImGui::BeginPopupModal(popup->title, NULL, flags)) {
        typeof(popup->func) func = popup->func;
        if (func(popup->data)) {
            ImGui::CloseCurrentPopup();
            gui->popup_count--;
            popup->title = NULL;
            popup->func = NULL;
            free(popup->data);
            popup->data = NULL;
            if (popup->on_closed) popup->on_closed();
            popup->on_closed = NULL;
            popup->opened = false;
        }
        render_popups(index + 1);
        ImGui::EndPopup();
    }
}

void gui_iter(const inputs_t *inputs)
{
    if (!gui) gui_init(inputs);
    unsigned int i;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw_list;
    ImGuiStyle& style = ImGui::GetStyle();
    const theme_t *theme = theme_get();
    gesture_t *gestures[] = {&gui->gestures.drag, &gui->gestures.hover};
    float display_rect[4] = {
        0.f, 0.f, (float)goxel.screen_size[0], (float)goxel.screen_size[1]};
    float font_size = ImGui::GetFontSize();

    io.DisplaySize = ImVec2((float)goxel.screen_size[0],
                            (float)goxel.screen_size[1]);

    io.DisplayFramebufferScale = ImVec2(goxel.screen_scale,
                                        goxel.screen_scale);
    io.DeltaTime = 1.0 / 60;
    if (inputs) {
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
        memset((void*)inputs->chars, 0, sizeof(inputs->chars));
    }

    // Setup theme.
    style.FramePadding = ImVec2(theme->sizes.item_padding_h,
                                (theme->sizes.item_height - font_size) / 2);
    style.FrameRounding = theme->sizes.item_rounding;
    style.ItemSpacing = ImVec2(theme->sizes.item_spacing_h,
                               theme->sizes.item_spacing_v);
    style.ItemInnerSpacing = ImVec2(theme->sizes.item_inner_spacing_h, 0);
    style.ScrollbarSize = theme->sizes.item_height;
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 0;

    style.Colors[ImGuiCol_WindowBg] = COLOR(BASE, BACKGROUND, 0);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.38, 0.38, 0.38, 1.0);
    style.Colors[ImGuiCol_Header] = style.Colors[ImGuiCol_WindowBg];
    style.Colors[ImGuiCol_Text] = COLOR(BASE, TEXT, 0);
    style.Colors[ImGuiCol_Button] = COLOR(BASE, INNER, 0);
    style.Colors[ImGuiCol_FrameBg] = COLOR(BASE, INNER, 0);
    style.Colors[ImGuiCol_PopupBg] = COLOR(BASE, BACKGROUND, 0);
    style.Colors[ImGuiCol_ButtonActive] = COLOR(BASE, INNER, 1);
    style.Colors[ImGuiCol_ButtonHovered] =
        color_lighten(COLOR(BASE, INNER, 0), 1.2);
    style.Colors[ImGuiCol_CheckMark] = COLOR(WIDGET, INNER, 1);
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
    render_top_bar();

    goxel.no_edit = gui->popup_count;
    render_left_panel();
    gui_same_line();

    render_popups(0);

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
    if (    inputs &&
            !(!gui->mouse_in_view && inputs->mouse_wheel) &&
            !gui->capture_mouse) {
        inputs_t inputs2 = *inputs;
        for (i = 0; i < ARRAY_SIZE(inputs->touches); i++) {
            inputs2.touches[i].pos[1] =
                io.DisplaySize.y - inputs2.touches[i].pos[1];
        }
        goxel_mouse_in_view(view_rect, &inputs2, !io.WantCaptureKeyboard);
    }

    render_axis_arrows(view_size);
    ImGui::Text("%s", goxel.hint_text ?: "");
    ImGui::SameLine(180);
    ImGui::Text("%s", goxel.help_text ?: "");

    ImGui::EndChild();
    ImGui::End();

    // Handle the shortcuts.  XXX: this should be done with actions.
    if (ImGui::IsKeyPressed(KEY_DELETE, false))
        action_exec2("layer_clear", "");

    if (!io.WantCaptureKeyboard) {
        float last_tool_radius = goxel.tool_radius;
        if (isCharPressed('[')) goxel.tool_radius -= 0.5;
        if (isCharPressed(']')) goxel.tool_radius += 0.5;
        if (goxel.tool_radius != last_tool_radius) {
            goxel.tool_radius = clamp(goxel.tool_radius, 0.5, 64);
        }
        actions_iter(check_action_shortcut, NULL);
    }
    ImGui::EndFrame();
}

void gui_render(void)
{
    ImGui::Render();
    ImImpl_RenderDrawLists(ImGui::GetDrawData());
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

    action = action_get(id, true);
    assert(action);
    PushID(action->id);
    ret = gui_button(label, size, action->icon);
    if (IsItemHovered()) goxel_set_help_text(action_get(id, true)->help);
    if (ret) {
        va_start(ap, sig);
        action_execv(action_get(id, true), sig, ap);
        va_end(ap);
    }
    PopID();
    return ret;
}

bool gui_action_checkbox(const char *id, const char *label)
{
    bool b;
    const action_t *action = action_get(id, true);
    action_exec(action, ">b", &b);
    if (ImGui::Checkbox(label, &b)) {
        action_exec(action, "b", b);
        return true;
    }
    if (ImGui::IsItemHovered()) {
        if (!*action->shortcut)
            goxel_set_help_text(action->help);
        else
            goxel_set_help_text("%s (%s)", action->help, action->shortcut);
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
            center.y += 0.5;
            uv0 = ImVec2(((icon - 1) % 8) / 8.0, ((icon - 1) / 8) / 8.0);
            uv1 = uv0 + ImVec2(1. / 8, 1. / 8);
            window->DrawList->AddImage((void*)(intptr_t)g_tex_icons->tex,
                                       center - ImVec2(16, 16),
                                       center + ImVec2(16, 16),
                                       uv0, uv1, get_icon_color(icon, *v));
        }
    } else {
        ret = ImGui::Button(label, size);
    }
    ImGui::PopStyleColor(3);
    if (ret) *v = !*v;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
        goxel_set_help_text(tooltip);
    }
    ImGui::PopID();

    if (ret) on_click();
    return ret;
}

bool gui_selectable(const char *name, bool *v, const char *tooltip, float w)
{
    return _selectable(name, v, tooltip, w, -1);
}

bool gui_selectable_toggle(const char *name, int *v, int set_v,
                           const char *tooltip, float w)
{
    bool b = *v == set_v;
    if (gui_selectable(name, &b, tooltip, w)) {
        if (b) *v = set_v;
        return true;
    }
    return false;
}

bool gui_selectable_icon(const char *name, bool *v, int icon)
{
    return _selectable(name, v, NULL, 0, icon);
}

float gui_get_avail_width(void)
{
    return GetContentRegionAvailWidth();
}

void gui_text(const char *label, ...)
{
    va_list args;
    va_start(args, label);
    TextV(label, args);
    va_end(args);
}

void gui_same_line(void)
{
    SameLine();
}

bool gui_color(const char *label, uint8_t color[4])
{
    static uint8_t backup_color[4];
    ImVec2 size;
    const theme_t *theme = theme_get();

    size.x = size.y = theme->sizes.icons_button_size;
    ImGui::PushID(label);
    if (ImGui::ColorButton(label, color, 0, size))
        memcpy(backup_color, color, 4);
    if (ImGui::BeginPopupContextItem("color context menu", 0)) {
        color_edit("##edit", color, backup_color);
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (label && label[0] != '#') {
        gui_same_line();
        ImGui::Text("%s", label);
    }
    ImGui::PopID();
    return false;
}

bool gui_color_small(const char *label, uint8_t color[4])
{
    ImVec4 c = color;
    ImGui::PushID(label);
    ImGui::ColorButton(label, c);
    if (ImGui::BeginPopupContextItem("color context menu", 0)) {
        color_edit("##edit", color, NULL);
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    gui_same_line();
    ImGui::Text("%s", label);
    ImGui::PopID();
    return false;
}

bool gui_checkbox(const char *label, bool *v, const char *hint)
{
    bool ret;
    const theme_t *theme = theme_get();
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    // Checkbox inside a group box have a plain background.
    if (gui->group) {
        GoxBox2(ImGui::GetCursorScreenPos(),
                ImVec2(0, theme->sizes.item_height),
                COLOR(WIDGET, INNER, 0), true, 0);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,
                color_lighten(style.Colors[ImGuiCol_FrameBg], 1.2));
    }
    ret = Checkbox(label, v);
    if (gui->group) ImGui::PopStyleColor();
    if (hint && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hint);
    if (ret) on_click();
    return ret;
}

bool gui_checkbox_flag(const char *label, int *v, int flag, const char *hint)
{
    bool ret, b;
    b = (*v) & flag;
    ret = gui_checkbox(label, &b, hint);
    if (ret) {
        if (b) *v |= flag;
        else   *v &= ~flag;
    }
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
    int w, isize;

    button_size = ImVec2(size * GetContentRegionAvailWidth(),
                         theme->sizes.item_height);
    if (size == -1) button_size.x = GetContentRegionAvailWidth();
    if (size == 0 && (label == NULL || label[0] == '#')) {
        button_size.x = theme->sizes.icons_button_size;
        button_size.y = theme->sizes.icons_button_size;
    }
    if (size == 0 && label && label[0] != '#') {
        w = CalcTextSize(label, NULL, true).x +
            theme->sizes.item_padding_h * 2;
        if (w < theme->sizes.item_height)
            button_size.x = theme->sizes.item_height;
    }
    isize = (label && label[0] != '#' && label) ? 12 : 16;
    label = label ?: "";
    ret = Button(label, button_size);
    if (icon) {
        center = GetItemRectMin() + ImVec2(GetItemRectSize().y / 2,
                                           GetItemRectSize().y / 2);
        uv0 = ImVec2(((icon - 1) % 8) / 8.0, ((icon - 1) / 8) / 8.0);
        uv1 = ImVec2(uv0.x + 1. / 8, uv0.y + 1. / 8);
        draw_list->AddImage((void*)(intptr_t)g_tex_icons->tex,
                            center - ImVec2(isize, isize),
                            center + ImVec2(isize, isize),
                            uv0, uv1, get_icon_color(icon, 0));
    }
    if (ret) on_click();
    return ret;
}

bool gui_button_right(const char *label, int icon)
{
    const theme_t *theme = theme_get();
    float text_size = ImGui::CalcTextSize(label).x;
    float w = text_size + 2 * theme->sizes.item_padding_h;
    w = max(w, theme->sizes.item_height);
    w += theme->sizes.item_padding_h;
    gui_same_line();
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvailWidth() - w, 0));
    gui_same_line();
    return gui_button(label, 0, icon);
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
    return ret;
}

bool gui_combo(const char *label, int *v, const char **names, int nb)
{
    const theme_t *theme = theme_get();
    bool ret;
    float font_size = ImGui::GetFontSize();

    ImGui::PushItemWidth(-1);
    PushStyleVar(ImGuiStyleVar_ItemSpacing,
                 ImVec2(0, (theme->sizes.item_height - font_size) / 2));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, COLOR(WIDGET, INNER, 0));
    ret = Combo(label, v, names, nb);
    PopStyleVar();
    PopStyleColor();
    ImGui::PopItemWidth();
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

void gui_open_popup(const char *title, int flags, void *data,
                    bool (*func)(void *data))
{
    typeof(gui->popup[0]) *popup;
    popup = &gui->popup[gui->popup_count++];
    popup->title = title;
    popup->func = func;
    popup->flags = flags;
    assert(!popup->data);
    popup->data = data;
}

void gui_on_popup_closed(void (*func)(void))
{
    gui->popup[gui->popup_count - 1].on_closed = func;
}

void gui_popup_body_begin(void) {
    ImGui::BeginChild("body", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
}

void gui_popup_body_end(void) {
    ImGui::EndChild();
}

void gui_alert(const char *title, const char *msg)
{
    gui_open_popup(title, 0, msg ? strdup(msg) : NULL, alert_popup);
}

bool gui_collapsing_header(const char *label, bool default_opened)
{
    if (default_opened)
        ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
    return ImGui::CollapsingHeader(label);
}

void gui_columns(int count)
{
    ImGui::Columns(count);
}

void gui_next_column(void)
{
    ImGui::NextColumn();
}

void gui_separator(void)
{
    ImGui::Separator();
}

void gui_push_id(const char *id)
{
    ImGui::PushID(id);
}

void gui_pop_id(void)
{
    ImGui::PopID();
}

void gui_floating_icon(int icon)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 uv0, uv1, pos;
    uv0 = ImVec2(((icon - 1) % 8) / 8.0, ((icon - 1) / 8) / 8.0);
    uv1 = ImVec2(uv0.x + 1. / 8, uv0.y + 1. / 8);
    pos = GetItemRectMin();
    draw_list->AddImage((void*)(intptr_t)g_tex_icons->tex,
            pos, pos + ImVec2(32, 32),
            uv0, uv1, get_icon_color(icon, 0));
}

void gui_request_panel_width(float width)
{
    gui->panel_width = width;
}

bool gui_layer_item(int i, int icon, bool *visible, bool *edit,
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
        gui_same_line();
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
                    uv0, uv1, get_icon_color(icon, 0));
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

bool gui_is_key_down(int key)
{
    return ImGui::IsKeyDown(key);
}

bool gui_palette_entry(const uint8_t color[4], uint8_t target[4])
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
    if (ret) {
        on_click();
        memcpy(target, color, 4);
    }
    return ret;
}

void gui_get_view_rect(float rect[4])
{
    memcpy(rect, gui->view.rect, sizeof(gui->view.rect));
}

}
