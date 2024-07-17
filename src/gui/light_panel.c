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

void gui_light_panel(void)
{
    float v;
    char buf[256];

    gui_group_begin(NULL);
    snprintf(buf, sizeof(buf), "%s: X", _("Angle"));
    gui_angle(buf, &goxel.rend.light.pitch, -90, +90);
    gui_angle("Z", &goxel.rend.light.yaw, 0, 360);
    gui_group_end();
    gui_input_float(_("Intensity"), &goxel.rend.light.intensity,
                    0.1, 0, 10, NULL);
    gui_checkbox(_("Fixed"), &goxel.rend.light.fixed, NULL);

    if (!DEFINED(GOXEL_NO_SHADOW)) {
        v = goxel.rend.settings.shadow;
        if (gui_input_float(_("Shadow"), &v, 0.1, 0, 0, NULL)) {
            goxel.rend.settings.shadow = clamp(v, 0, 1);
        }
    }

    v = goxel.rend.settings.ambient;
    if (gui_input_float(_("Environment"), &v, 0.1, 0, 1, NULL)) {
        v = clamp(v, 0, 1);
        goxel.rend.settings.ambient = v;
    }
}
