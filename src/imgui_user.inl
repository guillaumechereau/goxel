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
    bool GoxSelectable(const char *name, bool *v, int tex, int icon,
                       const char *tooltip) {
        ImGuiWindow* window = GetCurrentWindow();
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImVec2 size(32, 32);

        const ImVec2 padding = ImVec2(0, 0);//style.FramePadding;
        const ImRect image_bb(window->DC.CursorPos + padding,
                              window->DC.CursorPos + padding + size); 
        bool ret;
        ImVec2 uv0, uv1; // The position in the icon texture.
        ImVec4 color;

        if (!tooltip) tooltip = name;
        color = style.Colors[ImGuiCol_Button];
        if (*v) color = style.Colors[ImGuiCol_ButtonActive];
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushID(name);
        if (tex) {
            ret = ImGui::Button("", size);
            uv0 = ImVec2((icon % 8) / 8.0, (icon / 8) / 8.0);
            uv1 = uv0 + ImVec2(1. / 8, 1. / 8);
            window->DrawList->AddImage((void*)tex, image_bb.Min, image_bb.Max,
                                       uv0, uv1, 0xFFFFFFFF);
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
};
