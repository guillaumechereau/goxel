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
        if (!ItemAdd(total_bb, id, &frame_bb))
        {
            ItemSize(total_bb, style.FramePadding.y);
            return false;
        }

        const bool hovered = ItemHoverable(frame_bb, id);
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
                max(g.FontSize * 2.0f, theme->sizes.item_height),
                g.FontSize + style.FramePadding.y * 2.0f);
        int button_flags =
            ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;
        float speed = step / 20;
        uint8_t color[4];

        ImGui::PushID(label);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));

        theme_get_color(THEME_GROUP_WIDGET, THEME_COLOR_TEXT, 0, color);
        ImGui::PushStyleColor(ImGuiCol_Text, imvec4(color));
        theme_get_color(THEME_GROUP_WIDGET, THEME_COLOR_INNER, 0, color);
        ImGui::PushStyleColor(ImGuiCol_Button, imvec4(color));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, imvec4(color));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
            color_lighten(imvec4(color), 1.2));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            color_lighten(imvec4(color), 1.2));


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

        ret |= ImGui::GoxDragFloat("", label, v, speed, minv, maxv, format, 1.0);
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

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();
        ImGui::PopID();

        if (ret)
            *v = clamp(*v, minv, maxv);

        return ret;
    }
};

