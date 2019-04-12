/* Goxel 3D voxels editor
 *
 * copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
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

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#include "mustache.h"
#include "utlist.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#    include <tre/regex.h>
#else
#    include <regex.h>
#endif
#include <stdarg.h>

enum {
    M_TYPE_DICT,
    M_TYPE_LIST,
    M_TYPE_STR,
};

struct mustache
{
    char type;
    char *key;
    union {
        char *s;
    };
    mustache_t *next, *prev, *children, *parent;
};

static mustache_t *create_(mustache_t *m, const char *key, int type)
{
    mustache_t *ret = calloc(1, sizeof(*ret));
    ret->parent = m;
    ret->key = key ? strdup(key) : NULL;
    ret->type = type;
    if (m) DL_APPEND(m->children, ret);
    return ret;
}

mustache_t *mustache_root(void)
{
    mustache_t *ret = calloc(1, sizeof(*ret));
    ret->type = M_TYPE_DICT;
    return ret;
}

mustache_t *mustache_add_dict(mustache_t *m, const char *key)
{
    return create_(m, key, M_TYPE_DICT);
}

mustache_t *mustache_add_list(mustache_t *m, const char *key)
{
    return create_(m, key, M_TYPE_LIST);
}

void mustache_add_str(mustache_t *m, const char *key, const char *fmt, ...)
{
    va_list args;
    int r;
    mustache_t *ret;

    ret = create_(m, key, M_TYPE_STR);
    va_start(args, fmt);
    if (fmt) {
        r = vasprintf(&ret->s, fmt, args);
        (void)r; // Ignore errors.
    }
    va_end(args);
}

void mustache_free(mustache_t *m)
{
    mustache_t *c, *tmp;
    DL_FOREACH_SAFE(m->children, c, tmp)
        mustache_free(c);
    if (m->type == M_TYPE_STR) free(m->s);
    free(m->key);
    free(m);
}

const mustache_t *get_elem(const mustache_t *m, const char *key)
{
    mustache_t *c;
    if (key[0] == '#') key++;
    DL_FOREACH(m->children, c) {
        if (c->key && strcmp(key, c->key) == 0) return c;
    }
    return NULL;
}

static int render_(const mustache_t *tree, const mustache_t *m, char *out)
{
    int i = 0;
    mustache_t tmp;
    const mustache_t *c;
    const mustache_t *elem = NULL;

    if (tree->key) elem = get_elem(m, tree->key);

    // Some text.
    if (tree->type == M_TYPE_STR && !tree->key) {
        if (out)
            for (i = 0; i < strlen(tree->s); i++) *out++ = tree->s[i];
        return strlen(tree->s);
    }

    if (tree->key && tree->key[0] == '/') return 0;

    // A list
    if (tree->key && tree->key[0] == '#') {
        if (!elem) return 0;
        tmp = *tree;
        tmp.type = M_TYPE_LIST;
        tmp.key = NULL;
        if (elem->type == M_TYPE_LIST) {
            DL_FOREACH(elem->children, c) {
                assert(c->type == M_TYPE_DICT);
                i += render_(&tmp, c, out ? out + i : NULL);
            }
        }
        if (elem->type == M_TYPE_DICT) {
            i += render_(&tmp, elem, out);
        }
        return i;
    }

    // A variable
    if (tree->type == M_TYPE_STR && tree->key) {
        if (!elem) return 0;
        if (out)
            for (i = 0; i < strlen(elem->s); i++) *out++ = elem->s[i];
        return strlen(elem->s);
    }

    if (tree->children) {
        DL_FOREACH(tree->children, c) {
            i += render_(c, m, out ? out + i : NULL);
        }
        if (!tree->parent) {
            if (out) out[i] = '\0';
        }
        return i;
    }
    return 0;
}

int mustache_render(const mustache_t *m, const char *templ, char *out)
{
    regex_t reg;
    regmatch_t matches[2];
    mustache_t *tree;
    char key[128];
    int r, len;
    regcomp(&reg, "\\{\\{([#/]?[[:alnum:]_]+)\\}\\}", REG_EXTENDED);

    // Create the template tree.
    tree = mustache_add_list(NULL, NULL);
    while (templ) {
        r = regexec(&reg, templ, 2, matches, 0);
        if (r == REG_NOMATCH) { // No more tags.
            mustache_add_str(tree, NULL, templ);
            break;
        }
        if (matches[0].rm_so)
            mustache_add_str(tree, NULL, "%.*s", matches[0].rm_so, templ);

        len = matches[1].rm_eo - matches[1].rm_so;
        strncpy(key, templ + matches[1].rm_so, len);
        key[len] = '\0';
        mustache_add_str(tree, key, NULL);
        if (key[0] == '/') {
            r = asprintf(&tree->s, "%.*s", (int)(templ - tree->s), tree->s);
            (void)r; // Ignore errors.
            tree = tree->parent;
        }
        templ += matches[0].rm_eo;
        if (key[0] == '#') {
            tree = tree->children->prev;
            tree->s = (char*)templ;
        }
    }
    regfree(&reg);
    assert(!tree->parent);
    // Apply the parsed tree to the given dict.
    r = render_(tree, m, out);
    mustache_free(tree);
    return r;
}
