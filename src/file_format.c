#include "file_format.h"
#include "utlist.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// The global hash table of file formats.
file_format_t *file_formats = NULL;

static bool endswith(const char *str, const char *end)
{
    const char *start;
    if (strlen(str) < strlen(end)) return false;
    start = str + strlen(str) - strlen(end);
    return strcmp(start, end) == 0;
}


void file_format_register(file_format_t *format)
{
    DL_APPEND(file_formats, format);
}

const file_format_t *file_format_for_path(const char *path, const char *name,
                                          const char *mode)
{
    const file_format_t *f;
    bool need_read = strchr(mode, 'r');
    bool need_write = strchr(mode, 'w');
    const char *ext;

    assert(mode);
    assert(path || name);

    DL_FOREACH(file_formats, f) {
        if (need_read && !f->import_func) continue;
        if (need_write && !f->export_func) continue;
        if (name && strcasecmp(f->name, name) != 0) continue;
        if (path) {
            ext = f->ext + strlen(f->ext) + 2; // Pick the string after '*'.
            if (!endswith(path, ext)) continue;
        }
        return f;
    }
    return NULL;
}

void file_format_iter(const char *mode, void *user,
                      void (*fun)(void *user, const file_format_t *f))
{
    assert(mode);
    assert(fun);
    const file_format_t *f;
    bool need_read = strchr(mode, 'r');
    bool need_write = strchr(mode, 'w');
    DL_FOREACH(file_formats, f) {
        if (need_read && !f->import_func) continue;
        if (need_write && !f->export_func) continue;
        fun(user, f);
    }
}
