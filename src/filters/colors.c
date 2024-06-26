/* Goxel 3D voxels editor
 *
 * copyright (c) 2024-present Guillaume Chereau <guillaume@noctua-software.com>
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

/*
 * Filter to adjust the colors.
 */

typedef struct {
    filter_t filter;
    volume_t *original;
    float hue;
    float lightness;
    float saturation;
} filter_colors_t;

static float mod(float x, float y)
{
    while (x < 0) x += y;
    return fmod(x, y);
}

static void hsl_to_srgb(const float hsl[3], uint8_t rgb[3])
{
    float r = 0, g = 0, b = 0, c, x, m;
    const float h = hsl[0] / 60, s = hsl[1], l = hsl[2];
    c = (1 - fabs(2 * l - 1)) * s;
    x = c * (1 - fabs(fmod(h, 2) - 1));
    if      (h < 1) {r = c; g = x; b = 0;}
    else if (h < 2) {r = x; g = c; b = 0;}
    else if (h < 3) {r = 0; g = c; b = x;}
    else if (h < 4) {r = 0; g = x; b = c;}
    else if (h < 5) {r = x; g = 0; b = c;}
    else if (h < 6) {r = c; g = 0; b = x;}
    m = l - 0.5 * c;
    rgb[0] = (r + m) * 255;
    rgb[1] = (g + m) * 255;
    rgb[2] = (b + m) * 255;
}

static void srgb_to_hsl(const uint8_t rgb[3], float hsl[3])
{
    float h = 0, s, v, m, c, l;
    const float r = rgb[0] / 255.f, g = rgb[1] / 255.f, b = rgb[2] / 255.f;

    v = max3(r, g, b);
    m = min3(r, g, b);
    l = (v + m) / 2;
    c = v - m;
    if (c == 0) {
        hsl[0] = 0;
        hsl[1] = 0;
        hsl[2] = l;
        return;
    }
    if      (v == r) {h = (g - b) / c + (g < b ? 6 : 0);}
    else if (v == g) {h = (b - r) / c + 2;}
    else if (v == b) {h = (r - g) / c + 4;}
    h *= 60;
    s = (l > 0.5) ? c / (2 - v - m) : c / (v + m);
    hsl[0] = h;
    hsl[1] = s;
    hsl[2] = l;
}

static void on_open(filter_t *filter_)
{
    filter_colors_t *filter = (void*)filter_;
    filter->hue = 0;
    filter->lightness = 0;
    filter->saturation = 0;
    filter->original = volume_copy(goxel.image->active_layer->volume);
    image_history_push(goxel.image);
}

static void on_close(filter_t *filter_)
{
    filter_colors_t *filter = (void*)filter_;
    volume_delete(filter->original);
}

static void move_value(float *x, float v)
{
    float dst = v >= 0 ? 1 : 0;
    v = fabs(v);
    *x = mix(*x, dst, v);
}

static void apply_values(void *args, uint8_t color[4])
{
    filter_colors_t *filter = args;
    float hsl[3];

    srgb_to_hsl(color, hsl);
    hsl[0] = mod(hsl[0] + filter->hue, 360);
    move_value(&hsl[1], filter->saturation / 100);
    move_value(&hsl[2], filter->lightness / 100);
    hsl_to_srgb(hsl, color);
}

static int gui(filter_t *filter_)
{
    filter_colors_t *filter = (void*)filter_;
    layer_t *layer = goxel.image->active_layer;
    float hue = filter->hue;
    float lightness = filter->lightness;
    float saturation = filter->saturation;

    gui_input_float("Hue", &hue, 1, -180, +180, "%.1f");
    gui_input_float("Lightness", &lightness, 1, -100, +100, "%.1f");
    gui_input_float("Saturation", &saturation, 1, -100, +100, "%.1f");

    if (    hue != filter->hue ||
            lightness != filter->lightness ||
            saturation != filter->saturation)
    {
        volume_set(layer->volume, filter->original);
        filter->hue = hue;
        filter->lightness = lightness;
        filter->saturation = saturation;
        goxel_apply_color_filter(apply_values, filter);
    }

    if (gui_button(_(RESET), -1, 0)) {
        volume_set(layer->volume, filter->original);
        filter->hue = 0;
        filter->lightness = 0;
        filter->saturation = 0;
    }
    return 0;
}

FILTER_REGISTER(colors, filter_colors_t,
    .name = "Adjust Colors",
    .on_open = on_open,
    .on_close = on_close,
    .gui_fn = gui,
)
