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

namespace ImGui {

    static struct {
        int nb;
        int col;
        int i;
    } g_group = {};

    void GoxBox2(ImVec2 pos, ImVec2 size, ImVec4 color,
                 bool fill, int corners)
    {
        ImGuiWindow* window = GetCurrentWindow();
        size.x = size.x ?: ImGui::GetContentRegionAvailWidth();
        if (fill) {
            window->DrawList->AddRectFilled(
                    pos, pos + size,
                    ImGui::ColorConvertFloat4ToU32(color), 4, corners);
        } else {
            window->DrawList->AddRect(
                    pos, pos + size,
                    ImGui::ColorConvertFloat4ToU32(color), 4, corners);
        }
    }

    void GoxBox(ImVec2 pos, ImVec2 size, bool selected, int corners)
    {
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        ImVec4 color  = style.Colors[selected ? ImGuiCol_ButtonActive :
                                     ImGuiCol_Button];
        return GoxBox2(pos, size, color, true, corners);
    }

    void GoxGroupBegin(int nb, int col)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        g_group.nb = nb;
        g_group.col = col ?: nb;
        g_group.i = 0;
        draw_list->ChannelsSplit(2);
        draw_list->ChannelsSetCurrent(1);
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
    }

    void GoxGroupEnd(void)
    {
        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0, 0));
        ImGui::EndGroup();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->ChannelsSetCurrent(0);
        ImVec2 pos = ImGui::GetItemRectMin();
        ImVec2 size = ImGui::GetItemRectMax() - pos;
        GoxBox2(pos, size, ImVec4(0.3, 0.3, 0.3, 1), true, 0xFF);
        draw_list->ChannelsMerge();
        GoxBox2(pos, size, ImVec4(0, 0, 0, 1), false, 0xFF);
        memset(&g_group, 0, sizeof(g_group));
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
        bool ret, hovered;
        int corners = 0;
        ImVec2 uv0, uv1; // The position in the icon texture.
        int colorText = ImGui::ColorConvertFloat4ToU32(
                                        style.Colors[ImGuiCol_Text]);

        if (!tooltip) tooltip = name;
        ImGui::PushID(name);

        ret = ImGui::InvisibleButton(name, size);
        hovered = ImGui::IsItemHovered();

        if (g_group.nb) {
            if (g_group.i == 0) corners |= 0x1;
            if (g_group.i == g_group.col - 1) corners |= 0x2;
            if (g_group.i == g_group.nb - 1) corners |= 0x4;
            if (g_group.i % g_group.col == 0 &&
                    g_group.i >= g_group.nb - g_group.col) corners |= 0x8;
        }
        GoxBox(pos, size, *v, corners);

        if (tex) {
            uv0 = ImVec2((icon % 8) / 8.0, (icon / 8) / 8.0);
            uv1 = uv0 + ImVec2(1. / 8, 1. / 8);
            window->DrawList->AddImage((void*)tex, image_bb.Min, image_bb.Max,
                                       uv0, uv1, 0xFF000000);
        } else {
            window->DrawList->AddText(pos + ImVec2(3, 2), colorText, name);
        }
        if (ret) *v = !*v;
        if (hovered) {
            ImGui::SetTooltip("%s", tooltip);
            goxel_set_help_text(goxel, tooltip);
        }
        ImGui::PopID();
        if (g_group.nb) {
            g_group.i++;
            if ((g_group.i % g_group.col) != 0) ImGui::SameLine();
        }

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
        bool ret;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        ret = ImGui::CollapsingHeader(label, str_id, display_frame, default_open);
        if (ret) ImGui::Dummy(ImVec2(0, 4));
        ImGui::PopStyleColor();
        return ret;
    }

    bool GoxAction(const char *id, const char *label, const char *sig, ...)
    {
        va_list ap;
        assert(action_get(id));
        if (ImGui::Button(label)) {
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
                    0xFFFFFFFF);
            pos.y -= glyph->XAdvance;
        }
        ImGui::PopID();
        return ret;
    }

    // Copied from imgui, with some custom modifications.
    bool GoxDragFloat(const char* label, float* v, float v_speed,
            float v_min, float v_max, const char* display_format, float power)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const float w = CalcItemWidth();

        const ImVec2 label_size = CalcTextSize("", NULL, true);
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
            return InputScalarAsWidgetReplacement(frame_bb, label, ImGuiDataType_Float, v, id, decimal_precision);

        // Actual drag behavior
        ItemSize(total_bb, style.FramePadding.y);
        const bool value_changed = DragBehavior(frame_bb, id, v, v_speed, v_min, v_max, decimal_precision, power);

        // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
        char value_buf[64];
        const char* value_buf_end = value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), display_format, *v);
        RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(1.0f,0.5f));

        value_buf_end = value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), "%s:", label);
        RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.0f,0.5f));

        return value_changed;
    }

    bool GoxInputFloat(const char *label, float *v, float step,
                       float minv, float maxv, const char *format)
    {
        bool ret = false;
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindow();
        ImVec2 pos = window->DC.CursorPos;
        const ImGuiStyle& style = g.Style;
        const ImVec2 button_sz = ImVec2(
                g.FontSize, g.FontSize) + style.FramePadding * 2.0f;
        int button_flags =
            ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;

        GoxGroupBegin(1, 0);

        ImGui::PushID(label);

        // Set frame background to transparent.
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));

        GoxBox(pos, ImVec2(0, 18), false, 0xFF);
        if (ImGui::ButtonEx("-", button_sz, button_flags)) {
            (*v) -= step;
            ret = true;
        }
        ImGui::SameLine();

        ImGui::PushItemWidth(
                ImGui::GetContentRegionAvailWidth() -
                button_sz.x - style.ItemSpacing.x);

        ret |= ImGui::GoxDragFloat(label, v, step, minv, maxv, format, 1.0);

        ImGui::SameLine();
        ImGui::PopItemWidth();

        if (ImGui::ButtonEx("+", button_sz, button_flags)) {
            (*v) += step;
            ret = true;
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopID();
        GoxGroupEnd();

        return ret;
    }

    bool GoxInputInt(const char *format, int *v, int step, int minv, int maxv)
    {
        float vf = *v;
        bool ret = GoxInputFloat(format, &vf, 0.05, minv, maxv, "%.0f");
        if (ret) *v = vf;
        return ret;
    }
};

