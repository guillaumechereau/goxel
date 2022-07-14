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

