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

#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdbool.h>

enum {
    EFFECT_RENDER_POS       = 1 << 1,
    EFFECT_BORDERS          = 1 << 3,
    EFFECT_SEMI_TRANSPARENT = 1 << 5,
    EFFECT_SEE_BACK         = 1 << 6,
    EFFECT_MARCHING_CUBES   = 1 << 7,
    EFFECT_SHADOW_MAP       = 1 << 8,
    EFFECT_MC_SMOOTH        = 1 << 9,

    // For render box.
    EFFECT_NO_SHADING       = 1 << 10,
    EFFECT_STRIP            = 1 << 11,
    EFFECT_WIREFRAME        = 1 << 12,
    EFFECT_GRID             = 1 << 13,
    EFFECT_EDGES            = 1 << 14,
    EFFECT_GRID_ONLY        = 1 << 15,

    EFFECT_PROJ_SCREEN      = 1 << 16, // Image project in screen.
    EFFECT_ANTIALIASING     = 1 << 17,
    EFFECT_UNLIT            = 1 << 18,
};

typedef struct {
    float ambient;
    float smoothness;
    float shadow;
    int   effects;
    float occlusion_strength;
} render_settings_t;

typedef struct renderer renderer_t;
typedef struct render_item_t render_item_t;
struct renderer
{
    float view_mat[4][4];
    float proj_mat[4][4];
    int    fbo;     // The renderer target framebuffer.
    float  scale;   // For retina display.

    struct {
        float  pitch;
        float  yaw;
        bool   fixed;       // If set, then the light moves with the view.
        float  intensity;
    } light;

    render_settings_t settings;

    render_item_t    *items;
};

void render_init(void);
void render_deinit(void);
void render_mesh(renderer_t *rend, const mesh_t *mesh,
                 const material_t *material,
                 int effects);
void render_grid(renderer_t *rend, const float plane[4][4],
                 const uint8_t color[4], const float clip_box[4][4]);
void render_line(renderer_t *rend, const float a[3], const float b[3],
                 const uint8_t color[4], int effects);
void render_box(renderer_t *rend, const float box[4][4],
                const uint8_t color[4], int effects);
void render_sphere(renderer_t *rend, const float mat[4][4]);
void render_img(renderer_t *rend, texture_t *tex, const float mat[4][4],
                int efffects);

/*
 * Function: render_img2
 * Render an image directly from it's pixel data.
 *
 * XXX: this is experimental: eventually I think we should remove render_img
 * and only user render_img2 (renamed to render_img).
 */
void render_img2(renderer_t *rend,
                 const uint8_t *data, int w, int h, int bpp,
                 const float mat[4][4], int effects);

void render_rect(renderer_t *rend, const float plane[4][4], int effects);
// Flushes all the queued render items.  Actually calls opengl.
//  rect: the viewport rect (passed to glViewport).
//  clear_color: clear the screen with this first.
void render_submit(renderer_t *rend, const float viewport[4],
                   const uint8_t clear_color[4]);
// Compute the light direction in the model coordinates (toward the light)
void render_get_light_dir(const renderer_t *rend, float out[3]);

// Ugly function that return the position of the block at a given id
// when the mesh is rendered with render_mesh.
void render_get_block_pos(renderer_t *rend, const mesh_t *mesh,
                          int id, int pos[3]);

// Attempt to release some memory.
void render_on_low_memory(renderer_t *rend);

#endif // RENDER_H
