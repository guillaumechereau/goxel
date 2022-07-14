#include "goxel.h"

void gui_light_panel(void)
{
    float v;

    gui_group_begin(NULL);
    gui_angle("Pitch", &goxel.rend.light.pitch, -90, +90);
    gui_angle("Yaw", &goxel.rend.light.yaw, 0, 360);
    gui_input_float("Intensity", &goxel.rend.light.intensity,
                    0.1, 0, 10, NULL);
    gui_group_end();
    gui_checkbox("Fixed", &goxel.rend.light.fixed, NULL);

    if (!DEFINED(GOXEL_NO_SHADOW)) {
        v = goxel.rend.settings.shadow;
        if (gui_input_float("Shadow", &v, 0.1, 0, 0, NULL)) {
            goxel.rend.settings.shadow = clamp(v, 0, 1);
        }
    }

    v = goxel.rend.settings.ambient;
    if (gui_input_float("Ambient", &v, 0.1, 0, 1, NULL)) {
        v = clamp(v, 0, 1);
        goxel.rend.settings.ambient = v;
    }
}
