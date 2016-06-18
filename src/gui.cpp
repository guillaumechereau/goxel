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

#include "imgui.h"

static inline ImVec4 IMHEXCOLOR(uint32_t v)
{
    uvec4b_t c = HEXCOLOR(v);
    return ImVec4(c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
}

namespace ImGui {
    bool GoxSelectable(const char *name, bool *v, int tex = 0, int icon = 0,
                       const char *tooltip = NULL);
    bool GoxColorEdit(const char *name, uvec4b_t *color);
    bool GoxPaletteEntry(const uvec4b_t *color, uvec4b_t *target);
    bool GoxIsCharPressed(int c);
    bool GoxCollapsingHeader(const char *label, const char *str_id = NULL,
                             bool display_frame = true,
                             bool default_open = false);
    bool GoxAction(const char *id, const char *label, const arg_t *args);
};

static texture_t *g_tex_icons = NULL;

// All the icons positions inside icon.png (as Y*8 + X).
enum {
    ICON_TOOL_BRUSH = 0,
    ICON_TOOL_CUBE = 1,
    ICON_TOOL_LASER = 2,
    ICON_TOOL_PLANE = 3,
    ICON_TOOL_MOVE = 4,
    ICON_TOOL_PICK = 5,
    ICON_TOOL_SELECTION = 6,
    ICON_TOOL_PROCEDURAL = 7,

    ICON_OP_ADD = 8,
    ICON_OP_SUB = 9,
    ICON_OP_PAINT = 10,

    ICON_SHAPE_SPHERE = 16,
    ICON_SHAPE_CUBE = 17,
    ICON_SHAPE_CYLINDER = 18,
};

static const int MiB = 1 << 20;

static ImVec4 uvec4b_to_imvec4(uvec4b_t v)
{
    return ImVec4(v.x / 255., v.y / 255., v.z / 255., v.w / 255);
}

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

    char prog_path[1024];       // "\0" if no loaded prog.
    char prog_buff[64 * 1024];  // XXX: make it dynamic?
    bool prog_export_animation;
    char prog_export_animation_path[1024];
} gui_t;

static gui_t *gui = NULL;

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
    render_prepare_context();
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;

        GL(glBufferData(GL_ARRAY_BUFFER,
                    (GLsizeiptr)cmd_list->VtxBuffer.size() * sizeof(ImDrawVert),
                    (GLvoid*)&cmd_list->VtxBuffer.front(), GL_DYNAMIC_DRAW));

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
                GL(glScissor((int)pcmd->ClipRect.x, (int)(height - pcmd->ClipRect.w),
                             (int)(pcmd->ClipRect.z - pcmd->ClipRect.x),
                             (int)(pcmd->ClipRect.w - pcmd->ClipRect.y)));
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

    unsigned char* pixels;
    int width, height;
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

static void init_ImGui()
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

    if (DEFINED(__linux__)) {
        io.SetClipboardTextFn = sys_set_clipboard_text;
        io.GetClipboardTextFn = sys_get_clipboard_text;
    }

    io.RenderDrawListsFn = ImImpl_RenderDrawLists;
    load_fonts_texture();

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 0;
    style.WindowRounding = 0;
    style.WindowFillAlphaDefault = 1.0;
    style.ItemSpacing = ImVec2(4, 4);
    style.Colors[ImGuiCol_WindowBg] = IMHEXCOLOR(0x202020FF);
    style.Colors[ImGuiCol_Header] = style.Colors[ImGuiCol_WindowBg];
    style.Colors[ImGuiCol_Text] = IMHEXCOLOR(0xD0D0D0FF);
    style.Colors[ImGuiCol_Button] = IMHEXCOLOR(0x727272FF);
    style.Colors[ImGuiCol_ButtonActive] = IMHEXCOLOR(0x6666CCFF);
    style.Colors[ImGuiCol_ButtonHovered] = IMHEXCOLOR(0x6666FFFF);
}

void gui_init(void)
{
    gui = (gui_t*)calloc(1, sizeof(*gui));
    init_prog(&gui->prog);
    GL(glGenBuffers(1, &gui->array_buffer));
    GL(glGenBuffers(1, &gui->index_buffer));
    init_ImGui();

    g_tex_icons = texture_new_image("data/icons.png");
    GL(glBindTexture(GL_TEXTURE_2D, g_tex_icons->tex));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
}

// XXX: Move this somewhere else.
void render_axis_arrows(goxel_t *goxel, const vec2_t *view_size)
{
    const vec3_t AXIS[] = {vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1)};
    int i;
    const int d = 40;  // Distance to corner of the view.
    vec2_t spos = vec2(d, d);
    vec3_t pos, normal, b;
    uvec4b_t c;
    float s = 1;
    vec4_t view = vec4(0, 0, view_size->x, view_size->y);
    camera_get_ray(&goxel->camera, &spos, &view, &pos, &normal);
    if (goxel->camera.ortho)
        s = goxel->camera.dist / 32;
    else
        vec3_iaddk(&pos, normal, 100);

    for (i = 0; i < 3; i++) {
        b = vec3_addk(pos, AXIS[i], 2.0 * s);
        c = uvec4b(AXIS[i].x * 255, AXIS[i].y * 255, AXIS[i].z * 255, 255);
        render_line(&goxel->rend, &pos, &b, &c);
    }
}

typedef struct view {
    goxel_t *goxel;
    vec4_t  rect;
} view_t;

void render_view(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    view_t *view = (view_t*)cmd->UserCallbackData;
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    int rect[4] = {view->rect.x, height - view->rect.y - view->rect.w,
                   view->rect.z, view->rect.w};
    vec4_t back_color;
    back_color = uvec4b_to_vec4(view->goxel->back_color);
    goxel_render_view(view->goxel, &view->rect);
    render_render(&view->goxel->rend, rect, &back_color);
    GL(glViewport(0, 0, width, height));
}

static void op_panel(goxel_t *goxel)
{
    int i;
    bool v;
    struct {
        int        op;
        const char *name;
        int        icon;
    } values[] = {
        {OP_ADD,    "Add",  ICON_OP_ADD},
        {OP_SUB,    "Sub",  ICON_OP_SUB},
        {OP_PAINT,  "Paint", ICON_OP_PAINT},
    };
    ImGui::Text("Operation");
    for (i = 0; i < (int)ARRAY_SIZE(values); i++) {
        v = goxel->painter.op == values[i].op;
        if (ImGui::GoxSelectable(values[i].name, &v,
                                 g_tex_icons->tex, values[i].icon)) {
            goxel->painter.op = values[i].op;
        }
        if (i != 2)
            ImGui::SameLine();
    }
}

static void shapes_panel(goxel_t *goxel);
static void tool_options_panel(goxel_t *goxel)
{
    int i;
    float v;
    bool s;
    const char *snap[] = {"Mesh", "Plane"};
    ImVec4 color;
    layer_t *layer;
    mat4_t mat;
    if (IS_IN(goxel->tool, TOOL_BRUSH, TOOL_LASER)) {
        i = goxel->tool_radius * 2;
        if (ImGui::InputInt("Size", &i, 1)) {
            i = clamp(i, 1, 128);
            goxel->tool_radius = i / 2.0;
        }
        s = goxel->painter.smoothness;
        if (ImGui::Checkbox("Antialiased", &s)) {
            goxel->painter.smoothness = s ? 1 : 0;
        }
    }
    if (IS_IN(goxel->tool, TOOL_BRUSH, TOOL_CUBE)) {
        ImGui::Text("Snap on");
        for (i = 0; i < 2; i++) {
            s = goxel->snap & (1 << i);
            if (ImGui::GoxSelectable(snap[i], &s, 0, 0)) {
                goxel->snap = s ? goxel->snap | (1 << i) : goxel->snap & ~(1 << i);
            }
            if (i != 1)
                ImGui::SameLine();
        }
    }
    if (goxel->tool == TOOL_BRUSH) {
        v = goxel->snap_offset;
        if (ImGui::InputFloat("Snap offset", &v, 0.1))
            goxel->snap_offset = clamp(v, -1, +1);
    }
    if (IS_IN(goxel->tool, TOOL_BRUSH, TOOL_CUBE)) {
        op_panel(goxel);
        shapes_panel(goxel);
    }
    if (IS_IN(goxel->tool, TOOL_BRUSH, TOOL_CUBE, TOOL_PICK_COLOR)) {
        ImGui::Text("Color");
        color = uvec4b_to_imvec4(goxel->painter.color);
        ImGui::ColorButton(color);
        if (ImGui::BeginPopupContextItem("color context menu", 0)) {
            ImGui::GoxColorEdit("##edit", &goxel->painter.color);
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    if (goxel->tool == TOOL_SET_PLANE) {
        i = 0;
        if (ImGui::InputInt("Move", &i))
            mat4_itranslate(&goxel->plane.mat, 0, 0, -i);
        i = 0;
        if (ImGui::InputInt("Rot X", &i))
            mat4_irotate(&goxel->plane.mat, i * M_PI / 2, 1, 0, 0);
        i = 0;
        if (ImGui::InputInt("Rot Y", &i))
            mat4_irotate(&goxel->plane.mat, i * M_PI / 2, 0, 1, 0);
    }
    if (goxel->tool == TOOL_MOVE) {
        mat = mat4_identity;
        layer = goxel->image->active_layer;
        i = 0;
        if (ImGui::InputInt("Move X", &i))
            mat4_itranslate(&mat, i, 0, 0);
        i = 0;
        if (ImGui::InputInt("Move Y", &i))
            mat4_itranslate(&mat, 0, i, 0);
        i = 0;
        if (ImGui::InputInt("Move Z", &i))
            mat4_itranslate(&mat, 0, 0, i);
        i = 0;
        if (ImGui::InputInt("Rot X", &i))
            mat4_irotate(&mat, i * M_PI / 2, 1, 0, 0);
        i = 0;
        if (ImGui::InputInt("Rot Y", &i))
            mat4_irotate(&mat, i * M_PI / 2, 0, 1, 0);
        i = 0;
        if (ImGui::InputInt("Rot Z", &i))
            mat4_irotate(&mat, i * M_PI / 2, 0, 0, 1);
        if (layer->image && ImGui::InputInt("Scale", &i)) {
            v = pow(2, i);
            mat4_iscale(&mat, v, v, v);
        }
        if (memcmp(&mat, &mat4_identity, sizeof(mat))) {
            mesh_move(layer->mesh, &mat);
            layer->mat = mat4_mul(mat, layer->mat);
            goxel_update_meshes(goxel, true);
            image_history_push(goxel->image);
        }
    }
    if (goxel->tool == TOOL_SELECTION) {
        ImGui::GoxAction("clear_selection", "Clear selection", NULL);
        ImGui::GoxAction("cut_as_new_layer", "Cut as new layer", NULL);
    }
}

static void shapes_panel(goxel_t *goxel)
{
    struct {
        const char  *name;
        shape_t     *shape;
        int         icon;
    } shapes[] = {
        {"Sphere", &shape_sphere, ICON_SHAPE_SPHERE},
        {"Cube", &shape_cube, ICON_SHAPE_CUBE},
        {"Cylinder", &shape_cylinder, ICON_SHAPE_CYLINDER},
    };
    int i;
    bool v;
    ImGui::Text("Shape");
    ImGui::PushID("shapes");
    for (i = 0; i < (int)ARRAY_SIZE(shapes); i++) {
        v = goxel->painter.shape == shapes[i].shape;
        if (ImGui::GoxSelectable(shapes[i].name, &v, g_tex_icons->tex,
                                 shapes[i].icon)) {
            goxel->painter.shape = shapes[i].shape;
        }
        if (i != ARRAY_SIZE(shapes) - 1)
            ImGui::SameLine();
    }
    ImGui::PopID();
}

static void procedural_panel(goxel_t *goxel)
{
    static char **progs = NULL;
    static char **names = NULL;
    static int nb_progs = 0;
    static bool first_time = true;
    int i;
    static int current = -1;
    gox_proc_t *proc = &goxel->proc;
    bool enabled;
    static bool auto_run;
    static int timer = 0;

    ImGuiStyle& style = ImGui::GetStyle();

    if (first_time) {
        first_time = false;
        strcpy(gui->prog_buff, "shape main {\n    cube[s 3]\n}");
        for (i = 0; i < nb_progs; i++) {free(progs[i]); free(names[i]);}
        free(progs);
        free(names);
        nb_progs = proc_list_examples(NULL);
        progs = (char**)calloc(nb_progs, sizeof(*progs));
        names = (char**)calloc(nb_progs, sizeof(*names));
        proc_list_examples(
                [](int i, const char *name, const char *code){
                    progs[i] = strdup(code);
                    names[i] = strdup(name);
                });
        proc_parse(gui->prog_buff, proc);
    }

    if (ImGui::InputTextMultiline("", gui->prog_buff,
                                  ARRAY_SIZE(gui->prog_buff),
                                  ImVec2(-1, 400))) {
        timer = 0;
        proc_parse(gui->prog_buff, proc);
    }
    if (proc->error.str) {
        float h = ImGui::CalcTextSize("").y;
        ImVec2 rmin = ImGui::GetItemRectMin();
        ImVec2 rmax = ImGui::GetItemRectMax();
        rmin.y = rmin.y + goxel->proc.error.line * h + 2;
        rmax.y = rmin.y + h;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRect(rmin, rmax, 0x800000ff);
        ImGui::Text("%s", proc->error.str);
    }
    enabled = proc->state >= PROC_READY;

    if (auto_run && proc->state == PROC_READY && timer == 0) timer = 1;
    if (proc->state == PROC_RUNNING) {
        if (ImGui::Button("Stop")) proc_stop(proc);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[
                         enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled]);
        if (    (ImGui::Button("Run") && enabled) ||
                (auto_run && proc->state == PROC_READY &&
                 timer && timer++ >= 16)) {
            mesh_clear(goxel->image->active_layer->mesh);
            proc_start(proc);
            timer = 0;
        }
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto", &auto_run))
        proc_parse(gui->prog_buff, proc);
    ImGui::SameLine();

    if (ImGui::Button("Export Animation")) {
        char *dir_path;
        if (dialog_open(DIALOG_FLAG_SAVE | DIALOG_FLAG_DIR, NULL, &dir_path)) {
            mesh_clear(goxel->image->active_layer->mesh);
            proc_start(proc);
            gui->prog_export_animation = true;
            sprintf(gui->prog_export_animation_path, "Path: %s", dir_path);
            free(dir_path);
        }
    }

    // File load / save.  No error check yet!
    if (*gui->prog_path) {
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##path", gui->prog_path, sizeof(gui->prog_path));
        ImGui::PopItemWidth();
    }
    if (ImGui::Button("Load")) {
        char *path;
        if (dialog_open(DIALOG_FLAG_OPEN, "goxcf\0*.goxcf\0", &path)) {
            FILE *f = fopen(path, "r");
            int nb;
            nb = fread(gui->prog_buff, 1, sizeof(gui->prog_buff), f);
            gui->prog_buff[nb] = '\0';
            fclose(f);
            strcpy(gui->prog_path, path);
            free(path);
        }
        proc_parse(gui->prog_buff, proc);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (!*gui->prog_path) {
            char *path;
            if (dialog_open(DIALOG_FLAG_SAVE, "goxcf\0*.goxcf\0", &path)) {
                strcpy(gui->prog_path, path);
                free(path);
            }
        }
        if (*gui->prog_path) {
            FILE *f = fopen(gui->prog_path, "w");
            fwrite(gui->prog_buff, strlen(gui->prog_buff), 1, f);
            fclose(f);
        }
    }

    ImGui::PushItemWidth(-100);
    if (ImGui::Combo("Examples", &current, (const char**)names, nb_progs)) {
        strcpy(gui->prog_buff, progs[current]);
        proc_parse(gui->prog_buff, proc);
    }
    ImGui::PopItemWidth();

    if (proc->state == PROC_RUNNING && gui->prog_export_animation
            && !proc->in_frame) {
        char path[1024];
        sprintf(path, "%s/img_%04d.png",
                gui->prog_export_animation_path, proc->frame);
        action_exec2("export_as", ARG("type", "png"), ARG("path", path));
    }
    if (proc->state != PROC_RUNNING) gui->prog_export_animation = false;

    if (proc->state == PROC_RUNNING) {
        proc_iter(proc);
        if (!proc->in_frame)
            goxel_update_meshes(goxel, false);
    }
}


static void tools_panel(goxel_t *goxel)
{
    const struct {
        int         tool;
        const char  *name;
        int         icon;
    } values[] = {
        {TOOL_BRUSH,        "Brush",        ICON_TOOL_BRUSH},
        {TOOL_CUBE,         "Cube",         ICON_TOOL_CUBE},
        {TOOL_LASER,        "Laser",        ICON_TOOL_LASER},
        {TOOL_SET_PLANE,    "Plane",        ICON_TOOL_PLANE},
        {TOOL_MOVE,         "Move",         ICON_TOOL_MOVE},
        {TOOL_PICK_COLOR,   "Pick Color",   ICON_TOOL_PICK},
        {TOOL_SELECTION,    "Selection",    ICON_TOOL_SELECTION},
        {TOOL_PROCEDURAL,   "Procedural",   ICON_TOOL_PROCEDURAL},
    };
    const int nb = ARRAY_SIZE(values);
    int i;
    bool v;
    ImGui::PushID("tools_panel");
    for (i = 0; i < nb; i++) {
        v = goxel->tool == values[i].tool;
        if (ImGui::GoxSelectable(values[i].name, &v, g_tex_icons->tex,
                                 values[i].icon)) {
            goxel->tool = values[i].tool;
            goxel->tool_state = 0;
        }
        if ((i + 1) % 4 && i != nb - 1) ImGui::SameLine();
    }
    if (goxel->tool != TOOL_PROCEDURAL) {
        if (ImGui::GoxCollapsingHeader("Tool Options", NULL, true, true))
            tool_options_panel(goxel);
    } else {
        if (ImGui::GoxCollapsingHeader("Procedural Rendering", NULL,
                                       true, true))
            procedural_panel(goxel);
    }
    ImGui::PopID();
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

static void layers_panel(goxel_t *goxel)
{
    layer_t *layer;
    int i = 0;
    bool current;
    ImGui::PushID("layers_planel");
    DL_FOREACH(goxel->image->layers, layer) {
        ImGui::PushID(i);
        ImGui::AlignFirstTextHeightToWidgets();
        current = goxel->image->active_layer == layer;
        if (ImGui::Selectable(current ? "*" : " ", &current, 0,
                              ImVec2(12, 12))) {
            if (current) {
                goxel->image->active_layer = layer;
                goxel_update_meshes(goxel, true);
            }
        }
        ImGui::SameLine();
        if (ImGui::Selectable(layer->visible ? "v##v" : " ##v",
                    &layer->visible, 0, ImVec2(12, 12))) {
            if (ImGui::IsKeyDown(KEY_SHIFT))
                toggle_layer_only_visible(goxel, layer);
            goxel_update_meshes(goxel, true);
        }
        ImGui::SameLine();
        ImGui::InputText("##name", layer->name, sizeof(layer->name));
        i++;
        ImGui::PopID();
    }
    ImGui::GoxAction("img_new_layer", "Add", NULL);
    ImGui::SameLine();
    ImGui::GoxAction("img_del_layer", "Del", NULL);
    ImGui::SameLine();
    ImGui::GoxAction("img_move_layer", "^", ARGS(ARG("ofs", +1)));
    ImGui::SameLine();
    ImGui::GoxAction("img_move_layer", "v", ARGS(ARG("ofs", -1)));
    ImGui::GoxAction("img_duplicate_layer", "Duplicate", NULL);
    ImGui::SameLine();
    ImGui::GoxAction("img_merge_visible_layers", "Merge visible", NULL);
    ImGui::PopID();
}

static void palette_panel(goxel_t *goxel)
{
    palette_t *p = goxel->palette;
    int i;
    for (i = 0; i < p->size; i++) {
        ImGui::PushID(i);
        ImGui::GoxPaletteEntry(&p->values[i], &goxel->painter.color);
        if ((i + 1) % 6 && i != p->size - 1) ImGui::SameLine();
        ImGui::PopID();
    }
}

static void render_advanced_panel(goxel_t *goxel)
{
    float v;
    bool b;
    ImVec4 c;
    int i;
    const struct {
        uvec4b_t   *color;
        const char *label;
    } COLORS[] = {
        {&goxel->back_color, "Back color"},
        {&goxel->grid_color, "Grid color"},
    };

    ImGui::PushID("RenderAdvancedPanel");

    ImGui::Text("Light");
    i = round(goxel->rend.light.pitch * DR2D);
    ImGui::InputInt("Pitch", &i);
    goxel->rend.light.pitch = clamp(i, -90, +90) * DD2R;
    i = round(goxel->rend.light.yaw * DR2D);
    ImGui::InputInt("Yaw", &i);
    while (i < 0) i += 360;
    goxel->rend.light.yaw = (i % 360) * DD2R;
    ImGui::Checkbox("Fixed", &goxel->rend.light.fixed);


    v = goxel->rend.settings.border_shadow;
    if (ImGui::InputFloat("bshadow", &v, 0.1)) {
        v = clamp(v, 0, 1); \
        goxel->rend.settings.border_shadow = v;
    }
#define MAT_FLOAT(name, min, max) \
    v = goxel->rend.settings.name;  \
    if (ImGui::InputFloat(#name, &v, 0.1)) { \
        v = clamp(v, min, max); \
        goxel->rend.settings.name = v; \
    }

    MAT_FLOAT(ambient, 0, 1);
    MAT_FLOAT(diffuse, 0, 1);
    MAT_FLOAT(specular, 0, 1);
    MAT_FLOAT(shininess, 0.1, 4);
    MAT_FLOAT(smoothness, 0, 1);

#undef MAT_FLOAT

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

    ImGui::Text("Other");
    for (i = 0; i < (int)ARRAY_SIZE(COLORS); i++) {
        ImGui::PushID(COLORS[i].label);
        c = uvec4b_to_imvec4(*COLORS[i].color);
        ImGui::ColorButton(c);
        if (ImGui::BeginPopupContextItem("color context menu", 0)) {
            ImGui::GoxColorEdit("##edit", COLORS[i].color);
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::Text("%s", COLORS[i].label);
        ImGui::PopID();
    }
    ImGui::PopID();

    b = !goxel->plane_hidden;
    if (ImGui::Checkbox("Show grid", &b)) goxel->plane_hidden = !b;

    ImGui::Text("Export");
    i = goxel->image->export_width;
    if (ImGui::InputInt("width", &i, 1))
        goxel->image->export_width = clamp(i, 1, 2048);
    i = goxel->image->export_height;
    if (ImGui::InputInt("height", &i, 1))
        goxel->image->export_height = clamp(i, 1, 2048);
}


static void render_panel(goxel_t *goxel)
{
    int i, current = 0;
    int nb = render_get_default_settings(0, NULL, NULL);
    float v;
    char *name;
    render_settings_t settings;

    ImGui::Checkbox("Ortho", &goxel->camera.ortho);
    ImGui::PushID("RenderPanel");
    for (i = 0; i < nb; i++) {
        render_get_default_settings(i, &name, &settings);
        current = memcmp(&goxel->rend.settings, &settings,
                         sizeof(settings)) == 0;
        if (ImGui::RadioButton(name, current) && !current)
            goxel->rend.settings = settings;
    }
    v = goxel->rend.settings.shadow;
    if (ImGui::InputFloat("shadow", &v, 0.1)) {
        goxel->rend.settings.shadow = clamp(v, 0, 1);
    }
    if (ImGui::GoxCollapsingHeader("Render Advanced", NULL, true, false))
        render_advanced_panel(goxel);
    ImGui::PopID();
}

static void save_as(goxel_t *goxel)
{
    char *path = NULL;
    bool result;
    result = dialog_open(DIALOG_FLAG_SAVE, "gox\0*.gox\0", &path);
    if (!result) return;
    free(goxel->image->path);
    goxel->image->path = path;
    save_to_file(goxel, path);
}

static void save(goxel_t *goxel)
{
    if (!goxel->image->path) {
        save_as(goxel);
        return;
    }
    save_to_file(goxel, goxel->image->path);
}

static void import_dicom(goxel_t *goxel)
{
    char *path = NULL;
    bool result = dialog_open(DIALOG_FLAG_OPEN | DIALOG_FLAG_DIR, NULL, &path);
    if (!result) return;
    dicom_import(path);
    free(path);
}

static void import_qubicle(goxel_t *goxel)
{
    char *path = NULL;
    bool result = dialog_open(DIALOG_FLAG_OPEN, NULL, &path);
    if (!result) return;
    qubicle_import(path);
    free(path);
}

static void import_vox(goxel_t *goxel)
{
    char *path = NULL;
    bool result = dialog_open(DIALOG_FLAG_OPEN, NULL, &path);
    if (!result) return;
    vox_import(path);
    free(path);
}

static void import_image_plane(goxel_t *goxel)
{
    char *path = NULL;
    bool result = dialog_open(DIALOG_FLAG_OPEN, NULL, &path);
    if (!result) return;
    goxel_import_image_plane(goxel, path);
    free(path);
}

static void export_as(goxel_t *goxel, const char *type)
{
    char *path = NULL;
    bool result = dialog_open(DIALOG_FLAG_SAVE, type, &path);
    if (!result) return;
    action_exec2("export_as", ARG("type", type), ARG("path", path));
    free(path);
}

static void load(goxel_t *goxel)
{
    char *path = NULL;
    bool result = dialog_open(DIALOG_FLAG_OPEN, "gox\0*.gox\0", &path);
    if (!result) return;
    load_from_file(goxel, path);
    free(path);
}

static void render_profiler_info(void)
{
    profiler_block_t *block, *root;
    int percent, fps;
    double time_per_frame;
    double self_time;

    root = profiler_get_blocks();
    if (!root) return;
    time_per_frame = root->tot_time / root->count / (1000.0 * 1000.0);
    fps = 1000.0 / time_per_frame;
    ImGui::BulletText("%.1fms/frame (%dfps)", time_per_frame, fps);
    for (block = root; block; block = block->next) {
        self_time = block->self_time / root->count / (1000.0 * 1000.0);
        percent = block->self_time * 100 / root->tot_time;
        if (!percent) continue;
        ImGui::BulletText("%s: self:%.1fms/frame (%d%%)",
                block->name, self_time, percent);
    }
}

static void shift_alpha_popup(goxel_t *goxel, bool just_open)
{
    static int v = 0;
    static mesh_t *original_mesh;
    mesh_t *mesh;
    mesh = goxel->image->active_layer->mesh;
    if (just_open)
        original_mesh = mesh_copy(mesh);
    if (ImGui::InputInt("shift", &v, 1)) {
        mesh_set(&mesh, original_mesh);
        mesh_shift_alpha(mesh, v);
        goxel_update_meshes(goxel, true);
    }
    if (ImGui::Button("OK")) {
        mesh_delete(original_mesh);
        original_mesh = NULL;
        ImGui::CloseCurrentPopup();
    }
}

void gui_iter(goxel_t *goxel, const inputs_t *inputs)
{
    static view_t view;
    static int current_panel = 0;
    float left_pane_width;
    bool open_shift_alpha = false;
    unsigned int i;
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)goxel->screen_size.x, (float)goxel->screen_size.y);

    // Setup time step
    io.DeltaTime = 1.0 / 60;

    io.MousePos = ImVec2(inputs->mouse_pos.x, inputs->mouse_pos.y);
    io.MouseDown[0] = inputs->mouse_down[0];
    io.MouseDown[1] = inputs->mouse_down[1];
    io.MouseWheel = inputs->mouse_wheel;

    for (i = 0; i < ARRAY_SIZE(inputs->keys); i++)
        io.KeysDown[i] = inputs->keys[i];
    io.KeyShift = inputs->keys[KEY_SHIFT];
    io.KeyCtrl = inputs->keys[KEY_CONTROL];
    for (i = 0; i < ARRAY_SIZE(inputs->chars); i++) {
        if (!inputs->chars[i]) break;
        io.AddInputCharacter(inputs->chars[i]);
    }

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

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                save(goxel);
            }
            if (ImGui::MenuItem("Save as")) {
                save_as(goxel);
            }
            if (ImGui::MenuItem("Load", "Ctrl+O")) {
                load(goxel);
            }
            if (ImGui::BeginMenu("Import...")) {
                if (ImGui::MenuItem("image plane")) import_image_plane(goxel);
                if (ImGui::MenuItem("qubicle")) import_qubicle(goxel);
                if (ImGui::MenuItem("vox")) import_vox(goxel);
                if (ImGui::MenuItem("dicom")) import_dicom(goxel);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Export As..")) {
                if (ImGui::MenuItem("png")) export_as(goxel, "png\0*.png\0");
                if (ImGui::MenuItem("obj")) export_as(goxel, "obj\0*.obj\0");
                if (ImGui::MenuItem("ply")) export_as(goxel, "ply\0*.ply\0");
                if (ImGui::MenuItem("qubicle")) export_as(goxel, "qubicle\0*.qb\0");
                if (ImGui::MenuItem("txt")) export_as(goxel, "txt\0*.txt\0");
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) goxel_undo(goxel);
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) goxel_redo(goxel);
            if (ImGui::MenuItem("Shift Alpha"))
                open_shift_alpha = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::Spacing();

    left_pane_width = 180;
    if (goxel->tool == TOOL_PROCEDURAL) {
        left_pane_width = clamp(ImGui::CalcTextSize(gui->prog_buff).x + 60,
                                250, 600);
    }
    ImGui::BeginChild("left pane", ImVec2(left_pane_width, 0), true);
    ImGui::PushItemWidth(75);

    const struct {
        const char *name;
        const char *tooltip;
        void (*fn)(goxel_t *goxel);
    } PANELS[] = {
        {"T", "Tools", tools_panel},
        {"P", "Palette", palette_panel},
        {"L", "layers", layers_panel},
        {"R", "Render", render_panel}
    };

    for (i = 0; i < (int)ARRAY_SIZE(PANELS); i++) {
        bool b = (current_panel == (int)i);
        if (i) ImGui::SameLine();
        if (ImGui::GoxSelectable(PANELS[i].name, &b, 0, 0,
                                 PANELS[i].tooltip))
            current_panel = i;
    }

    ImGui::Text(PANELS[current_panel].tooltip);
    PANELS[current_panel].fn(goxel);

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("3d view", ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollWithMouse);
    // ImGui::Text("3d view");
    view.goxel = goxel;
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    canvas_size.y -= 20; // Leave space for the help label.
    view.rect = vec4(canvas_pos.x, canvas_pos.y, canvas_size.x, canvas_size.y);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCallback(render_view, &view);
    // Invisible button so that we catch inputs.
    ImGui::InvisibleButton("canvas", canvas_size);
    vec2_t view_size = vec2(view.rect.z, view.rect.w);
    if (ImGui::IsItemHovered() || goxel->tool_state) {
        inputs_t rel_inputs = *inputs;
        rel_inputs.mouse_pos =
            vec2(ImGui::GetIO().MousePos.x - canvas_pos.x,
                 ImGui::GetIO().MousePos.y - canvas_pos.y);
        goxel_mouse_in_view(goxel, &view_size, &rel_inputs,
                ImGui::IsItemHovered());
    }
    render_axis_arrows(goxel, &view_size);

    // Apparently there is a bug if we do not render anything.  So I render
    // a '.' if there is nothing.  This is a hack.
    ImGui::Text("%s", goxel->help_text ?: ".");
    ImGui::EndChild();

    if (DEBUG || PROFILER) {
        ImGui::SetCursorPos(ImVec2(left_pane_width + 20, 30));
        ImGui::BeginChild("debug", ImVec2(0, 0), false,
                          ImGuiWindowFlags_NoInputs);
        ImGui::Text("Blocks: %d (%.2g MiB)", goxel->block_count,
                (float)goxel->block_count * sizeof(block_data_t) / MiB);
        ImGui::Text("Blocks id: %d", goxel->block_next_id);
        if (PROFILER)
            render_profiler_info();
        ImGui::EndChild();
    }

    ImGui::End();

    if (open_shift_alpha)
        ImGui::OpenPopup("Shift Alpha");
    if (ImGui::BeginPopupModal("Shift Alpha", NULL,
                ImGuiWindowFlags_AlwaysAutoResize)) {
        shift_alpha_popup(goxel, open_shift_alpha);
        ImGui::EndPopup();
    }

    // Handle the shortcuts.  XXX: this should be done better.
    if (ImGui::GoxIsCharPressed('#'))
        goxel->plane_hidden = !goxel->plane_hidden;
    if (ImGui::IsKeyPressed(' ', false) && goxel->painter.op == OP_ADD)
        goxel->painter.op = OP_SUB;
    if (ImGui::IsKeyReleased(' ') && goxel->painter.op == OP_SUB)
        goxel->painter.op = OP_ADD;
    if (ImGui::IsKeyReleased(' ') && goxel->painter.op == OP_SUB)
        goxel->painter.op = OP_ADD;
    if (ImGui::IsKeyPressed(KEY_CONTROL, false) && goxel->tool == TOOL_BRUSH) {
        tool_cancel(goxel, goxel->tool, goxel->tool_state);
        goxel->prev_tool = goxel->tool;
        goxel->tool = TOOL_PICK_COLOR;
    }
    if (ImGui::IsKeyReleased(KEY_CONTROL) && goxel->prev_tool) {
        tool_cancel(goxel, goxel->tool, goxel->tool_state);
        goxel->tool = goxel->prev_tool;
        goxel->prev_tool = 0;
    }
    // XXX: this won't map correctly to a French keyboard.  Unfortunately as
    // far as I can tell, GLFW3 does not allow to check for ctrl-Z on any
    // layout on Windows.  For the moment I just ignore the problem until I
    // either find a solution, either find a replacement for GLFW.
    // With the GLUT backend ctrl-z and ctrl-y are actually reported as the
    // key 25 and 26, which might makes more sense.  Here I test for both.
    if (    (io.KeyCtrl && ImGui::IsKeyPressed('Z', false)) ||
            ImGui::GoxIsCharPressed(26))
        goxel_undo(goxel);
    if (    (io.KeyCtrl && ImGui::IsKeyPressed('Y', false)) ||
            ImGui::GoxIsCharPressed(25))
        goxel_redo(goxel);

}

void gui_render(void)
{
    ImGui::Render();
}
