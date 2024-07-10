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
    char buf[256];
    pathtracer_t *pt = &goxel.pathtracer;
    material_t *material;

    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize));
    maxsize /= 2; // Because png export already double it.
    goxel.show_export_viewport = true;
    gui_group_begin(NULL);
    gui_checkbox(_(SIZE), &goxel.image->export_custom_size, NULL);
    if (!goxel.image->export_custom_size) {
        goxel.image->export_width = goxel.gui.viewport[2];
        goxel.image->export_height = goxel.gui.viewport[3];
    }
    gui_enabled_begin(goxel.image->export_custom_size);
    i = goxel.image->export_width;
    if (gui_input_int("w", &i, 0, 0))
        goxel.image->export_width = clamp(i, 1, maxsize);
    i = goxel.image->export_height;
    if (gui_input_int("h", &i, 0, 0))
        goxel.image->export_height = clamp(i, 1, maxsize);
    gui_enabled_end();
    gui_group_end();

    if (gui_input_int(_(SAMPLES), &pt->num_samples, 0, 0))
        pt->num_samples = clamp(pt->num_samples, 1, 10000);

    if (pt->status == PT_STOPPED && gui_button(_(START), 1, 0))
        pt->status = PT_RUNNING;
    if (pt->status == PT_RUNNING && gui_button(_(STOP), 1, 0)) {
        pathtracer_stop(pt);
        pt->status = PT_STOPPED;
    }
    if (pt->status == PT_FINISHED && gui_button(_(RESTART), 1, 0)) {
        pt->status = PT_RUNNING;
        pt->samples = 0;
        pt->force_restart = true;
    }
    if (pt->status) {
        gui_text("%d/%d (%d%%)", pt->samples, pt->num_samples,
                 pt->samples * 100 / pt->num_samples);
    }
    if (    pt->status == PT_FINISHED &&
            gui_button(_(SAVE), -1, 0))
    {
        action_exec2(ACTION_export_render_buf_to_photos);
    }

    if (gui_section_begin(_(WORLD), GUI_SECTION_COLLAPSABLE_CLOSED)) {
        gui_group_begin(NULL);
        gui_selectable_toggle(_(NONE), &pt->world.type, PT_WORLD_NONE,
                              NULL, -1);
        gui_selectable_toggle(_(UNIFORM), &pt->world.type, PT_WORLD_UNIFORM,
                              NULL, -1);
        gui_selectable_toggle(_(SKY), &pt->world.type, PT_WORLD_SKY,
                              NULL, -1);
        gui_group_end();
        if (pt->world.type) {
            gui_input_float(_(INTENSITY), &pt->world.energy,
                            0.1, 0, 10, "%.1f");
            gui_color_small(_(COLOR), pt->world.color);
        }
    } gui_section_end();

    if (gui_section_begin(_(FLOOR), GUI_SECTION_COLLAPSABLE_CLOSED)) {
        gui_group_begin(NULL);
        gui_selectable_toggle(_(NONE), &pt->floor.type, PT_FLOOR_NONE,
                              NULL, -1);
        gui_selectable_toggle(_(PLANE), &pt->floor.type, PT_FLOOR_PLANE,
                              NULL, -1);
        gui_group_end();

        if (pt->floor.type == PT_FLOOR_PLANE) {
            gui_input_int("w", &pt->floor.size[0], 0, 0);
            gui_input_int("h", &pt->floor.size[1], 0, 0);
            pt->floor.size[0] = fmax(pt->floor.size[0], 1);
            pt->floor.size[1] = fmax(pt->floor.size[1], 1);
            gui_color_small(_(COLOR), pt->floor.color);

            gui_text(_(MATERIAL));
            if (gui_combo_begin("##material",
                    pt->floor.material ? pt->floor.material->name : NULL)) {
                DL_FOREACH(goxel.image->materials, material) {
                    if (gui_combo_item(material->name,
                            material == pt->floor.material)) {
                        pt->floor.material = material;
                    }
                }
                gui_combo_end();
            }
        }
    } gui_section_end();

    if (gui_section_begin(_(LIGHT), GUI_SECTION_COLLAPSABLE_CLOSED)) {
        snprintf(buf, sizeof(buf), "%s: X", _(ANGLE));
        gui_angle(buf, &goxel.rend.light.pitch, -90, +90);
        gui_angle("Z", &goxel.rend.light.yaw, 0, 360);
        gui_checkbox(_(FIXED), &goxel.rend.light.fixed, NULL);
        gui_input_float(_(INTENSITY), &goxel.rend.light.intensity,
                        0.1, 0, 10, NULL);
    } gui_section_end();
}

static void on_saved_to_photo(int ret) {
    gui_alert(_(EXPORT), _(SUCCESS));
}

static void export_render_buf_to_photos(void)
{
    int w = goxel.pathtracer.w;
    int h = goxel.pathtracer.h;
    int bpp = 4;
    int size;
    uint8_t *img;
    img = img_write_to_mem(goxel.pathtracer.buf, w, h, bpp, &size);
    sys_save_to_photos(img, size, on_saved_to_photo);
    free(img);
}

ACTION_REGISTER(ACTION_export_render_buf_to_photos,
    .cfunc = export_render_buf_to_photos,
)
