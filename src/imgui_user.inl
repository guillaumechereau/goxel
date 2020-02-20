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

    inline bool TempInputTextIsActive(ImGuiID id) {
        ImGuiContext& g = *GImGui;
        return (g.ActiveId == id && g.ScalarAsInputTextId == id);
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
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        if (ImGui::ButtonEx("▶", button_sz, button_flags)) {
            (*v) += step;
            ret = true;
        }
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1);

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        ImGui::PopID();

        if (ret)
            *v = clamp(*v, minv, maxv);

        return ret;
    }
};

