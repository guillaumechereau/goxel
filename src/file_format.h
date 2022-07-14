#ifndef FILE_FORMAT_H
#define FILE_FORMAT_H

#include "image.h"

typedef struct file_format file_format_t;
struct file_format
{
    file_format_t   *next, *prev; // For the global list of formats.
    const char      *name;
    const char      *ext;
    void            (*export_gui)(void);
    int             (*export_func)(const image_t *img, const char *path);
    int             (*import_func)(image_t *img, const char *path);
};

void file_format_register(file_format_t *format);

const file_format_t *file_format_for_path(const char *path, const char *name,
                                          const char *mode);

void file_format_iter(const char *mode, void *user,
                      void (*f)(void *user, const file_format_t *f));

// The global list of registered file formats.
extern file_format_t *file_formats;

#define FILE_FORMAT_REGISTER(id_, ...) \
    static file_format_t GOX_format_##id_ = {__VA_ARGS__}; \
    __attribute__((constructor)) \
    static void GOX_register_format_##id_(void) { \
        file_format_register(&GOX_format_##id_); \
    }

#endif // FILE_FORMAT_H
