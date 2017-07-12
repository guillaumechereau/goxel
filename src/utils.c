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
#include <dirent.h>

// Prevent warnings in stb code.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#ifdef WIN32
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "stb_image.h"
#include "stb_image_write.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

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
    const bool use_colors = !DEFINED(__APPLE__);
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

static const char* get_gl_error_text(int code) {
    switch (code) {
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return "undefined error";
    }
}

int check_gl_errors(const char *file, int line)
{
    int errors = 0;
    while (true)
    {
        GLenum x = glGetError();
        if (x == GL_NO_ERROR)
            return errors;
        LOG_E("%s:%d: OpenGL error: %d (%s)\n",
            file, line, x, get_gl_error_text(x));
        errors++;
    }
}

static void parse_gl_extensions(bool extensions[GOX_GL_EXTENSIONS_COUNT])
{
    static const char *NAMES[] = {
        [GOX_GL_QCOM_tiled_rendering]     = "GL_QCOM_tiled_rendering",
        [GOX_GL_OES_packed_depth_stencil] = "GL_OES_packed_depth_stencil",
        [GOX_GL_EXT_discard_framebuffer]  = "GL_EXT_discard_framebuffer",
    };
    _Static_assert(ARRAY_SIZE(NAMES) == GOX_GL_EXTENSIONS_COUNT, "");
    char *str, *token, *tmp = NULL;
    const int nb = GOX_GL_EXTENSIONS_COUNT;
    int i;
    str = strdup((const char*)glGetString(GL_EXTENSIONS));
    for (   token = strtok_r(str, " ", &tmp); token;
            token = strtok_r(NULL, " ", &tmp)) {
        for (i = 0; i < nb; i++) {
            if (strcmp(token, NAMES[i]) == 0) {
                extensions[i] = true;
                break;
            }
        }
    }
    for (i = 0; i < nb; i++)
        LOG_D("GL extension %s: %d", NAMES[i], extensions[i]);
    free(str);
}

bool _has_gl_extension(int extension)
{
    static bool *extensions = NULL;
    assert(extension < GOX_GL_EXTENSIONS_COUNT);
    if (!extensions) {
        extensions = calloc(GOX_GL_EXTENSIONS_COUNT, sizeof(*extensions));
        parse_gl_extensions(extensions);
    }
    return extensions[extension];
}

static int compile_shader(int shader, const char *code, const char *include)
{
    int status, len;
    char *log;
#ifndef GLES2
    const char *pre = "#define highp\n#define mediump\n#define lowp\n";
#else
    const char *pre = "";
#endif
    const char *sources[] = {pre, include, code};
    glShaderSource(shader, 3, (const char**)&sources, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        log = malloc(len + 1);
        LOG_E("Compile shader error:");
        glGetShaderInfoLog(shader, len, &len, log);
        LOG_E("%s", log);
        free(log);
        assert(false);
    }
    return 0;
}

int create_program(const char *vertex_shader_code,
                       const char *fragment_shader_code, const char *include)
{
    int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    include = include ? : "";
    assert(vertex_shader);
    if (compile_shader(vertex_shader, vertex_shader_code, include))
        return 0;
    int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    assert(fragment_shader);
    if (compile_shader(fragment_shader, fragment_shader_code, include))
        return 0;
    int prog = glCreateProgram();
    glAttachShader(prog, vertex_shader);
    glAttachShader(prog, fragment_shader);
    glLinkProgram(prog);
    int status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        LOG_E("Link Error");
        int len;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        char log[len];
        glGetProgramInfoLog(prog, len, &len, log);
        LOG_E("%s", log);
        return 0;
    }
    return prog;
}

void delete_program(int prog)
{
    int i;
    GLuint shaders[2];
    GLint count;
    if (DEBUG) {
        GL(glGetProgramiv(prog, GL_ATTACHED_SHADERS, &count));
        assert(count == 2);
    }
    GL(glGetAttachedShaders(prog, 2, NULL, shaders));
    for (i = 0; i < 2; i++)
        GL(glDeleteShader(shaders[i]));
    GL(glDeleteProgram(prog));
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

uint8_t *img_read_from_mem(const char *data, int size,
                           int *w, int *h, int *bpp)
{
    assert(*bpp >= 0 && *bpp <= 4);
    return stbi_load_from_memory((uint8_t*)data, size, w, h, bpp, *bpp);
}

uint8_t *img_read(const char *path, int *width, int *height, int *bpp)
{
    int size;
    char *data;
    bool need_to_free = false;
    uint8_t *img;

    if (str_startswith(path, "asset://")) {
        data = (char*)assets_get(path, &size);
    } else {
        data = read_file(path, &size);
        need_to_free = true;
    }
    if (!data) LOG_E("Cannot open image %s", path);
    assert(data);
    img = img_read_from_mem(data, size, width, height, bpp);
    if (need_to_free) free(data);
    return img;
}

void img_write(const uint8_t *img, int w, int h, int bpp, const char *path)
{
    stbi_write_png(path, w, h, bpp, img, 0);
}

uint8_t *img_write_to_mem(const uint8_t *img, int w, int h, int bpp, int *size)
{
    return stbi_write_png_to_mem((void*)img, 0, w, h, bpp, size);
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

int list_dir(const char *url, int flags, void *user,
             int (*f)(int i, const char *path, void *user))
{
    DIR *dir;
    struct dirent *dirent;
    char *path;
    int i = 0;
    dir = opendir(url);
    while ((dirent = readdir(dir))) {
        if (dirent->d_name[0] == '.') continue;
        asprintf(&path, "%s/%s", url, dirent->d_name);
        if (f(i, path, user) != 0) i++;
        free(path);
    }
    closedir(dir);
    return i;
}

void img_downsample(const uint8_t *img, int w, int h, int bpp,
                    uint8_t *out)
{
#define IX(x, y, k) ((((size_t)y) * w + (x)) * bpp + (k))
    int i, j, k;

    assert(w % 2 == 0 && h % 2 == 0);

    for (i = 0; i < h; i += 2)
    for (j = 0; j < w; j += 2)
    for (k = 0; k < bpp; k++) {
        out[(i * w / 4 + j / 2) * bpp + k] = (
                            img[IX(j + 0, i + 0, k)] +
                            img[IX(j + 0, i + 1, k)] +
                            img[IX(j + 1, i + 0, k)] +
                            img[IX(j + 1, i + 1, k)]) / 4;
    }
#undef IX
}

// Like gluUnproject.
vec3_t unproject(const vec3_t *win, const mat4_t *model,
                 const mat4_t *proj, const vec4_t *view)
{
    mat4_t inv;
    vec4_t p;
    inv = mat4_inverted(mat4_mul(*proj, *model));
    p = vec4((win->x - view->v[0]) / view->v[2] * 2 - 1,
             (win->y - view->v[1]) / view->v[3] * 2 - 1,
             2 * win->z - 1, 1);
    p = mat4_mul_vec(inv, p);
    if (p.w != 0) vec3_imul(&(p.xyz), 1 / p.w);
    return p.xyz;
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
