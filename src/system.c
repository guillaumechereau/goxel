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

void sys_log(const char *msg)
{
    printf("%s\n", msg);
    fflush(stdout);
}

const char *sys_get_data_dir(void)
{
    return "./user_data";
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

