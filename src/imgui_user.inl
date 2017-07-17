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

static inline ImVec4 color_lighten(ImVec4 c, float k)
{
    c.x *= k;
    c.y *= k;
    c.z *= k;
    return c;
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

    void GoxBox2(ImVec2 pos, ImVec2 size, ImVec4 color, bool fill,
                 int rounding_corners_flags = ~0)
    {
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        ImGuiWindow* window = GetCurrentWindow();
        float r = style.FrameRounding;
        size.x = size.x ?: ImGui::GetContentRegionAvailWidth();
        if (fill) {
            window->DrawList->AddRectFilled(
                    pos, pos + size,
                    ImGui::ColorConvertFloat4ToU32(color), r,
                    rounding_corners_flags);
        } else {
            window->DrawList->AddRect(
                    pos, pos + size,
                    ImGui::ColorConvertFloat4ToU32(color), r,
                    rounding_corners_flags);
        }
    }

    void GoxBox(ImVec2 pos, ImVec2 size, bool selected,
                int rounding_corners_flags = ~0)
    {
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        ImVec4 color  = style.Colors[selected ? ImGuiCol_ButtonActive :
                                     ImGuiCol_Button];
        return GoxBox2(pos, size, color, true, rounding_corners_flags);
    }

    bool GoxSelectable(const char *name, bool *v, int tex, int icon,
                       const char *tooltip, ImVec2 size) {
        const theme_t *theme = theme_get();
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
        uvec4b_t color;

        if (!tooltip) tooltip = name;
        ImGui::PushID(name);

        color = (*v) ? theme->colors.inner_selected : theme->colors.inner;
        PushStyleColor(ImGuiCol_Button, uvec4b_to_imvec4(color));
        PushStyleColor(ImGuiCol_ButtonHovered, color_lighten(
                    style.Colors[ImGuiCol_Button], 1.2));
        color = (*v) ? theme->colors.text_selected : theme->colors.text;
        PushStyleColor(ImGuiCol_Text, uvec4b_to_imvec4(color));

        if (tex) {
            ret = ImGui::Button("", size);
            uv0 = ImVec2((icon % 8) / 8.0, (icon / 8) / 8.0);
            uv1 = uv0 + ImVec2(1. / 8, 1. / 8);
            window->DrawList->AddImage((void*)tex, image_bb.Min, image_bb.Max,
                                       uv0, uv1, 0xFF000000);
        } else {
            ret = ImGui::Button(name, size);
        }
        ImGui::PopStyleColor(3);
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
        ImVec2 ra, rb;
        bool ret, highlight, hovered, held;
        ImRect bb;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGuiWindow* window = GetCurrentWindow();
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID("#colorbutton");
        bb = ImRect(window->DC.CursorPos, window->DC.CursorPos +
                    ImVec2(20, 15));
        highlight = color->r == target->r &&
                    color->g == target->g &&
                    color->b == target->b &&
                    color->a == target->a;

        c = uvec4b_to_imvec4(*color);
        ItemSize(bb, 0.0);
        ret = ButtonBehavior(bb, id, &hovered, &held);
        RenderFrame(bb.Min, bb.Max, GetColorU32(c), false,
                    style.FrameRounding);
        if (highlight)
            draw_list->AddRect(bb.Min, bb.Max, 0xFFFFFFFF, 0, 0, 1);
        if (ret) *target = *color;
        window->DC.LastItemRect = bb;
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
        const theme_t *theme = theme_get();
        uvec4b_t color;
        ImVec2 text_size = CalcTextSize(text);
        ImGuiWindow* window = GetCurrentWindow();
        ImVec2 pos = window->DC.CursorPos + ImVec2(pad, text_size.x + pad);

        color = (*v) ? theme->colors.background : theme->colors.tabs;
        ImGui::PushStyleColor(ImGuiCol_Button, uvec4b_to_imvec4(color));

        ImGui::PushID(text);

        ret = InvisibleButton("", ImVec2(text_size.y + pad * 2,
                                         text_size.x + pad * 2));
        GoxBox(GetItemRectMin(), GetItemRectSize(), false, 0x09);

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
        RenderTextClipped(frame_bb.Min, frame_bb.Max - ImVec2(30, 0), value_buf, value_buf_end, NULL, ImVec2(0.0f,0.5f));

        return value_changed;
    }

    bool GoxInputFloat(const char *label, float *v, float step,
                       float minv, float maxv, const char *format)
    {
        const theme_t *theme = theme_get();
        bool ret = false;
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImVec2 button_sz = ImVec2(
                g.FontSize * 2.0f, g.FontSize + style.FramePadding.y * 2.0f);
        int button_flags =
            ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;
        float speed = step / 20;

        ImGui::PushID(label);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));

        ImGui::SetWindowFontScale(0.75);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        if (ImGui::ButtonEx("◀", button_sz, button_flags)) {
            (*v) -= step;
            ret = true;
        }
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1);

        ImGui::SameLine();
        ImGui::PushItemWidth(
                ImGui::GetContentRegionAvailWidth() -
                button_sz.x - style.ItemSpacing.x);

        ImGui::PushStyleColor(ImGuiCol_FrameBg,
                              uvec4b_to_imvec4(theme->colors.inner));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
                              color_lighten(
                                  uvec4b_to_imvec4(theme->colors.inner), 1.2));

        ret |= ImGui::GoxDragFloat("", label, v, speed, minv, maxv, format, 1.0);
        ImGui::PopStyleColor(2);
        ImGui::PopItemWidth();

        ImGui::SameLine();

        ImGui::SetWindowFontScale(0.75);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        if (ImGui::ButtonEx("▶", button_sz, button_flags)) {
            (*v) += step;
            ret = true;
        }
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1);

        ImGui::PopStyleVar();
        ImGui::PopID();

        if (ret)
            *v = clamp(*v, minv, maxv);

        return ret;
    }
};

