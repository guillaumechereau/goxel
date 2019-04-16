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

#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef LOG_TIME
#   define LOG_TIME 1
#endif

static double get_log_time()
{
    static double origin = 0;
    double time;
    time = sys_get_time();
    if (!origin) origin = time;
    return time - origin;
}

void dolog(int level, const char *msg,
           const char *func, const char *file, int line, ...)
{
    const bool use_colors = !DEFINED(__APPLE__) && !DEFINED(__EMSCRIPTEN__);
    char *msg_formatted, *full_msg;
    const char *format;
    char time_str[32] = "";
    va_list args;

    va_start(args, line);
    vasprintf(&msg_formatted, msg, args);
    va_end(args);

    if (use_colors && level >= GOX_LOG_WARN) {
        format = "\e[33;31m%s%-60s\e[m %s (%s:%d)";
    } else {
        format = "%s%-60s %s (%s:%d)";
    }

    if (DEFINED(LOG_TIME))
        sprintf(time_str, "%.3f: ", get_log_time());

    file = file + max(0, (int)strlen(file) - 20); // Truncate file path.
    asprintf(&full_msg, format, time_str, msg_formatted, func, file, line);
    sys_log(full_msg);

    free(msg_formatted);
    free(full_msg);
}

double get_unix_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000.0 / 1000.0;
}

char *read_file(const char *path, int *size)
{
    FILE *file;
    char *ret = NULL;
    long read_size __attribute__((unused));
    int size_default;

    size = size ?: &size_default; // Allow to pass NULL as size;
    file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    *size = (int)ftell(file);
    fseek(file, 0, SEEK_SET);
    ret = malloc(*size + 1);
    read_size = fread(ret, *size, 1, file);
    assert(read_size == 1 || *size == 0);
    ret[*size] = '\0';
    fclose(file);
    return ret;
}

bool str_endswith(const char *str, const char *end)
{
    if (!str || !end) return false;
    if (strlen(str) < strlen(end)) return false;
    const char *start = str + strlen(str) - strlen(end);
    return strcmp(start, end) == 0;
}

bool str_startswith(const char *s1, const char *s2)
{
    if (!s1 || !s2) return false;
    if (strlen(s1) < strlen(s2)) return false;
    return strncmp(s1, s2, strlen(s2)) == 0;
}

// Like gluUnproject.
void unproject(const float win[3], const float model[4][4],
               const float proj[4][4], const float viewport[4],
               float out[3])
{
    float inv[4][4];
    float p[4];

    mat4_mul(proj, model, inv);
    mat4_invert(inv, inv);
    vec4_set(p, (win[0] - viewport[0]) / viewport[2] * 2 - 1,
                (win[1] - viewport[1]) / viewport[3] * 2 - 1,
                2 * win[2] - 1, 1);
    mat4_mul_vec4(inv, p, p);
    if (p[3]) vec3_imul(p, 1 / p[3]);
    vec3_copy(p, out);
}

int unix_to_dtf(double t, int *iy, int *im, int *id, int *h, int *m, int *s)
{
    time_t time = (int)t;
    struct tm *tm;
    tm = localtime(&time);
    if (!tm) return -1;
    *iy = tm->tm_year + 1900;
    *im = tm->tm_mon + 1;
    *id = tm->tm_mday;
    *h = tm->tm_hour;
    *m = tm->tm_min;
    *s = tm->tm_sec;
    return 0;
}

// From blender source code.
int utf_16_to_8(const wchar_t *in16, char *out8, size_t size8)
{
    char *out8end = out8 + size8;
    wchar_t u = 0;
    int err = 0;
    if (!size8 || !in16 || !out8) return -1;
    out8end--;

    for (; out8 < out8end && (u = *in16); in16++, out8++) {
        if (u < 0x0080) {
            *out8 = u;
        }
        else if (u < 0x0800) {
            if (out8 + 1 >= out8end) break;
            *out8++ = (0x3 << 6) | (0x1F & (u >> 6));
            *out8  = (0x1 << 7) | (0x3F & (u));
        }
        else if (u < 0xD800 || u >= 0xE000) {
            if (out8 + 2 >= out8end) break;
            *out8++ = (0x7 << 5) | (0xF & (u >> 12));
            *out8++ = (0x1 << 7) | (0x3F & (u >> 6));
            *out8  = (0x1 << 7) | (0x3F & (u));
        }
        else if (u < 0xDC00) {
            wchar_t u2 = *++in16;

            if (!u2) break;
            if (u2 >= 0xDC00 && u2 < 0xE000) {
                if (out8 + 3 >= out8end) break; else {
                    unsigned int uc = 0x10000 + (u2 - 0xDC00) + ((u - 0xD800) << 10);

                    *out8++ = (0xF << 4) | (0x7 & (uc >> 18));
                    *out8++ = (0x1 << 7) | (0x3F & (uc >> 12));
                    *out8++ = (0x1 << 7) | (0x3F & (uc >> 6));
                    *out8  = (0x1 << 7) | (0x3F & (uc));
                }
            }
            else {
                out8--; err = -1;
            }
        }
        else if (u < 0xE000) {
            out8--; err = -1;
        }
    }
    *out8 = *out8end = 0;
    if (*in16) err = -1;
    return err;
}
