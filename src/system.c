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

#ifdef __linux__
#define NOC_FILE_DIALOG_GTK
#define NOC_FILE_DIALOG_IMPLEMENTATION
#include "noc_file_dialog.h"
#include <pwd.h>
#include <sys/stat.h>
#include <errno.h>
#endif

#ifdef WIN32
#define NOC_FILE_DIALOG_WIN32
#define NOC_FILE_DIALOG_IMPLEMENTATION
#include "noc_file_dialog.h"
#endif

void sys_log(const char *msg)
{
    printf("%s\n", msg);
    fflush(stdout);
}

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

double sys_get_time(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (double)now.tv_sec + now.tv_usec / 1000000.0;
}

#ifndef __APPLE__

bool sys_asset_exists(const char *path)
{
    FILE *file;
    file = fopen(path, "r");
    if (file)
        fclose(file);
    return (bool)file;
}



char *sys_read_asset(const char *path, int *size)
{
    FILE *file;
    int read_size __attribute__((unused));
    char *ret;
    int default_size;
    size = size ?: &default_size;

    file = fopen(path, "rb");
    if (!file) LOG_E("cannot file file %s", path);
    assert(file);
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    ret = malloc(*size + 1);
    read_size = fread(ret, *size, 1, file);
    assert(read_size == 1 || *size == 0);
    fclose(file);
    ret[*size] = '\0';
    return ret;
}

#else
#include <CoreFoundation/CoreFoundation.h>

bool sys_asset_exists(const char *path)
{
    CFBundleRef mainBundle;
    CFURLRef url;
    mainBundle = CFBundleGetMainBundle();
    url = CFBundleCopyResourceURL(mainBundle,
                                  CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8),
                                  nil,
                                  NULL);
    return url != nil;
}

char *sys_read_asset(const char *path, int *size)
{
    FILE *file;
    int read_size __attribute__((unused));
    char *ret;
    int default_size;
    CFBundleRef main_bundle;
    CFURLRef url;
    CFStringRef asset_path;
    CFStringEncoding encodingMethod;
    main_bundle = CFBundleGetMainBundle();
    url = CFBundleCopyResourceURL(main_bundle,
                                  CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8),
                                  nil,
                                  NULL);
    asset_path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    encodingMethod = CFStringGetSystemEncoding();
    path = CFStringGetCStringPtr(asset_path, encodingMethod);

    size = size ?: &default_size;
    file = fopen(path, "rb");
    if (!file) LOG_E("cannot file file %s", path);
    assert(file);
    fseek(file, 0, SEEK_END);
    *size = (int)ftell(file);
    fseek(file, 0, SEEK_SET);
    ret = malloc(*size + 1);
    read_size = (int)fread(ret, *size, 1, file);
    assert(read_size == 1 || *size == 0);
    fclose(file);
    ret[*size] = '\0';
    return ret;
}

#endif

GLuint sys_get_screen_framebuffer(void)
{
    return 0;
}

#ifdef __linux__

#include <gtk/gtk.h>

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
