/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#ifdef __linux__
#define NOC_FILE_DIALOG_GTK
#define NOC_FILE_DIALOG_IMPLEMENTATION
#include "noc_file_dialog.h"
#include <pwd.h>
#endif

#ifdef WIN32
#define NOC_FILE_DIALOG_WIN32
#define NOC_FILE_DIALOG_IMPLEMENTATION
#include "noc_file_dialog.h"
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

void sys_log(const char *msg)
{
    printf("%s\n", msg);
    fflush(stdout);
}

// List all the files in a directory.
int sys_list_dir(const char *dirpath,
                 int (*callback)(const char *dirpath, const char *name,
                                 void *user),
                 void *user)
{
    DIR *dir;
    struct dirent *dirent;
    dir = opendir(dirpath);
    if (!dir) return -1;
    while ((dirent = readdir(dir))) {
        if (dirent->d_name[0] == '.') continue;
        if (callback(dirpath, dirent->d_name, user) != 0) break;
    }
    closedir(dir);
    return 0;
}

double sys_get_time(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (double)now.tv_sec + now.tv_usec / 1000000.0;
}

int sys_make_dir(const char *path)
{
    char tmp[PATH_MAX];
    char *p;
    strcpy(tmp, path);
    for (p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if ((mkdir(tmp, S_IRWXU) != 0) && (errno != EEXIST)) return -1;
        *p = '/';
    }
    return 0;
}

GLuint sys_get_screen_framebuffer(void)
{
    return 0;
}

#ifdef __linux__

#include <gtk/gtk.h>

const char *sys_get_user_dir(void)
{
    static char *ret = NULL;
    const char *home;
    if (!ret) {
        home = getenv("XDG_CONFIG_HOME");
        if (home) {
            asprintf(&ret, "%s/goxel", home);
        } else {
            home = getenv("HOME");
            if (!home) home = getpwuid(getuid())->pw_dir;
            asprintf(&ret, "%s/.config/goxel", home);
        }
    }
    return ret;
}

const char *sys_get_clipboard_text(void* user)
{
    GtkClipboard *cb;
    static gchar *text = NULL;
    gtk_init_check(NULL, NULL);
    g_free(text);
    cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    text = gtk_clipboard_wait_for_text(cb);
    return text;
}

void sys_set_clipboard_text(void *user, const char *text)
{
    GtkClipboard *cb;
    gtk_init_check(NULL, NULL);
    cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_can_store(cb, NULL, 0);
    gtk_clipboard_set_text(cb, text, -1);
    gtk_clipboard_store(cb);
}

#endif
