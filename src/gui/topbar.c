#include "goxel.h"

#ifndef GUI_CUSTOM_TOPBAR

static int gui_mode_select(void)
{

    gui_choice_begin("Mode", &goxel.painter.mode, false);
    gui_choice("Add", MODE_OVER, ICON_MODE_ADD);
    gui_choice("Sub", MODE_SUB, ICON_MODE_SUB);
    gui_choice("Paint", MODE_PAINT, ICON_MODE_PAINT);
    gui_choice_end();
    return 0;
}

void gui_top_bar(void)
{
    gui_action_button(ACTION_undo, NULL, 0);
    gui_same_line();
    gui_action_button(ACTION_redo, NULL, 0);
    gui_same_line();
    gui_action_button(ACTION_layer_clear, NULL, 0);
    gui_same_line();
    gui_mode_select();
    gui_same_line();
    gui_color("##color", goxel.painter.color);
}

#endif // GUI_CUSTOM_TOPBAR
