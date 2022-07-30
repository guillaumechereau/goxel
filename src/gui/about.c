#include "goxel.h"

int gui_about_popup(void *data)
{
    gui_text("Goxel2 " GOXEL_VERSION_STR);
    gui_text("a cross-platform 3D voxel art editor extendable via Lua.");
    return gui_button("OK", 0, 0);
}

