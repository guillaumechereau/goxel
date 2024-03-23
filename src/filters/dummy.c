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
 * Dummy filter, just here for now until we have real filters.
 */

typedef struct {
    filter_t filter;
    int x;
} filter_dummy_t;

static int gui(filter_t *filter)
{
    filter_dummy_t *dummy = (void*)filter;
    gui_text_wrapped("Just a test of a filter that does nothing.");
    gui_input_int("X", &dummy->x, 0, 0);
    return 0;
}

FILTER_REGISTER(dummy, filter_dummy_t,
    .name = "Dummy",
    .gui_fn = gui,
)
