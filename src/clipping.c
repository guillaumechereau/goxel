/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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

void clip_init(void)
{
    goxel.clipping.clip=true;

    float bbox[4][4];
    box_get_bbox(goxel.image->active_layer->box,bbox);
    vec3_copy(bbox[3],goxel.clipping.origin);
}

void clip_cancel()
{
    goxel.clipping.clip=false;
}

static layer_t *clip_layer(image_t *img, layer_t *layer)
{
    layer_t *clipped_layer;
    mesh_iterator_t iter;
    mesh_t *mesh2clip;
    mesh_accessor_t accessor;
    int vp[3];
    float p[3], p_vec[3];
    uint8_t new_value[4];

    img = img?:goxel.image;
    layer = layer?:img->active_layer;
    layer->visible=false; // Hide the layer being clipped

    clipped_layer = image_duplicate_layer(img,layer);
    mesh2clip = clipped_layer->mesh;
    vec4_set(new_value,0,0,0,0);
    iter = mesh_get_iterator(mesh2clip,MESH_ITER_VOXELS);

    accessor = mesh_get_accessor(mesh2clip);
    while (mesh_iter(&iter, vp)) {
        vec3_set(p, vp[0] + 0.5, vp[1] + 0.5, vp[2] + 0.5);
        vec3_sub(p,goxel.clipping.origin,p_vec);
        vec3_normalize(p_vec,p_vec);
        if (vec3_dot(p_vec, goxel.clipping.normal)>0)
            mesh_set_at(mesh2clip, &accessor, vp,new_value);
    }
    goxel.clipping.clip=false;
    // Update name 
    strcat(clipped_layer->name,"_clipped");
    return clipped_layer;
}

static void clipping_normal_x()
{
    vec3_set(goxel.clipping.normal, 1, 0, 0);
}
static void clipping_normal_y()
{
    vec3_set(goxel.clipping.normal, 0, 1, 0);
}
static void clipping_normal_z()
{
    vec3_set(goxel.clipping.normal, 0, 0, 1);
}
static void clipping_normal_cam()
{
    camera_t *camera = goxel.image->active_camera;
    float cam_normal[3];
    vec3_sub(camera->mat[3],goxel.clipping.origin, cam_normal);
    vec3_normalize(cam_normal,goxel.clipping.normal);
}
static void clipping_invert_normal()
{
    vec3_mul(goxel.clipping.normal,-1,goxel.clipping.normal);   
}
ACTION_REGISTER(clipping_invert_normal,
    .help = "Cliping normal is being inverted.",
    .cfunc = clipping_invert_normal,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)
ACTION_REGISTER(clipping_normal_x,
    .help = "Cliping normal is X axis",
    .cfunc = clipping_normal_x,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)
ACTION_REGISTER(clipping_normal_y,
    .help = "Cliping normal is Y axis",
    .cfunc = clipping_normal_y,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)
ACTION_REGISTER(clipping_normal_z,
    .help = "Cliping normal is Z axis",
    .cfunc = clipping_normal_z,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)
ACTION_REGISTER(clipping_normal_cam,
    .help = "Cliping normal is Camera direction",
    .cfunc = clipping_normal_cam,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)
ACTION_REGISTER(clip_init,
    .help = "Start clipping the current layer",
    .cfunc = clip_init,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)
ACTION_REGISTER(clip_layer,
    .help = "Clip the current layer",
    .cfunc = clip_layer,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)
ACTION_REGISTER(clip_cancel,
    .help = "Cancel clipping",
    .cfunc = clip_cancel,
    .csig = "vpp",
    .flags = ACTION_TOUCH_IMAGE,
)