#include "goxel.h"

static int gui_quit_popup(void *data)
{
    gui_text("Quit without saving your changes?");
    if (gui_button("Quit", 0, 0)) {
        goxel.quit = true;
        return 1;
    }
    gui_same_line();
    if (gui_button("Cancel", 0, 0))
        return 2;
    return 0;
}

void gui_query_quit(void)
{
    if (image_get_key(goxel.image) == goxel.image->saved_key) {
        goxel.quit = true;
        return;
    }
    gui_open_popup("Unsaved changes", GUI_POPUP_RESIZE, NULL, gui_quit_popup);
}
