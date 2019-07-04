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

int gui_about_popup(void *data)
{
    gui_text("Goxel " GOXEL_VERSION_STR);
    gui_text("Copyright © 2015-2019 Guillaume Chereau");
    gui_text("<guillaume@noctua-software.com>");
    gui_text("All right reserved");
    if (!DEFINED(GOXEL_MOBILE)) gui_text("GPL 3 License");
    gui_text("http://guillaumechereau.github.io/goxel");

    if (gui_collapsing_header("Credits", true)) {
        gui_text("Code:");
        gui_text("● Guillaume Chereau <guillaume@noctua-software.com>");
        gui_text("● Dustin Willis Webber <dustin.webber@gmail.com>");
        gui_text("● Pablo Hugo Reda <pabloreda@gmail.com>");
        gui_text("● Othelarian (https://github.com/othelarian)");

        gui_text("Libraries:");
        gui_text("● dear imgui (https://github.com/ocornut/imgui)");
        gui_text("● stb (https://github.com/nothings/stb)");
        gui_text("● yocto-gl (https://github.com/xelatihy/yocto-gl)");
        gui_text("● uthash (https://troydhanson.github.io/uthash/)");
        gui_text("● inih (https://github.com/benhoyt/inih)");

        gui_text("Design:");
        gui_text("● Guillaume Chereau <guillaume@noctua-software.com>");
        gui_text("● Michal (https://github.com/YarlBoro)");
    }
    return gui_button("OK", 0, 0);
}

