#include "goxel.h"

void gui_image_panel(void)
{
    image_t *image = goxel.image;
    float (*box)[4][4] = &image->box;
    gui_bbox(*box);
    gui_action_button(ACTION_img_auto_resize, "Auto resize", 0);
}

