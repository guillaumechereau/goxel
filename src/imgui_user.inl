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

static texture_t *g_hsl_tex = NULL;
static texture_t *g_hue_tex = NULL;
static int g_group = 0;

static ImVec4 uvec4b_to_imvec4(uvec4b_t v)
{
    return ImVec4(v.x / 255., v.y / 255., v.z / 255., v.w / 255);
}

// Create an Sat/Hue bitmap with all the value for a given hue.
static void hsl_bitmap(int hue, uint8_t *buffer, int w, int h)
{
    int x, y, sat, light;
    uvec3b_t rgb;
    for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
        sat = 256 * (h - y - 1) / h;
        light = 256 * x / w;
        rgb = hsl_to_rgb(uvec3b(hue, sat, light));
        memcpy(&buffer[(y * w + x) * 3], rgb.v, 3);
    }
}

static void hue_bitmap(uint8_t *buffer, int w, int h)
{
    int x, y, hue;
    uvec3b_t rgb;
    for (y = 0; y < h; y++) {
        hue = 256 * (h - y - 1) / h;
        rgb = hsl_to_rgb(uvec3b(hue, 255, 127));
        for (x = 0; x < w; x++) {
            memcpy(&buffer[(y * w + x) * 3], rgb.v, 3);
        }
    }
}

void stencil_callback(const ImDrawList* parent_list, const ImDrawCmd* cmd)
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


namespace ImGui {

    void GoxBox2(ImVec2 pos, ImVec2 size, ImVec4 color, bool fill)
    {
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        ImGuiWindow* window = GetCurrentWindow();
        float r = style.FrameRounding;
        size.x = size.x ?: ImGui::GetContentRegionAvailWidth();
        if (fill) {
            window->DrawList->AddRectFilled(
                    pos, pos + size,
                    ImGui::ColorConvertFloat4ToU32(color), r);
        } else {
            window->DrawList->AddRect(
                    pos, pos + size,
                    ImGui::ColorConvertFloat4ToU32(color), r);
        }
    }

    void GoxBox(ImVec2 pos, ImVec2 size, bool selected)
    {
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        ImVec4 color  = style.Colors[selected ? ImGuiCol_ButtonActive :
                                     ImGuiCol_Button];
        return GoxBox2(pos, size, color, true);
    }

    void GoxStencil(int op)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddCallback(stencil_callback, (void*)(intptr_t)op);
    }

    void GoxGroupBegin(void)
    {
        g_group++;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->ChannelsSplit(2);
        draw_list->ChannelsSetCurrent(1);
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    }

    void GoxGroupEnd(void)
    {
        g_group--;
        ImGui::PopStyleVar(2);
        ImGui::Dummy(ImVec2(0, 0));
        ImGui::EndGroup();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->ChannelsSetCurrent(0);
        ImVec2 pos = ImGui::GetItemRectMin();
        ImVec2 size = ImGui::GetItemRectMax() - pos;

        GoxStencil(1); // Stencil write.
        GoxBox2(pos + ImVec2(1, 1), size - ImVec2(2, 2),
                ImVec4(0.3, 0.3, 0.3, 1), true);
        GoxStencil(2); // Stencil filter.

        draw_list->ChannelsMerge();
        GoxStencil(0); // Stencil reset.
        GoxBox2(pos, size, ImVec4(0.3, 0.3, 0.3, 1), false);
    }

    bool GoxSelectable(const char *name, bool *v, int tex, int icon,
                       const char *tooltip, ImVec2 size) {
        ImGuiWindow* window = GetCurrentWindow();
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        if (size.x == 0) size.x = 32;
        if (size.y == 0) size.y = 32;

        const ImVec2 padding = ImVec2(0, 0);
        const ImRect image_bb(pos + padding, pos + padding + size);
        bool ret = false;
        ImVec2 uv0, uv1; // The position in the icon texture.
        ImVec4 color;

        if (!tooltip) tooltip = name;
        ImGui::PushID(name);

        color = style.Colors[ImGuiCol_Button];
        if (*v) color = style.Colors[ImGuiCol_ButtonActive];
        ImGui::PushStyleColor(ImGuiCol_Button, color);

        if (tex) {
            ret = ImGui::Button("", size);
            uv0 = ImVec2((icon % 8) / 8.0, (icon / 8) / 8.0);
            uv1 = uv0 + ImVec2(1. / 8, 1. / 8);
            window->DrawList->AddImage((void*)tex, image_bb.Min, image_bb.Max,
                                       uv0, uv1, 0xFF000000);
        } else {
            ret = ImGui::Button(name, size);
        }
        ImGui::PopStyleColor();
        if (ret) *v = !*v;
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
            goxel_set_help_text(goxel, tooltip);
        }
        ImGui::PopID();

        return ret;
    }

    static int create_hsl_texture(int hue) {
        uint8_t *buffer;
        if (!g_hsl_tex) {
            g_hsl_tex = texture_new_surface(256, 256, TF_RGB);
        }
        buffer = (uint8_t*)malloc(256 * 256 * 3); 
        hsl_bitmap(hue, buffer, 256, 256);

        glBindTexture(GL_TEXTURE_2D, g_hsl_tex->tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, buffer);

        free(buffer);
        return g_hsl_tex->tex;
    }

    static int create_hue_texture(void)
    {
        uint8_t *buffer;
        if (!g_hue_tex) {
            g_hue_tex = texture_new_surface(32, 256, TF_RGB);
            buffer = (uint8_t*)malloc(32 * 256 * 3);
            hue_bitmap(buffer, 32, 256);
            glBindTexture(GL_TEXTURE_2D, g_hue_tex->tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 256, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, buffer);
            free(buffer);
        }
        return g_hue_tex->tex;
    }

    static ImVec4 uvec4b_to_imvec(uvec4b_t v)
    {
        return ImVec4(v.r / 255.0, v.g / 255.0, v.b / 255.0, v.a / 255.0);
    }

    bool GoxPaletteEntry(const uvec4b_t *color, uvec4b_t *target)
    {
        ImVec4 c;
        bool ret;
        c = uvec4b_to_imvec4(*color);
        ImGuiWindow* window = GetCurrentWindow();

        // XXX: set border color according to color.
        if (uvec4b_equal(*color, *target))
            window->Flags |= ImGuiWindowFlags_ShowBorders;
        ret = ImGui::ColorButton(c, true);
        if (ret) *target = *color;
        window->Flags &= ~ImGuiWindowFlags_ShowBorders;
        return ret;
    }

    bool GoxColorEdit(const char *name, uvec4b_t *color) {
        bool ret = false;
        ImVec2 c_pos;
        ImGuiIO& io = ImGui::GetIO();
        uvec4b_t hsl;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImGui::Text("Edit color");
        ImVec4 c = uvec4b_to_imvec(*color);
        hsl.xyz = rgb_to_hsl(color->rgb);
        hsl.a = color->a;

        c_pos = ImGui::GetCursorScreenPos();
        int tex_size = 256;
        ImGui::Image((void*)create_hsl_texture(hsl.x),
                     ImVec2(tex_size, tex_size));
        // Draw lines.
        draw_list->AddLine(c_pos + ImVec2(hsl.z * tex_size / 255, 0),
                           c_pos + ImVec2(hsl.z * tex_size / 255, 256),
                           0xFFFFFFFF, 2.0f);
        draw_list->AddLine(c_pos + ImVec2(0,   256 - hsl.y * tex_size / 255),
                           c_pos + ImVec2(256, 256 - hsl.y * tex_size / 255),
                           0xFFFFFFFF, 2.0f);

        draw_list->AddLine(c_pos + ImVec2(hsl.z * tex_size / 255, 0),
                           c_pos + ImVec2(hsl.z * tex_size / 255, 256),
                           0xFF000000, 1.0f);
        draw_list->AddLine(c_pos + ImVec2(0,   256 - hsl.y * tex_size / 255),
                           c_pos + ImVec2(256, 256 - hsl.y * tex_size / 255),
                           0xFF000000, 1.0f);

        if (ImGui::IsItemHovered() && io.MouseDown[0]) {
            ImVec2 pos = ImVec2(ImGui::GetIO().MousePos.x - c_pos.x,
                                ImGui::GetIO().MousePos.y - c_pos.y);
            int sat = 255 - pos.y;
            int light = pos.x;
            color->rgb = hsl_to_rgb(uvec3b(hsl.x, sat, light));
            c = uvec4b_to_imvec(*color);
            ret = true;
        }
        ImGui::SameLine();
        c_pos = ImGui::GetCursorScreenPos();
        ImGui::Image((void*)create_hue_texture(), ImVec2(32, 256));
        draw_list->AddLine(c_pos + ImVec2( 0, 255 - hsl.x * 256 / 255),
                           c_pos + ImVec2(31, 255 - hsl.x * 256 / 255),
                           0xFFFFFFFF, 2.0f);

        if (ImGui::IsItemHovered() && io.MouseDown[0]) {
            ImVec2 pos = ImVec2(ImGui::GetIO().MousePos.x - c_pos.x,
                                ImGui::GetIO().MousePos.y - c_pos.y);
            int hue = 255 - pos.y;
            color->rgb = hsl_to_rgb(uvec3b(hue, hsl.y, hsl.z));
            c = uvec4b_to_imvec(*color);
            ret = true;
        }

        ret = ImGui::ColorEdit3(name, (float*)&c) || ret;
        if (ret) {
            *color = uvec4b(c.x * 255, c.y * 255, c.z * 255, c.w * 255);
        }
        return ret;
    }

    bool GoxIsCharPressed(int c)
    {
        ImGuiContext& g = *GImGui;
        return g.IO.InputCharacters[0] == c;
    }

    bool GoxCollapsingHeader(const char *label, const char *str_id,
                             bool display_frame,
                             bool default_open)
    {
        return ImGui::CollapsingHeader(label, str_id, display_frame, default_open);
    }

    bool GoxAction(const char *id, const char *label, float size,
                   const char *sig, ...)
    {
        va_list ap;
        float w = ImGui::GetContentRegionAvailWidth();
        assert(action_get(id));
        if (ImGui::Button(label, ImVec2(size * w, 0))) {
            va_start(ap, sig);
            action_execv(action_get(id), sig, ap);
            va_end(ap);
            return true;
        }
        if (ImGui::IsItemHovered()) {
            goxel_set_help_text(goxel, action_get(id)->help);
        }
        return false;
    }

    bool GoxCheckbox(const char *id, const char *label)
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

    bool GoxMenuItem(const char *id, const char *label)
    {
        const action_t *action = action_get(id);
        assert(action);
        if (ImGui::MenuItem(label, action->shortcut)) {
            action_exec(action, "");
            return true;
        }
        return false;
    }

    bool GoxInputAngle(const char *id, float *v, int vmin, int vmax)
    {
        int a;
        bool ret;
        a = round(*v * DR2D);
        ret = ImGui::InputInt(id, &a);
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

    bool GoxTab(const char *text, bool *v)
    {
        ImFont *font = GImGui->Font;
        const ImFont::Glyph *glyph;
        char c;
        bool ret;
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        float pad = style.FramePadding.x;
        uint32_t text_color;
        ImVec4 color;
        ImVec2 text_size = CalcTextSize(text);
        ImGuiWindow* window = GetCurrentWindow();
        ImVec2 pos = window->DC.CursorPos + ImVec2(pad, text_size.x + pad);

        color = style.Colors[ImGuiCol_Button];
        if (*v) color = style.Colors[ImGuiCol_ButtonActive];
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushID(text);
        ret = ImGui::Button("", ImVec2(text_size.y + pad * 2,
                                       text_size.x + pad * 2));
        ImGui::PopStyleColor();
        text_color = ImGui::ColorConvertFloat4ToU32(
                style.Colors[ImGuiCol_Text]);
        while ((c = *text++)) {
            glyph = font->FindGlyph(c);
            if (!glyph) continue;

            window->DrawList->PrimReserve(6, 4);
            window->DrawList->PrimQuadUV(
                    pos + ImVec2(glyph->Y0, -glyph->X0),
                    pos + ImVec2(glyph->Y0, -glyph->X1),
                    pos + ImVec2(glyph->Y1, -glyph->X1),
                    pos + ImVec2(glyph->Y1, -glyph->X0),

                    ImVec2(glyph->U0, glyph->V0),
                    ImVec2(glyph->U1, glyph->V0),
                    ImVec2(glyph->U1, glyph->V1),
                    ImVec2(glyph->U0, glyph->V1),
                    text_color);
            pos.y -= glyph->XAdvance;
        }
        ImGui::PopID();
        return ret;
    }

    // Copied from imgui, with some customization...
    bool GoxInputScalarAsWidgetReplacement(const ImRect& aabb, const char* label, ImGuiDataType data_type, void* data_ptr, ImGuiID id, int decimal_precision)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindow();

        // Our replacement widget will override the focus ID (registered previously to allow for a TAB focus to happen)
        SetActiveID(g.ScalarAsInputTextId, window);
        SetHoveredID(0);
        FocusableItemUnregister(window);

        char buf[32];
        DataTypeFormatString(data_type, data_ptr, decimal_precision, buf, IM_ARRAYSIZE(buf));
        bool text_value_changed = InputTextEx(label, buf, IM_ARRAYSIZE(buf), aabb.GetSize(), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        if (g.ScalarAsInputTextId == 0)
        {
            // First frame
            IM_ASSERT(g.ActiveId == id);    // InputText ID expected to match the Slider ID (else we'd need to store them both, which is also possible)
            g.ScalarAsInputTextId = g.ActiveId;
            SetHoveredID(id);
        }
        else if (g.ActiveId != g.ScalarAsInputTextId)
        {
            // Release
            g.ScalarAsInputTextId = 0;
        }
        if (text_value_changed)
            return DataTypeApplyOpFromText(buf, GImGui->InputTextState.InitialText.begin(), data_type, data_ptr, NULL);
        return false;
    }

    // Copied from imgui, with some custom modifications.
    bool GoxDragFloat(const char* label, const char* name,
            float* v, float v_speed,
            float v_min, float v_max, const char* display_format, float power)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const float w = CalcItemWidth();

        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y*2.0f));
        const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
        const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

        // NB- we don't call ItemSize() yet because we may turn into a text edit box below
        if (!ItemAdd(total_bb, &id))
        {
            ItemSize(total_bb, style.FramePadding.y);
            return false;
        }

        const bool hovered = IsHovered(frame_bb, id);
        if (hovered)
            SetHoveredID(id);

        if (!display_format)
            display_format = "%.3f";
        int decimal_precision = ParseFormatPrecision(display_format, 3);

        // Tabbing or CTRL-clicking on Drag turns it into an input box
        bool start_text_input = false;
        const bool tab_focus_requested = FocusableItemRegister(window, g.ActiveId == id);
        if (tab_focus_requested || (hovered && (g.IO.MouseClicked[0] | g.IO.MouseDoubleClicked[0])))
        {
            SetActiveID(id, window);
            FocusWindow(window);

            if (tab_focus_requested || g.IO.KeyCtrl || g.IO.MouseDoubleClicked[0])
            {
                start_text_input = true;
                g.ScalarAsInputTextId = 0;
            }
        }
        if (start_text_input || (g.ActiveId == id && g.ScalarAsInputTextId == id))
            return GoxInputScalarAsWidgetReplacement(frame_bb, label, ImGuiDataType_Float, v, id, decimal_precision);

        // Actual drag behavior
        ItemSize(total_bb, style.FramePadding.y);
        const bool value_changed = DragBehavior(frame_bb, id, v, v_speed, v_min, v_max, decimal_precision, power);

        // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
        char value_buf[64];
        const char* value_buf_end = value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), display_format, *v);
        RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(1.0f,0.5f));

        value_buf_end = value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), "%s:", name);
        RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.0f,0.5f));

        return value_changed;
    }

    bool GoxInputFloat(const char *label, float *v, float step,
                       float minv, float maxv, const char *format)
    {
        bool self_group = false;
        if (g_group == 0) {
            GoxGroupBegin();
            self_group = true;
        }
        bool ret = false;
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImVec2 button_sz = ImVec2(
                g.FontSize, g.FontSize) + style.FramePadding * 2.0f;
        int button_flags =
            ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;
        float speed = step / 20;

        ImGui::PushID(label);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));

        ImGui::SetWindowFontScale(0.75);
        if (ImGui::ButtonEx("◀", button_sz, button_flags)) {
            (*v) -= step;
            ret = true;
        }
        ImGui::SetWindowFontScale(1);

        ImGui::SameLine();
        ImGui::PushItemWidth(
                ImGui::GetContentRegionAvailWidth() -
                button_sz.x - style.ItemSpacing.x);

        ret |= ImGui::GoxDragFloat("", label, v, speed, minv, maxv, format, 1.0);
        ImGui::PopItemWidth();

        ImGui::SameLine();

        ImGui::SetWindowFontScale(0.75);
        if (ImGui::ButtonEx("▶", button_sz, button_flags)) {
            (*v) += step;
            ret = true;
        }
        ImGui::SetWindowFontScale(1);

        ImGui::PopStyleVar();
        ImGui::PopID();
        if (self_group) GoxGroupEnd();

        return ret;
    }

    bool GoxInputInt(const char *format, int *v, int step, int minv, int maxv)
    {
        float vf = *v;
        bool ret = GoxInputFloat(format, &vf, 1, minv, maxv, "%.0f");
        if (ret) *v = vf;
        return ret;
    }
};

