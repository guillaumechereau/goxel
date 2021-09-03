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

#ifndef GUI_HAS_MENU
#   define GUI_HAS_MENU 1
#endif

#ifndef GUI_HAS_SCROLLBARS
#   define GUI_HAS_SCROLLBARS 1
#endif

extern "C" {
#include "goxel.h"

void gui_app(void);
void gui_render_panel(void);
void gui_menu(void);
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

// Prevent warnings with gcc.
#ifndef __clang__
#pragma GCC diagnostic push
#if __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

#include "../ext_src/imgui/imgui.h"
#include "../ext_src/imgui/imgui_internal.h"

#ifndef __clang__
#pragma GCC diagnostic pop
#endif

static ImVec4 imvec4(const uint8_t v[4])
{
    return ImVec4(v[0] / 255., v[1] / 255., v[2] / 255., v[3] / 255.);
}

static inline ImVec4 color_lighten(ImVec4 c, float k)
{
    c.x *= k;
    c.y *= k;
    c.z *= k;
    return c;
}

namespace ImGui {
    void GoxBox2(ImVec2 pos, ImVec2 size, ImVec4 color, bool fill,
                 float thickness = 1,
                 int rounding_corners_flags = ~0)
    {
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        ImGuiWindow* window = GetCurrentWindow();
        float r = style.FrameRounding;
        size.x = size.x ?: ImGui::GetContentRegionAvail().x;
        if (fill) {
            window->DrawList->AddRectFilled(
                    pos, pos + size,
                    ImGui::ColorConvertFloat4ToU32(color), r,
                    rounding_corners_flags);
        } else {
            window->DrawList->AddRect(
                    pos, pos + size,
                    ImGui::ColorConvertFloat4ToU32(color), r,
                    rounding_corners_flags, thickness);
        }
    }

    void GoxBox(ImVec2 pos, ImVec2 size, bool selected,
                int rounding_corners_flags = ~0)
    {
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        ImVec4 color  = style.Colors[selected ? ImGuiCol_ButtonActive :
                                     ImGuiCol_Button];
        return GoxBox2(pos, size, color, true, 1, rounding_corners_flags);
    }

    bool GoxInputFloat(const char *label, float *v, float step,
                       float minv, float maxv, const char *format)
    {
        const theme_t *theme = theme_get();
        bool ret = false;
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImVec2 button_sz = ImVec2(
                max(g.FontSize * 2.0f, theme->sizes.item_height),
                g.FontSize + style.FramePadding.y * 2.0f);
        int button_flags =
            ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;
        float speed = step / 20;
        uint8_t color[4];
        char buf[128];
        bool input_active;
        ImVec4 text_color;

        ImGui::PushID(label);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));

        // XXX: commented out for the moment so that diabled input work.
        // theme_get_color(THEME_GROUP_WIDGET, THEME_COLOR_TEXT, 0, color);
        // ImGui::PushStyleColor(ImGuiCol_Text, imvec4(color));

        theme_get_color(THEME_GROUP_WIDGET, THEME_COLOR_INNER, 0, color);
        ImGui::PushStyleColor(ImGuiCol_Button, imvec4(color));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, imvec4(color));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
            color_lighten(imvec4(color), 1.2));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            color_lighten(imvec4(color), 1.2));

        ImGui::SetWindowFontScale(0.75);
        if (ImGui::ButtonEx("◀", button_sz, button_flags)) {
            (*v) -= step;
            ret = true;
        }
        ImGui::SetWindowFontScale(1);

        ImGui::SameLine();
        ImGui::PushItemWidth(
                ImGui::GetContentRegionAvail().x -
                button_sz.x - style.ItemSpacing.x);

        input_active = ImGui::TempInputTextIsActive(ImGui::GetID(""));
        text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        if (!input_active)
            text_color = ImVec4(0, 0, 0, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ret |= ImGui::DragFloat("", v, speed, minv, maxv, format, 1.0);
        ImGui::PopStyleColor();

        if (!input_active) {
            snprintf(buf, sizeof(buf), "%s:", label);
            ImGui::RenderTextClipped(ImGui::GetItemRectMin(),
                                     ImGui::GetItemRectMax(),
                                     buf, NULL, NULL, ImVec2(0, 0.5));
            snprintf(buf, sizeof(buf), format, *v);
            ImGui::RenderTextClipped(ImGui::GetItemRectMin(),
                                     ImGui::GetItemRectMax(),
                                     buf, NULL, NULL, ImVec2(1, 0.5));
        }

        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::SetWindowFontScale(0.75);
        if (ImGui::ButtonEx("▶", button_sz, button_flags)) {
            (*v) += step;
            ret = true;
        }
        ImGui::SetWindowFontScale(1);

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        ImGui::PopID();

        if (ret)
            *v = clamp(*v, minv, maxv);

        return ret;
    }
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
    void    *user;
    void    (*render)(void *user, const float viewport[4]);
} view_t;

enum {
    A_POS_LOC = 0,
    A_TEX_POS_LOC,
    A_COLOR_LOC,
};

static const char *ATTR_NAMES[] = {
    [A_POS_LOC]         = "a_pos",
    [A_TEX_POS_LOC]     = "a_tex_pos",
    [A_COLOR_LOC]       = "a_color",
    NULL
};

typedef typeof(((inputs_t*)0)->safe_margins) margins_t;

typedef struct gui_t {
    gl_shader_t *shader;
    GLuint  array_buffer;
    GLuint  index_buffer;
    const inputs_t *inputs;
    view_t  view;
    struct {
        gesture_t drag;
        gesture_t hover;
    }       gestures;
    bool    capture_mouse; // Mouse is captured by a window.
    int     group;
    margins_t margins;
    bool    is_scrolling;

    struct {
        const char *title;
        int       (*func)(void *data);
        void      (*on_closed)(int);
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
    // TODO: remove this function if possible.
    ImGuiContext& g = *GImGui;
    if (g.IO.InputQueueCharacters.Size == 0) return false;
    return g.IO.InputQueueCharacters[0] == c;
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
    GL(glUseProgram(gui->shader->prog));
    gl_update_uniform(gui->shader, "u_tex", 0);
    gl_update_uniform(gui->shader, "u_proj_mat", ortho_projection);

    GL(glBindBuffer(GL_ARRAY_BUFFER, gui->array_buffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gui->index_buffer));
    // This could probably be done only at init time.
    GL(glEnableVertexAttribArray(A_POS_LOC));
    GL(glEnableVertexAttribArray(A_TEX_POS_LOC));
    GL(glEnableVertexAttribArray(A_COLOR_LOC));
    GL(glVertexAttribPointer(A_POS_LOC, 2, GL_FLOAT, false,
                             sizeof(ImDrawVert),
                             (void*)OFFSETOF(ImDrawVert, pos)));
    GL(glVertexAttribPointer(A_TEX_POS_LOC, 2, GL_FLOAT, false,
                             sizeof(ImDrawVert),
                             (void*)OFFSETOF(ImDrawVert, uv)));
    GL(glVertexAttribPointer(A_COLOR_LOC, 4, GL_UNSIGNED_BYTE,
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

    float scale = goxel.screen_scale;
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
    GL(glGenTextures(1, &tex_id));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, tex_id));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels));
    io.Fonts->TexID = (void *)(intptr_t)tex_id;
}

static void init_ImGui(void)
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f/60.0f;
    io.IniFilename = NULL;

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

    if (DEFINED(__linux__)) {
        io.SetClipboardTextFn = sys_set_clipboard_text;
        io.GetClipboardTextFn = sys_get_clipboard_text;
    }
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

    if (DEFINED(GOXEL_MOBILE) && gest->type == GESTURE_HOVER) return 0;
    io.MousePos = ImVec2(gest->pos[0], gest->pos[1]);
    io.MouseDown[0] = (gest->type == GESTURE_DRAG) &&
                      (gest->state != GESTURE_END);

    if (gest->state == GESTURE_END || gest->type == GESTURE_HOVER) {
        gui->capture_mouse = false;
    }
    if (gest->state == GESTURE_END && gest->type != GESTURE_HOVER) {
        gui_iter(NULL);
        io.MousePos = ImVec2(-1, -1);
    }
    return 0;
}


static void gui_init(void)
{
    if (!gui) {
        gui = (gui_t*)calloc(1, sizeof(*gui));
        init_ImGui();
        gui->gestures.drag.type = GESTURE_DRAG;
        gui->gestures.drag.button = GESTURE_LMB;
        gui->gestures.drag.callback = on_gesture;
        gui->gestures.hover.type = GESTURE_HOVER;
        gui->gestures.hover.callback = on_gesture;
        goxel.gui.panel_width = GUI_PANEL_WIDTH_NORMAL;
    }

    if (!gui->shader) {
        gui->shader = gl_shader_create(VSHADER, FSHADER, NULL, ATTR_NAMES);
        GL(glGenBuffers(1, &gui->array_buffer));
        GL(glGenBuffers(1, &gui->index_buffer));
    }

    if (!g_tex_icons) {
        g_tex_icons = texture_new_image("asset://data/images/icons.png", 0);
        GL(glBindTexture(GL_TEXTURE_2D, g_tex_icons->tex));
    }

    ImGuiIO& io = ImGui::GetIO();
    if (!io.Fonts->TexID) load_fonts_texture();
}

void gui_release(void)
{
    if (gui) ImGui::DestroyContext();
}

void gui_release_graphics(void)
{
    ImGuiIO& io = ImGui::GetIO();
    gl_shader_delete(gui->shader);
    gui->shader = NULL;
    GL(glDeleteBuffers(1, &gui->array_buffer));
    GL(glDeleteBuffers(1, &gui->index_buffer));
    texture_delete(g_tex_icons);
    g_tex_icons = NULL;

    GL(glDeleteTextures(1, (GLuint*)&io.Fonts->TexID));
    io.Fonts->TexID = 0;
    io.Fonts->Clear();
}

static int alert_popup(void *data)
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
    } else {
        if (str_startswith(s, "Ctrl")) return 0;
    }
    if (io.KeyShift) {
        check_key = false;
    }
    if (    (check_char && isCharPressed(s[0])) ||
            (check_key && ImGui::IsKeyPressed(s[0], false))) {
        action_exec(action);
        return 1;
    }
    return 0;
}

static void render_menu(void)
{
    ImGui::PushStyleColor(ImGuiCol_PopupBg, COLOR(MENU, INNER, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, COLOR(MENU, TEXT, 0));

    if (!ImGui::BeginMenuBar()) return;
    gui_menu();
    ImGui::EndMenuBar();
    ImGui::PopStyleColor(2);
}

static void render_popups(int index)
{
    int r;
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    if (ImGui::BeginPopupModal(popup->title, NULL, flags)) {
        typeof(popup->func) func = popup->func;
        if ((r = func(popup->data))) {
            ImGui::CloseCurrentPopup();
            gui->popup_count--;
            popup->title = NULL;
            popup->func = NULL;
            free(popup->data);
            popup->data = NULL;
            if (popup->on_closed) popup->on_closed(r);
            popup->on_closed = NULL;
            popup->opened = false;
        }
        render_popups(index + 1);
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void gui_iter(const inputs_t *inputs)
{
    gui_init();
    unsigned int i;
    ImGuiIO& io = ImGui::GetIO();
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
    io.DeltaTime = goxel.delta_time;
    gui->inputs = inputs;

    if (inputs) {
        io.DisplayFramebufferScale = ImVec2(inputs->scale, inputs->scale);
        io.FontGlobalScale = 1 / inputs->scale;
        gui->margins = inputs->safe_margins;
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
    style.GrabMinSize = theme->sizes.item_height;
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 0;
    style.WindowRounding = 0;
    style.WindowPadding = ImVec2(4, 4);

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
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                                    (GUI_HAS_MENU ? ImGuiWindowFlags_MenuBar : 0);

    ImGui::SetNextWindowSize(ImVec2(
        io.DisplaySize.x - (gui->margins.left + gui->margins.right),
        io.DisplaySize.y - gui->margins.top));
    ImGui::SetNextWindowPos(ImVec2(gui->margins.left, gui->margins.top));
    ImGui::Begin("Goxel", NULL, window_flags);

    render_popups(0);
    goxel.no_edit = gui->popup_count;
    if (gui->popup_count) gui->capture_mouse = true;

    if (GUI_HAS_MENU) render_menu();

    gui_app();

    ImGui::End();

    // Handle the shortcuts.  XXX: this should be done with actions.
    if (ImGui::IsKeyPressed(KEY_DELETE, false))
        action_exec2(ACTION_layer_clear);

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

    sys_show_keyboard(io.WantTextInput);
}

void gui_render(void)
{
    gui_init();
    ImGui::Render();
    ImImpl_RenderDrawLists(ImGui::GetDrawData());
}

extern "C" {
using namespace ImGui;

static void reset_context_callback(const ImDrawList* parent_list,
                                   const ImDrawCmd* cmd)
{
    // Do nothing.
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
    GoxBox2(pos, size, COLOR(WIDGET, OUTLINE, 0), true);

    draw_list->ChannelsMerge();
    draw_list->AddCallback(reset_context_callback, NULL);
    GoxBox2(pos, size, COLOR(WIDGET, OUTLINE, 0), false);
}

void gui_div_begin(void)
{
    ImGui::BeginGroup();
}

void gui_div_end(void)
{
    ImGui::EndGroup();
}

void gui_child_begin(const char *id, float w, float h)
{
    ImGui::BeginChild(id, ImVec2(w, h), false,
                      ImGuiWindowFlags_NoScrollWithMouse);
}

void gui_child_end(void)
{
    ImGui::EndChild();
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

bool gui_action_button(int id, const char *label, float size)
{
    bool ret;
    const action_t *action;

    action = action_get(id, true);
    assert(action);
    PushID(action->id);
    ret = gui_button(label, size, action->icon);
    if (IsItemHovered()) goxel_set_help_text(action_get(id, true)->help);
    if (ret) {
        action_exec(action_get(id, true));
    }
    PopID();
    return ret;
}

static bool _selectable(const char *label, bool *v, const char *tooltip,
                        float w, int icon, int group)
{
    const theme_t *theme = theme_get();
    ImGuiWindow* window = GetCurrentWindow();
    ImVec2 size;
    ImVec2 center;
    bool ret = false;
    bool default_v = false;
    ImVec2 uv0, uv1; // The position in the icon texture.
    uint8_t color[4];

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
    if (!*v && group == THEME_GROUP_TAB) color[3] = 0;
    PushStyleColor(ImGuiCol_Button, color);
    theme_get_color(group, THEME_COLOR_INNER, *v, color);
    PushStyleColor(ImGuiCol_ButtonHovered, color_lighten(color, 1.2));
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
        gui_tooltip(tooltip);
        goxel_set_help_text(tooltip);
    }
    ImGui::PopID();

    if (ret) on_click();
    return ret;
}

bool gui_selectable(const char *name, bool *v, const char *tooltip, float w)
{
    return _selectable(name, v, tooltip, w, -1, THEME_GROUP_WIDGET);
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
    return _selectable(name, v, NULL, 0, icon, THEME_GROUP_WIDGET);
}

float gui_get_avail_width(void)
{
    return ImGui::GetContentRegionAvail().x;
}

void gui_text(const char *label, ...)
{
    va_list args;
    va_start(args, label);
    TextV(label, args);
    va_end(args);
}

void gui_text_wrapped(const char *label, ...)
{
    va_list args;
    ImGui::PushTextWrapPos(0);
    va_start(args, label);
    TextV(label, args);
    va_end(args);
    ImGui::PopTextWrapPos();
}

void gui_same_line(void)
{
    SameLine();
}

void gui_spacing(int w)
{
    ImGui::Dummy(ImVec2(w, 0));
    ImGui::SameLine();
}

bool gui_color(const char *label, uint8_t color[4])
{
    static uint8_t backup_color[4];
    ImVec2 size;
    const theme_t *theme = theme_get();

    size.x = size.y = theme->sizes.icons_height;
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
    uint8_t orig[4];
    memcpy(orig, color, 4);
    ImVec4 c = color;
    ImGui::PushID(label);
    ImGui::ColorButton(label, c);
    if (ImGui::BeginPopupContextItem("color context menu", 0)) {
        color_edit("##edit", color, NULL);
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    gui_same_line();
    ImGui::Text("%s", label);
    ImGui::PopID();
    return memcmp(color, orig, 4) != 0;
}

bool gui_color_small_f3(const char *label, float color[3])
{
    uint8_t c[4] = {(uint8_t)(color[0] * 255),
                    (uint8_t)(color[1] * 255),
                    (uint8_t)(color[2] * 255),
                    255};
    bool ret = gui_color_small(label, c);
    color[0] = c[0] / 255.;
    color[1] = c[1] / 255.;
    color[2] = c[2] / 255.;
    return ret;
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
    if (hint && ImGui::IsItemHovered()) gui_tooltip(hint);
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

    button_size = ImVec2(size * GetContentRegionAvail().x,
                         theme->sizes.item_height);
    if (size == -1) button_size.x = GetContentRegionAvail().x;
    if (size == 0 && (label == NULL || label[0] == '#')) {
        button_size.x = theme->sizes.icons_height;
        button_size.y = theme->sizes.icons_height;
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
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - w, 0));
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

bool gui_combo_begin(const char *label, const char *preview)
{
    bool ret;
    const theme_t *theme = theme_get();
    float font_size = ImGui::GetFontSize();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(0, (theme->sizes.item_height - font_size) / 2));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, COLOR(WIDGET, INNER, 0));
    ImGui::PushItemWidth(-1);
    ret = ImGui::BeginCombo(label, preview);

    if (!ret) {
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
    return ret;
}

void gui_combo_end(void)
{
    ImGui::EndCombo();
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

bool gui_combo_item(const char *label, bool is_selected)
{
    bool ret;
    ret = ImGui::Selectable(label, is_selected);
    if (is_selected)
        ImGui::SetItemDefaultFocus();
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
                    int (*func)(void *data))
{
    typeof(gui->popup[0]) *popup;
    popup = &gui->popup[gui->popup_count++];
    popup->title = title;
    popup->func = func;
    popup->flags = flags;
    assert(!popup->data);
    popup->data = data;
}

void gui_on_popup_closed(void (*func)(int))
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
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
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

void gui_bottom_text(const char *txt)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos, size, text_size;
    float wrap;

    pos = GetItemRectMin();
    size = GetItemRectSize();
    wrap = size.x - 8;
    text_size = CalcTextSize(txt, NULL, false, wrap);

    draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                       ImVec2(pos.x + 4, pos.y + size.y - text_size.y - 4),
                       ImGui::GetColorU32(COLOR(WIDGET, TEXT, false)),
                       txt, NULL, wrap);
}

void gui_request_panel_width(float width)
{
    goxel.gui.panel_width = width;
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

bool gui_menu_begin(const char *label)
{
    bool ret = ImGui::BeginMenu(label);
    if (ret) gui->capture_mouse = true;
    return ret;
}

void gui_menu_end(void)
{
    return ImGui::EndMenu();
}

bool gui_menu_item(int action, const char *label, bool enabled)
{
    const action_t *a = NULL;
    if (action) {
        a = action_get(action, true);
        assert(a);
    }
    if (ImGui::MenuItem(label, a ? a->shortcut : NULL, false, enabled)) {
        if (a) action_exec(a);
        return true;
    }
    return false;
}

void gui_scrollable_begin(int width)
{
    ImGuiWindowFlags flags = 0;

    if (!GUI_HAS_SCROLLBARS) {
        if (gui->is_scrolling && !ImGui::IsAnyMouseDown())
            gui->is_scrolling = false;
        flags |= ImGuiWindowFlags_NoScrollbar;
        if (gui->is_scrolling) flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::BeginChild("#scroll", ImVec2(width, 0), true, flags);
    ImGui::BeginGroup();
}

void gui_scrollable_end(void)
{
    // XXX: make this attribute of the item instead, so that we can use
    // several scrollable widgets.
    static float scroll_y = 0;
    static float x, y;
    static float last_y;
    static float speed = 0;
    static int state; // 0: possible, 1: scrolling, 2: cancel.

    ImGui::EndGroup();

    if (!GUI_HAS_SCROLLBARS) {
        if (ImGui::IsItemClicked()) {
            state = 0;
            speed = 0;
            y = ImGui::GetMousePos().y;
            x = ImGui::GetMousePos().x;
            last_y = y;
            scroll_y = ImGui::GetScrollY();
        }

        if (ImGui::IsItemHovered()) {
            if (state == 0 && fabs(x - ImGui::GetMousePos().x) > 8)
                state = 2;
            if (state == 0 && fabs(y - ImGui::GetMousePos().y) > 8) {
                state = 1;
                gui->is_scrolling = true;
            }
            if (state == 1) {
                speed = mix(speed, last_y - ImGui::GetMousePos().y, 0.5);
                last_y = ImGui::GetMousePos().y;
                ImGui::ClearActiveID();
                ImGui::SetScrollY(scroll_y + y - ImGui::GetMousePos().y);
            }
        } else if (speed) {
            ImGui::SetScrollY(ImGui::GetScrollY() + speed);
            speed *= 0.95;
            if (fabs(speed) < 1) speed = 0;
        }
    }
    ImGui::EndChild();
}

void gui_tooltip(const char *str)
{
    if (gui->is_scrolling) return;
    ImGui::SetTooltip("%s", str);
}

#if !DEFINED(GOXEL_MOBILE)
bool gui_need_full_version(void)
{
    return true;
}
#endif

void gui_choice_begin(const char *label, int *value, bool small)
{
    ImGuiStorage* storage = ImGui::GetStateStorage();
    gui_group_begin(NULL);
    storage->SetVoidPtr(ImGui::GetID("choices#value"), value);
    storage->SetBool(ImGui::GetID("choices#small"), small);
}

bool gui_choice(const char *label, int idx, int icon)
{
    ImGuiStorage* storage = ImGui::GetStateStorage();
    int *value;
    bool selected, small;

    small = storage->GetBool(ImGui::GetID("choices#small"));
    value = (int*)storage->GetVoidPtr(ImGui::GetID("choices#value"));
    assert(value);

    if (storage->GetBool(ImGui::GetID("choices#change"))) {
        storage->SetBool(ImGui::GetID("choices#change"), false);
        *value = idx;
    }

    selected = (*value == idx);

    if (!small) {
        if (gui_selectable_icon(label, &selected, icon) && selected) {
            *value = idx;
            return true;
        }
        gui_same_line();
    } else {
        if (!selected) return false;
        selected = false;
        if (gui_selectable_icon(label, &selected, icon)) {
            storage->SetBool(ImGui::GetID("choices#change"), true);
        }
    }

    return false;
}

void gui_choice_end(void)
{
    gui_group_end();
}

static void gui_canvas_(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    float scale = ImGui::GetIO().DisplayFramebufferScale.y;
    view_t *view = (view_t*)cmd->UserCallbackData;
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    view->render(view->user, view->rect);
    GL(glViewport(0, 0, width * scale, height * scale));
}

void gui_canvas(float w, float h,
                inputs_t *inputs, bool *has_mouse, bool *has_keyboard,
                void *user,
                void (*render)(void *user, const float viewport[4]))
{
    int i;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImGuiIO& io = ImGui::GetIO();
    bool hovered;

    if (h < 0) canvas_size.y += h;
    vec4_set(gui->view.rect,
             canvas_pos.x,
             goxel.screen_size[1] - (canvas_pos.y + canvas_size.y),
             canvas_size.x, canvas_size.y);
    gui->view.user = user;
    gui->view.render = render;
    draw_list->AddCallback(gui_canvas_, &gui->view);
    ImGui::InvisibleButton("canvas", canvas_size);
    hovered = ImGui::IsItemHovered();

    if (    gui->inputs &&
            (hovered || !gui->inputs->mouse_wheel) &&
            !gui->capture_mouse) {
        *has_mouse = true;
        *inputs = *gui->inputs;
        for (i = 0; i < (int)ARRAY_SIZE(inputs->touches); i++) {
            inputs->touches[i].pos[1] =
                io.DisplaySize.y - inputs->touches[i].pos[1];
        }
    } else {
        *has_mouse = false;
        memset(inputs, 0, sizeof(*inputs));
    }
    *has_keyboard = !io.WantCaptureKeyboard;
}

bool gui_tab(const char *label, int icon, bool *v)
{
    return _selectable(label, v, NULL, 0, icon, THEME_GROUP_TAB);
}

bool gui_panel_header(const char *label)
{
    const theme_t *theme = theme_get();
    bool ret;
    float label_w = ImGui::CalcTextSize(label).x;
    float w = ImGui::GetContentRegionAvail().x - theme->sizes.item_height;
    gui_push_id("panel_header");
    ImGui::Dummy(ImVec2((w - label_w) / 2, 0));
    gui_same_line();
    ImGui::AlignTextToFramePadding();
    gui_text(label);
    ret = gui_button_right("", ICON_CLOSE);
    ImGui::Separator();
    gui_pop_id();
    return ret;
}


}
