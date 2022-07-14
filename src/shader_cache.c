#include "goxel.h"

#include "shader_cache.h"

typedef struct {
    char key[256];
    gl_shader_t *shader;
} shader_t;

static shader_t g_shaders[16] = {};

gl_shader_t *shader_get(const char *name, const shader_define_t *defines,
                        const char **attr_names,
                        void (*on_created)(gl_shader_t *s))
{
    int i;
    shader_t *s = NULL;
    const char *code;
    char key[256];
    char path[128];
    char pre[256] = {};
    const shader_define_t *define;


    // Create the key of the form:
    // <name>_define1_define2
    strcpy(key, name);
    for (define = defines; define && define->name; define++) {
        if (define->set) {
            strcat(key, "_");
            strcat(key, define->name);
        }
    }

    for (i = 0; i < ARRAY_SIZE(g_shaders); i++) {
        s = &g_shaders[i];
        if (!*s->key) break;
        if (strcmp(s->key, key) == 0)
            return s->shader;
    }
    assert(i < ARRAY_SIZE(g_shaders));
    strcpy(s->key, key);

    sprintf(path, "asset://data/shaders/%s.glsl", name);
    code = assets_get(path, NULL);
    assert(code);

    for (define = defines; define && define->name; define++) {
        if (define->set)
            sprintf(pre + strlen(pre), "#define %s\n", define->name);
    }
    s->shader = gl_shader_create(code, code, pre, attr_names);
    if (on_created) on_created(s->shader);
    return s->shader;
}

/*
 * Function: shaders_release_all
 * Remove all the shaders from the cache.
 */
void shaders_release_all(void)
{
    int i;
    shader_t *s = NULL;
    for (i = 0; i < ARRAY_SIZE(g_shaders); i++) {
        s = &g_shaders[i];
        if (!*s->key) break;
        gl_shader_delete(s->shader);
        memset(s, 0, sizeof(*s));
    }
}
