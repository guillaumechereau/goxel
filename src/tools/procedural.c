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

enum {
    STATE_IDLE      = 0,
    STATE_CANCEL    = 1,
    STATE_END       = 2,
    STATE_ENTER     = 0x0100,

    STATE_SNAPED,
    STATE_PAINT,
};

int tool_procedural_iter(goxel_t *goxel, const inputs_t *inputs,
                         int state, const vec2_t *view_size,
                         bool inside)
{
    int snaped = 0;
    vec3_t pos, normal;
    box_t box;
    gox_proc_t *proc = &goxel->proc;
    const bool down = inputs->mouse_down[0];

    if (proc->state == PROC_PARSE_ERROR) return 0;

    // XXX: duplicate code with tool_brush_iter.
    if (inside)
        snaped = goxel_unproject(
                goxel, view_size, &inputs->mouse_pos,
                goxel->painter.mode == MODE_ADD && !goxel->snap_offset,
                &pos, &normal);
    if (snaped) {
        if (goxel->tool == TOOL_BRUSH && goxel->snap_offset)
            vec3_iaddk(&pos, normal, goxel->snap_offset * goxel->tool_radius);
        pos.x = round(pos.x - 0.5) + 0.5;
        pos.y = round(pos.y - 0.5) + 0.5;
        pos.z = round(pos.z - 0.5) + 0.5;
        box = bbox_from_extents(pos, 0.5, 0.5, 0.5);
        render_box(&goxel->rend, &box, NULL, EFFECT_WIREFRAME);
    }

    switch (state) {
    case STATE_IDLE:
        if (snaped) return STATE_SNAPED;
        break;

    case STATE_SNAPED:
        if (!snaped) return STATE_IDLE;
        if (down) {
            image_history_push(goxel->image);
            proc_stop(proc);
            proc_start(proc, &box);
            return STATE_PAINT;
        }
        break;

    case STATE_PAINT:
        if (!down) return STATE_IDLE;
        break;
    }

    return state;
}

TOOL_REGISTER(TOOL_PROCEDURAL, procedural,
              .iter_fn = tool_procedural_iter,
)
