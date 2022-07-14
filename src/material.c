#include "material.h"
#include "xxhash.h"

#include <stdio.h>
#include <stdlib.h>


material_t *material_new(const char *name)
{
    material_t *m = calloc(1, sizeof(*m));
    *m = MATERIAL_DEFAULT;
    if (name) snprintf(m->name, sizeof(m->name), "%s", name);
    return m;
}

void material_delete(material_t *m)
{
    free(m);
}

material_t *material_copy(const material_t *other)
{
    material_t *m = malloc(sizeof(*m));
    *m = *other;
    m->next = m->prev = NULL;
    return m;
}

uint32_t material_get_hash(const material_t *m)
{
    uint32_t ret = 0;
    ret = XXH32(&m->metallic, sizeof(m->metallic), ret);
    ret = XXH32(&m->roughness, sizeof(m->roughness), ret);
    ret = XXH32(&m->base_color, sizeof(m->base_color), ret);
    return ret;
}
