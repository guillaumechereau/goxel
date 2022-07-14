// Basic mustache templates support
// Check povray.c for an example of usage.

#ifndef MUSTACHE_H
#define MUSTACHE_H

typedef struct mustache mustache_t;
mustache_t *mustache_root(void);
mustache_t *mustache_add_dict(mustache_t *m, const char *key);
mustache_t *mustache_add_list(mustache_t *m, const char *key);
void mustache_add_str(mustache_t *m, const char *key, const char *fmt, ...);
int mustache_render(const mustache_t *m, const char *templ, char *out);
void mustache_free(mustache_t *m);

#endif // MUSTACHE_H
