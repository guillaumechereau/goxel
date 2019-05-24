/* Goxel 3D voxels editor
 *
 * copyright (c) 2019 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "goxel.h"

void gui_render_panel(void)
{
    int i;
    int maxsize;
    pathtracer_t *pt = &goxel.pathtracer;
    const char *path;
    float view_rect[4];

    goxel.no_edit |= pt->status;

    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize));
    maxsize /= 2; // Because png export already double it.
    goxel.show_export_viewport = true;
    gui_group_begin(NULL);
    i = goxel.image->export_width;
    if (gui_input_int("w", &i, 1, maxsize))
        goxel.image->export_width = clamp(i, 1, maxsize);
    i = goxel.image->export_height;
    if (gui_input_int("h", &i, 1, maxsize))
        goxel.image->export_height = clamp(i, 1, maxsize);
    if (gui_button("Fit screen", 1, 0)) {
        gui_get_view_rect(view_rect);
        goxel.image->export_width = view_rect[2];
        goxel.image->export_height = view_rect[3];
    }
    if (gui_button("Set output", 1, 0)) {
        path = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "png\0*.png\0", NULL,
                                    "untitled.png");
        if (path)
            snprintf(pt->output, sizeof(pt->output), "%s", path);
    }
    gui_group_end();
    if (*pt->output) gui_text("%s", pt->output);

    gui_input_int("Samples", &pt->num_samples, 1, 10000);

    if (pt->status == PT_STOPPED && gui_button("Start", 1, 0))
        pt->status = PT_RUNNING;
    if (pt->status == PT_RUNNING && gui_button("Stop", 1, 0)) {
        pathtracer_stop(pt);
        pt->status = PT_STOPPED;
    }
    if (pt->status == PT_FINISHED && gui_button("Restart", 1, 0)) {
        pt->status = PT_RUNNING;
        pt->progress = 0;
        pt->force_restart = true;
    }
    if (pt->status) {
        gui_text("%d/100", (int)(pt->progress * 100));
    }

    if (gui_collapsing_header("World", false)) {
        gui_push_id("world");
        gui_group_begin(NULL);
        gui_selectable_toggle("None", &pt->world.type, PT_WORLD_NONE,
                              NULL, -1);
        gui_selectable_toggle("Uniform", &pt->world.type, PT_WORLD_UNIFORM,
                              NULL, -1);
        gui_selectable_toggle("Sky", &pt->world.type, PT_WORLD_SKY,
                              NULL, -1);
        gui_group_end();
        if (pt->world.type) {
            gui_input_float("Energy", &pt->world.energy, 0.1, 0, 10, "%.1f");
            gui_color_small("Color", pt->world.color);
        }
        gui_pop_id();
    }
    if (gui_collapsing_header("Floor", false)) {
        gui_push_id("floor");
        gui_group_begin(NULL);
        gui_selectable_toggle("None", &pt->floor.type, PT_FLOOR_NONE,
                              NULL, -1);
        gui_selectable_toggle("Plane", &pt->floor.type, PT_FLOOR_PLANE,
                              NULL, -1);
        gui_group_end();

        gui_group_begin("size");
        gui_input_int("x", &pt->floor.size[0], 1, 2048);
        gui_input_int("y", &pt->floor.size[1], 1, 2048);
        gui_group_end();

        gui_color_small("Color", pt->floor.color);
        gui_input_float("Diffuse", &pt->floor.diffuse, 0.1, 0, 1, "%.1f");
        gui_input_float("Specular", &pt->floor.specular, 0.01, 0, 1, "%.3f");
        gui_pop_id();
    }
    if (gui_collapsing_header("Light", false)) {
        gui_group_begin("Light");
        gui_angle("Pitch", &goxel.rend.light.pitch, -90, +90);
        gui_angle("Yaw", &goxel.rend.light.yaw, 0, 360);
        gui_checkbox("Fixed", &goxel.rend.light.fixed, NULL);
        gui_input_float("Intensity", &goxel.rend.light.intensity,
                        0.1, 0, 10, NULL);
        gui_group_end();
    }
}

