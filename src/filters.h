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

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FILTERS_H
#define FILTERS_H

typedef struct filter filter_t;

struct filter {
    int (*gui_fn)(filter_t *filter);
    void (*on_open)(filter_t *filter);
    void (*on_close)(filter_t *filter);
    const char *name;
    const char *action_id;
    const char *default_shortcut;
};

#define FILTER_REGISTER(id_, klass_, ...) \
    static klass_ GOX_filter_##id_ = {\
            .filter = { \
                .action_id = "filter_open_" #id_, __VA_ARGS__ \
            } \
        }; \
    __attribute__((constructor)) \
    static void GOX_register_filter_##id_(void) { \
        filter_register_(&GOX_filter_##id_.filter); \
    }

void filter_register_(filter_t *filter);

/**
 * Iter all the registered filters
 */
void filters_iter_all(
        void *arg, void (*f)(void *arg, const filter_t *filter));

#endif // FILTERS_H
