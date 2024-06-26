/* Goxel 3D voxels editor
 *
 * copyright (c) 2024 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along
 * with goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"
#include "../ext_src/stb/stb_ds.h"

// stb array of registered filters.
static filter_t **g_filters = NULL;

static void a_filter_open(void *data)
{
    filter_t *filter = data;
    LOG_D("Open filter %s", filter->name);
    goxel.gui.current_filter = (filter_t*)data;
    if (filter->on_open) {
        filter->on_open(filter);
    }
}

void filter_register_(filter_t *filter)
{
    action_t action;
    action = (action_t) {
        .id = filter->action_id,
        .default_shortcut = filter->default_shortcut,
        .cfunc_data = a_filter_open,
        .data = (void*)filter,
        .flags = ACTION_CAN_EDIT_SHORTCUT,
    };
    action_register(&action, 0);
    arrput(g_filters, filter);
}


void filters_iter_all(
        void *arg, void (*f)(void *arg, const filter_t *filter))
{
    int i;
    for (i = 0; i < arrlen(g_filters); i++) {
        f(arg, g_filters[i]);
    }
}
