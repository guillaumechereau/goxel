#include "goxel.h"

void gui_debug_panel(void)
{
    mesh_global_stats_t stats;

    gui_text("FPS: %d", (int)round(goxel.fps));
    mesh_get_global_stats(&stats);
    gui_text("Nb meshes: %d", stats.nb_meshes);
    gui_text("Nb blocks: %d", stats.nb_blocks);
    gui_text("Mem: %dM", (int)(stats.mem / (1 << 20)));

    if (!DEFINED(GLES2)) {
        gui_checkbox_flag("Show wireframe", &goxel.view_effects,
                          EFFECT_WIREFRAME, NULL);
    }

    if (gui_button("Clear undo history", -1, 0)) {
        image_history_resize(goxel.image, 0);
    }
    if (gui_button("On low memory", -1, 0)) {
        goxel_on_low_memory();
    }
    if (gui_button("Test release", -1, 0)) {
        goxel.request_test_graphic_release = true;
    }

}

