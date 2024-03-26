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

/*
 * Section: System
 * Some system related functions.  All the things that might depend on the
 * operating system, not relying on libc, should go there.
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Global structure that hold pointers to system functions.  Need to be
 * set at init time.
 */
typedef struct {
    void *user;
    void (*log)(void *user, const char *msg);
    void (*set_window_title)(void *user, const char *title);
    const char *(*get_user_dir)(void *user);
    const char *(*get_global_dir)(void *user);
    const char *(*get_clipboard_text)(void* user);
    void (*set_clipboard_text)(void *user, const char *text);
    void (*show_keyboard)(void *user, bool has_text);
    void (*save_to_photos)(void *user, const uint8_t *data, int size,
                           void (*on_finished)(int r));

    bool (*open_dialog)(void *user, char *buf, size_t buf_size,
                        int flags, // 1: save, 2: folder.
                        const char *title,
                        const char *default_path_and_file,
                        int nb_filters, const char * const *filters,
                        const char *filters_desc);
} sys_callbacks_t;
extern sys_callbacks_t sys_callbacks;

/*
 * Function: sys_log
 * Write a log message to output.
 *
 * Note: this should never be called directly, use the LOG_ macros instead.
 */
void sys_log(const char *msg);

/*
 * Function: sys_list_dir
 * List all the files in a directory.
 *
 * Parameters:
 *   dir      - Path of the directory we want to list.
 *   callback - Function called once per entry.
 *   user     - User data passed to the callback.
 *
 * Return:
 *   The number of files found.
 */
int sys_list_dir(const char *dir,
                 int (*callback)(const char *dir, const char *name,
                                 void *user),
                 void *user);

/*
 * Function: sys_delete_file
 * Delete a file from the system.
 */
int sys_delete_file(const char *path);

/*
 * Function: sys_get_user_dir
 * Return the user config directory for goxel
 *
 * On linux, this should be $HOME/.config/goxel.
 */
const char *sys_get_user_dir(void);

/*
 * Function: sys_get_global_dir
 * Return the global config directory for goxel
 *
 * On linux, this should be /etc/goxel.
 */
const char *sys_get_global_dir(void);

/*
 * Function: sys_make_dir
 * Create all the directories parent of a given file path if they do not
 * exist yet.
 *
 * For example, sys_make_dir("/a/b/c.txt") will create /a/ and /a/b/.
 */
int sys_make_dir(const char *path);

const char *sys_get_clipboard_text(void* user);
void sys_set_clipboard_text(void *user, const char *text);
int sys_get_screen_framebuffer(void);

/*
 * Function: sys_get_time
 * Return the unix time (seconds since Jan 01 1970).
 */
double sys_get_time(void); // Unix time.

/*
 * Function: sys_set_window_title
 * Set the window title.
 */
void sys_set_window_title(const char *title);

/*
 * Function: sys_show_keyboard
 * Show a virtual keyboard if needed.
 */
void sys_show_keyboard(bool has_text);

/*
 * Function: sys_save_to_photo
 * Save a png file to the system photo album.
 */
void sys_save_to_photos(const uint8_t *data, int size,
                        void (*on_finished)(int r));

/*
 * Function: sys_get_save_path
 * Get the path where to save an image.  By default this opens a file dialog.
 */
const char *sys_get_save_path(const char *default_name,
                              const char * const*filters,
                              const char *filters_desc);

/*
 * Function: sys_on_saved
 * Called after we saved a file
 */
void sys_on_saved(const char *path);

const char *sys_open_file_dialog(const char *title,
                                 const char *default_path_and_file,
                                 const char * const *filters,
                                 const char *filters_desc);

const char *sys_open_folder_dialog(const char *title,
                                   const char *default_path);

const char *sys_save_file_dialog(const char *title,
                                 const char *default_path_and_file,
                                 const char * const *filters,
                                 const char *filters_desc);

#endif // SYSTEM_H
