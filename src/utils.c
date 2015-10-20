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

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
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
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time = tv.tv_sec + (double)tv.tv_usec / (1000 * 1000);
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
        sprintf(time_str, "%f: ", get_log_time());

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

static unsigned int gox__rand_seed = 1;
unsigned int gox_rand(void)
{
    gox__rand_seed = gox__rand_seed * 2147001325 + 715136305;
    return 0x31415926 ^ ((gox__rand_seed >> 16) + (gox__rand_seed << 16));
}

float gox_frand(float min, float max)
{
    double r = gox_rand() / ((double) (1 << 16) * (1 << 16));
    return min + r * (max - min);
}

void gox_rand_seed(unsigned int v)
{
    gox__rand_seed = v;
}

double get_unix_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000.0 / 1000.0;
}

static bool path_exists(const char *path)
{
    struct stat s;
    int err;
    err = stat(path, &s);
    if ((err == -1) && (errno == ENOENT)) return false;
    if (err == -1) LOG_E("Error when checking path '%s'", path);
    return true;
}

static void create_dirs(const char *path)
{
    char *slash;
    char *buff = malloc(strlen(path) + 1);
    int len = 0;
    while ((slash = strchr(path + len, '/'))) {
        len = slash - path + 1;
        memcpy(buff, path, len);
        buff[len] = '\0';
        if (!path_exists(buff)) {
            LOG_I("Create dir '%s'", buff);
#ifndef WIN32
            mkdir(buff, 0777);
#else
            mkdir(buff);
#endif
        }
    }
    free(buff);
}

FILE *open_data_file(const char *name, const char *mode, ...)
{
    char *full_name, *path;
    FILE *ret;
    va_list args;

    va_start(args, mode);
    vasprintf(&full_name, name, args);
    va_end(args);
    asprintf(&path, "%s/%s", sys_get_data_dir(), full_name);
    if (strchr(mode, 'w'))
        create_dirs(path);
    ret = fopen(path, mode);
    free(full_name);
    free(path);
    return ret;
}

char *read_file(const char *name, int *size, bool check_data_dir, ...)
{
    FILE *file;
    char *full_name = NULL, *path = NULL;
    va_list args;
    char *ret = NULL;
    int read_size __attribute__((unused));
    int size_default;

    va_start(args, check_data_dir);
    vasprintf(&full_name, name, args);
    va_end(args);

    size = size ?: &size_default; // Allow to pass NULL as size;
    if (!check_data_dir) goto read_asset;

    asprintf(&path, "%s/%s", sys_get_data_dir(), full_name);
    file = fopen(path, "rb");
    if (!file) goto read_asset;

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    ret = malloc(*size + 1);
    read_size = fread(ret, *size, 1, file);
    assert(read_size == 1 || *size == 0);
    ret[*size] = '\0';
    fclose(file);
    goto end;

read_asset:
    if (sys_asset_exists(full_name))
        ret = sys_read_asset(full_name, size);
end:
    free(path);
    free(full_name);
    return ret;
}

bool file_exists(const char *name, bool check_data_dir, ...)
{
    char *full_name = NULL, *path = NULL;
    va_list args;
    bool ret = false;
    va_start(args, check_data_dir);
    vasprintf(&full_name, name, args);
    va_end(args);
    if (check_data_dir) {
        asprintf(&path, "%s/%s", sys_get_data_dir(), full_name);
        ret = path_exists(path);
    }
    if (!ret)
        ret = sys_asset_exists(full_name);
    free(full_name);
    free(path);
    return ret;
}

void write_wav(int size, const char *buffer, int rate, int channels,
                   FILE *file)
{
#define WRITE_INT(type, x) \
    do {type _x = x; fwrite(&_x, sizeof(_x), 1, file); } while (0)
    // Header
    fwrite("RIFF", 4, 1, file);
    WRITE_INT(uint32_t, size + 40);         // remaining file size
    fwrite("WAVE", 4, 1, file);
    fwrite("fmt ", 4, 1, file);
    WRITE_INT(uint32_t, 16);                // chunk size
    WRITE_INT(uint16_t, 1);                 // compression code
    WRITE_INT(uint16_t, 1);                 // channels
    WRITE_INT(uint32_t, rate);              // sample rate
    WRITE_INT(uint32_t, rate * 2);          // bytes/sec
    WRITE_INT(uint16_t, 2);                 // block align
    WRITE_INT(uint16_t, 16 * channels);     // bits per sample
    fwrite("data", 4, 1, file);
    WRITE_INT(uint32_t, size);              // chunk size
    fwrite(buffer, 1, size, file);
#undef WRITE_INT
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
    char *data = read_file(path, &size, false);
    uint8_t *img;
    img = img_read_from_mem(data, size, width, height, bpp);
    free(data);
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
