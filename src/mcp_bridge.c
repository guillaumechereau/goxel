/* Goxel 3D voxels editor
 *
 * copyright (c) 2026 yberudev <yberudev@gmail.com>
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

/*
 * Local command bridge.
 *
 * A small server listening on a Unix domain socket (mode 0600, so only the
 * current user can connect).  The protocol is one JSON object per line:
 *
 *   request : {"id": 1, "cmd": "info", "args": {...}}
 *   response: {"id": 1, "ok": true, "result": {...}}
 *             {"id": 1, "ok": false, "error": "message"}
 *
 * Everything runs on the main thread from mcp_bridge_iter(), so no locking
 * is needed and the editor shows the changes immediately.  Every modifying
 * command pushes an undo snapshot.  See doc/mcp-bridge.md.
 */

#include "mcp_bridge.h"

#if defined(_WIN32) || defined(__EMSCRIPTEN__)

// Not supported on this platform.
void mcp_bridge_init(const char *path) {}
void mcp_bridge_iter(void) {}
void mcp_bridge_release(void) {}

#else

#include "goxel.h"
#include "file_format.h"
#include "json/json.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define BRIDGE_VERSION 1
#define MAX_REQUEST (256 * 1024 * 1024)
#define MAX_CREATED 1024

// Growable string buffer.
typedef struct {
    char   *buf;
    size_t len;
    size_t cap;
} sbuf_t;

static struct {
    int     server;
    int     client;
    char    path[512];
    sbuf_t  in;
} g_bridge = {.server = -1, .client = -1};

static void sb_reserve(sbuf_t *s, size_t n)
{
    if (s->len + n + 1 <= s->cap) return;
    while (s->len + n + 1 > s->cap) s->cap = s->cap ? s->cap * 2 : 4096;
    s->buf = realloc(s->buf, s->cap);
}

static void sb_add(sbuf_t *s, const char *data, size_t n)
{
    sb_reserve(s, n);
    memcpy(s->buf + s->len, data, n);
    s->len += n;
    s->buf[s->len] = '\0';
}

static void sb_printf(sbuf_t *s, const char *fmt, ...)
{
    va_list ap;
    int n;
    char tmp[256];

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < (int)sizeof(tmp)) {
        sb_add(s, tmp, n);
        return;
    }
    sb_reserve(s, n);
    va_start(ap, fmt);
    vsnprintf(s->buf + s->len, n + 1, fmt, ap);
    va_end(ap);
    s->len += n;
}

// Add a JSON string literal.
static void sb_str(sbuf_t *s, const char *str)
{
    const unsigned char *p;

    sb_add(s, "\"", 1);
    for (p = (const unsigned char*)(str ? str : ""); *p; p++) {
        if (*p == '"' || *p == '\\')
            sb_printf(s, "\\%c", *p);
        else if (*p < 0x20)
            sb_printf(s, "\\u%04x", *p);
        else
            sb_add(s, (const char*)p, 1);
    }
    sb_add(s, "\"", 1);
}

static const json_value *jget(const json_value *obj, const char *name)
{
    unsigned int i;

    if (!obj || obj->type != json_object) return NULL;
    for (i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, name) == 0)
            return obj->u.object.values[i].value;
    }
    return NULL;
}

static bool jint(const json_value *v, int64_t *out)
{
    if (!v) return false;
    if (v->type == json_integer) {
        *out = v->u.integer;
        return true;
    }
    if (v->type == json_double) {
        *out = (int64_t)v->u.dbl;
        return true;
    }
    return false;
}

static const char *jstr(const json_value *v)
{
    return (v && v->type == json_string) ? v->u.string.ptr : NULL;
}

static layer_t *find_layer(int64_t id)
{
    layer_t *layer;

    DL_FOREACH(goxel.image->layers, layer) {
        if (layer->id == id) return layer;
    }
    return NULL;
}

static int count_voxels(const volume_t *volume)
{
    volume_iterator_t iter;
    int pos[3], n = 0;
    uint8_t v[4];

    iter = volume_get_iterator(volume,
            VOLUME_ITER_VOXELS | VOLUME_ITER_SKIP_EMPTY);
    while (volume_iter(&iter, pos)) {
        volume_get_at(volume, &iter, pos, v);
        if (v[3]) n++;
    }
    return n;
}

static void write_layer_info(sbuf_t *out, const layer_t *layer, bool voxels)
{
    volume_iterator_t iter;
    int pos[3];
    uint8_t v[4];
    bool first = true;

    sb_printf(out, "{\"id\":%d,\"name\":", layer->id);
    sb_str(out, layer->name);
    sb_printf(out, ",\"visible\":%s,\"active\":%s,\"editable\":%s",
              layer->visible ? "true" : "false",
              layer == goxel.image->active_layer ? "true" : "false",
              image_layer_can_edit(goxel.image, layer) ? "true" : "false");
    if (!voxels) {
        sb_printf(out, ",\"voxels\":%d}", count_voxels(layer->volume));
        return;
    }
    // Voxels as a flat array: x, y, z, packed RGBA (0xRRGGBBAA).
    sb_add(out, ",\"data\":[", 9);
    iter = volume_get_iterator(layer->volume,
            VOLUME_ITER_VOXELS | VOLUME_ITER_SKIP_EMPTY);
    while (volume_iter(&iter, pos)) {
        volume_get_at(layer->volume, &iter, pos, v);
        if (!v[3]) continue;
        sb_printf(out, "%s%d,%d,%d,%u", first ? "" : ",",
                  pos[0], pos[1], pos[2],
                  ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) |
                  ((uint32_t)v[2] << 8) | (uint32_t)v[3]);
        first = false;
    }
    sb_add(out, "]}", 2);
}

static void write_layers(sbuf_t *out, bool voxels, int64_t only_id)
{
    layer_t *layer;
    bool first = true;

    sb_add(out, "[", 1);
    DL_FOREACH(goxel.image->layers, layer) {
        if (only_id && layer->id != only_id) continue;
        if (!first) sb_add(out, ",", 1);
        write_layer_info(out, layer, voxels);
        first = false;
    }
    sb_add(out, "]", 1);
}

/*
 * Apply one operation of an 'apply' request.  Returns an error message, or
 * NULL on success.  Operations can target a layer by "id", or by
 * "new_index": the index of a layer created by a previous "add_layer"
 * operation of the same request.
 */
static const char *apply_op(const json_value *op, sbuf_t *res, bool *changed,
                            int *created, int *nb_created)
{
    const char *kind = jstr(jget(op, "op"));
    const char *name;
    const json_value *data, *vis;
    int64_t id = 0, x, y, z, c, new_index;
    layer_t *layer = NULL;
    unsigned int i;
    int pos[3];
    uint8_t v[4];
    volume_iterator_t iter = {0};

    if (!kind) return "op missing";
    if (strcmp(kind, "add_layer") == 0) {
        layer = image_add_layer(goxel.image, NULL);
        name = jstr(jget(op, "name"));
        if (name) snprintf(layer->name, sizeof(layer->name), "%s", name);
        if (*nb_created < MAX_CREATED) created[(*nb_created)++] = layer->id;
        sb_printf(res, "{\"id\":%d}", layer->id);
        *changed = true;
        return NULL;
    }

    if (jint(jget(op, "new_index"), &new_index)) {
        if (new_index < 0 || new_index >= *nb_created)
            return "invalid new_index";
        id = created[new_index];
    } else if (!jint(jget(op, "id"), &id)) {
        return "layer id missing";
    }
    if (!(layer = find_layer(id))) return "layer not found";

    if (strcmp(kind, "delete_layer") == 0) {
        image_delete_layer(goxel.image, layer);
    } else if (strcmp(kind, "rename_layer") == 0) {
        name = jstr(jget(op, "name"));
        if (!name) return "name missing";
        snprintf(layer->name, sizeof(layer->name), "%s", name);
    } else if (strcmp(kind, "set_visible") == 0) {
        vis = jget(op, "visible");
        if (!vis || vis->type != json_boolean) return "visible missing";
        layer->visible = vis->u.boolean;
    } else if (strcmp(kind, "set_active") == 0) {
        goxel.image->active_layer = layer;
    } else if (strcmp(kind, "clear_layer") == 0) {
        if (!image_layer_can_edit(goxel.image, layer))
            return "layer is not editable";
        volume_clear(layer->volume);
    } else if (strcmp(kind, "set_voxels") == 0) {
        if (!image_layer_can_edit(goxel.image, layer))
            return "layer is not editable";
        data = jget(op, "data");
        if (!data || data->type != json_array || data->u.array.length % 4)
            return "data must be a flat array [x,y,z,rgba,...]";
        for (i = 0; i < data->u.array.length; i += 4) {
            if (!jint(data->u.array.values[i + 0], &x) ||
                !jint(data->u.array.values[i + 1], &y) ||
                !jint(data->u.array.values[i + 2], &z) ||
                !jint(data->u.array.values[i + 3], &c))
                return "non integer value in data";
            pos[0] = (int)x;
            pos[1] = (int)y;
            pos[2] = (int)z;
            v[0] = (c >> 24) & 0xff;
            v[1] = (c >> 16) & 0xff;
            v[2] = (c >> 8) & 0xff;
            v[3] = c & 0xff;
            volume_set_at(layer->volume, &iter, pos, v);
        }
        volume_remove_empty_tiles(layer->volume, false);
        sb_printf(res, "{\"set\":%u}", data->u.array.length / 4);
        *changed = true;
        return NULL;
    } else {
        return "unknown op";
    }
    sb_add(res, "{}", 2);
    *changed = true;
    return NULL;
}

static const char *open_file(const char *path)
{
    image_delete(goxel.image);
    goxel.image = image_new();
    if (str_endswith(path, ".gox")) {
        if (load_from_file(path, true) != 0) return "cannot load gox file";
    } else {
        if (goxel_import_file(path, NULL) != 0) return "cannot import file";
        image_history_push(goxel.image);
    }
    // A freshly opened file has no unsaved changes.
    goxel.image->saved_key = image_get_key(goxel.image);
    goxel_add_recent_file(path);
    return NULL;
}

static void write_response(sbuf_t *out, const json_value *idv,
                           const char *err, const sbuf_t *res)
{
    sb_add(out, "{\"id\":", 6);
    if (idv && idv->type == json_integer)
        sb_printf(out, "%lld", (long long)idv->u.integer);
    else
        sb_add(out, "null", 4);
    if (err) {
        sb_add(out, ",\"ok\":false,\"error\":", 20);
        sb_str(out, err);
    } else {
        sb_add(out, ",\"ok\":true,\"result\":", 20);
        if (res->buf)
            sb_add(out, res->buf, res->len);
        else
            sb_add(out, "{}", 2);
    }
    sb_add(out, "}\n", 2);
}

static void handle_request(const char *line, size_t len, sbuf_t *out)
{
    char errbuf[json_error_max] = "";
    json_settings settings = {0};
    json_value *req, *ops;
    const json_value *args, *idv;
    const char *cmd, *err = NULL, *path, *format;
    sbuf_t res = {0}, opres = {0};
    bool changed = false;
    int64_t only_id = 0;
    unsigned int i;
    int created[MAX_CREATED], nb_created = 0;

    req = json_parse_ex(&settings, line, len, errbuf);
    if (!req || req->type != json_object) {
        write_response(out, NULL, *errbuf ? errbuf : "invalid json request",
                       &res);
        if (req) json_value_free(req);
        return;
    }
    idv = jget(req, "id");
    cmd = jstr(jget(req, "cmd"));
    args = jget(req, "args");

    if (!cmd) {
        err = "cmd missing";
    } else if (strcmp(cmd, "ping") == 0 || strcmp(cmd, "info") == 0) {
        sb_printf(&res, "{\"goxel_version\":\"%s\",\"bridge_version\":%d,"
                  "\"path\":", GOXEL_VERSION_STR, BRIDGE_VERSION);
        sb_str(&res, goxel.image->path);
        sb_printf(&res, ",\"modified\":%s,\"layers\":",
                  image_get_key(goxel.image) != goxel.image->saved_key ?
                  "true" : "false");
        write_layers(&res, false, 0);
        sb_add(&res, "}", 1);
    } else if (strcmp(cmd, "get_scene") == 0) {
        jint(jget(args, "layer_id"), &only_id);
        sb_add(&res, "{\"layers\":", 10);
        write_layers(&res, true, only_id);
        sb_add(&res, "}", 1);
    } else if (strcmp(cmd, "apply") == 0) {
        ops = (json_value*)jget(args, "ops");
        if (!ops || ops->type != json_array) {
            err = "ops must be an array";
        } else {
            sb_add(&res, "{\"results\":[", 12);
            for (i = 0; i < ops->u.array.length; i++) {
                opres.len = 0;
                err = apply_op(ops->u.array.values[i], &opres, &changed,
                               created, &nb_created);
                if (err) break;
                if (i) sb_add(&res, ",", 1);
                sb_add(&res, opres.buf, opres.len);
            }
            sb_add(&res, "]}", 2);
        }
        // Keep partial changes undoable, even on error.
        if (changed) image_history_push(goxel.image);
    } else if (strcmp(cmd, "new_scene") == 0) {
        image_delete(goxel.image);
        goxel.image = image_new();
        sb_add(&res, "{}", 2);
    } else if (strcmp(cmd, "open") == 0) {
        path = jstr(jget(args, "path"));
        err = path ? open_file(path) : "path missing";
        if (!err) sb_add(&res, "{}", 2);
    } else if (strcmp(cmd, "save") == 0) {
        path = jstr(jget(args, "path"));
        if (!path) {
            err = "path missing";
        } else {
            save_to_file(goxel.image, path);
            if (path != goxel.image->path) {
                free(goxel.image->path);
                goxel.image->path = strdup(path);
            }
            goxel.image->saved_key = image_get_key(goxel.image);
            goxel_add_recent_file(path);
            sb_add(&res, "{}", 2);
        }
    } else if (strcmp(cmd, "export") == 0) {
        path = jstr(jget(args, "path"));
        format = jstr(jget(args, "format"));
        if (!path)
            err = "path missing";
        else if (goxel_export_to_file(path, format) != 0)
            err = "export failed";
        else
            sb_add(&res, "{}", 2);
    } else if (strcmp(cmd, "undo") == 0) {
        image_undo(goxel.image);
        sb_add(&res, "{}", 2);
    } else if (strcmp(cmd, "redo") == 0) {
        image_redo(goxel.image);
        sb_add(&res, "{}", 2);
    } else {
        err = "unknown command";
    }

    write_response(out, idv, err, &res);
    free(res.buf);
    free(opres.buf);
    json_value_free(req);
}

static void close_client(void)
{
    if (g_bridge.client >= 0) close(g_bridge.client);
    g_bridge.client = -1;
    g_bridge.in.len = 0;
}

static bool send_all(int fd, const char *data, size_t len)
{
    struct pollfd pfd = {.fd = fd, .events = POLLOUT};
    ssize_t n;

    while (len) {
        n = send(fd, data, len, 0);
        if (n > 0) {
            data += n;
            len -= n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (poll(&pfd, 1, 10000) <= 0) return false;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

void mcp_bridge_init(const char *path)
{
    struct sockaddr_un addr = {0};
    struct stat st;
    int fd;

    if (!path) path = getenv("GOXEL_MCP_SOCKET");
    if (!path || !*path) return;
    if (strlen(path) >= sizeof(addr.sun_path)) {
        LOG_E("MCP bridge: socket path too long: %s", path);
        return;
    }
    // Only remove a stale socket, never a regular file.
    if (lstat(path, &st) == 0) {
        if (!S_ISSOCK(st.st_mode)) {
            LOG_E("MCP bridge: %s exists and is not a socket", path);
            return;
        }
        unlink(path);
    }
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ||
        chmod(path, 0600) != 0 || listen(fd, 2) != 0)
    {
        LOG_E("MCP bridge: cannot listen on %s: %s", path, strerror(errno));
        close(fd);
        return;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    g_bridge.server = fd;
    snprintf(g_bridge.path, sizeof(g_bridge.path), "%s", path);
    LOG_I("MCP bridge listening on %s", path);
}

void mcp_bridge_release(void)
{
    close_client();
    if (g_bridge.server >= 0) {
        close(g_bridge.server);
        unlink(g_bridge.path);
    }
    g_bridge.server = -1;
    free(g_bridge.in.buf);
    g_bridge.in = (sbuf_t){0};
}

void mcp_bridge_iter(void)
{
    char buf[65536];
    ssize_t n;
    char *nl;
    size_t line_len, consumed;
    sbuf_t out = {0};
    int fd;
    int one = 1;

    if (g_bridge.server < 0 || !goxel.image || !goxel.graphics_initialized)
        return;

    // Only one client at a time.
    if (g_bridge.client < 0) {
        fd = accept(g_bridge.server, NULL, NULL);
        if (fd < 0) return;
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
        (void)one;
        g_bridge.client = fd;
        LOG_I("MCP bridge: client connected");
    }

    while (true) {
        n = recv(g_bridge.client, buf, sizeof(buf), 0);
        if (n > 0) {
            sb_add(&g_bridge.in, buf, n);
            if (g_bridge.in.len > MAX_REQUEST) {
                LOG_E("MCP bridge: request too large");
                close_client();
                return;
            }
            continue;
        }
        if (n == 0) {
            close_client();
            return;
        }
        if (errno == EINTR) continue;
        break; // EAGAIN: nothing more to read for now.
    }

    consumed = 0;
    while (g_bridge.in.buf && consumed < g_bridge.in.len &&
           (nl = memchr(g_bridge.in.buf + consumed, '\n',
                        g_bridge.in.len - consumed)))
    {
        line_len = nl - (g_bridge.in.buf + consumed);
        out.len = 0;
        if (line_len) {
            handle_request(g_bridge.in.buf + consumed, line_len, &out);
            if (!send_all(g_bridge.client, out.buf, out.len)) {
                free(out.buf);
                close_client();
                return;
            }
        }
        consumed += line_len + 1;
    }
    if (consumed) {
        memmove(g_bridge.in.buf, g_bridge.in.buf + consumed,
                g_bridge.in.len - consumed);
        g_bridge.in.len -= consumed;
    }
    free(out.buf);
}

#endif // !_WIN32 && !__EMSCRIPTEN__
