/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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

#include "procedural.h"
#include "goxel.h"

enum {
    STATE_IDLE = 0,
    STATE_SNAPED,
    STATE_PAINT,
};

typedef struct prog prog_t;
struct prog {
    prog_t *next, *prev;
    char *name;
    char *path;
    char *code;
    bool is_asset;
};

typedef struct {
    tool_t  tool;
    gox_proc_t proc;

    int timer;

    bool initialized;

    prog_t *progs;
    prog_t *current;

    bool export_animation;
    char export_animation_path[1024];

} tool_procedural_t;


static int iter(tool_t *tool, const painter_t *painter, const float viewport[4])
{
    float box[4][4];
    gox_proc_t *proc = &((tool_procedural_t*)tool)->proc;
    cursor_t *curs = &goxel.cursor;

    if (proc->state == PROC_PARSE_ERROR) return 0;

    switch (tool->state) {
    case STATE_IDLE:
        if (curs->snaped) return STATE_SNAPED;
        break;

    case STATE_SNAPED:
        if (!curs->snaped) return STATE_IDLE;
        bbox_from_extents(box, curs->pos, 0.5, 0.5, 0.5);
        render_box(&goxel.rend, box, NULL, EFFECT_WIREFRAME);
        if (curs->flags & CURSOR_PRESSED) {
            image_history_push(goxel.image);
            proc_stop(proc);
            proc_start(proc, box);
            return STATE_PAINT;
        }
        break;

    case STATE_PAINT:
        if (!(curs->flags & CURSOR_PRESSED)) return STATE_IDLE;
        break;
    }

    return tool->state;
}

static int on_asset_path(int i, const char *path, void *user_)
{
    const char *data, *name;
    void (*f)(void*, const char*, const char*, const char*, bool);
    void *user;
    f = USER_GET(user_, 0);
    user = USER_GET(user_, 1);
    if (!str_endswith(path, ".goxcf")) return -1;
    if (f) {
        data = assets_get(path, NULL);
        name = strrchr(path, '/') + 1;
        f(user, path, name, data, true);
    }
    return 0;
}

static int on_user_path(const char *dir, const char *name, void *user_)
{
    void (*f)(void*, const char*, const char*, const char*, bool);
    void *user;
    char path[1024];
    char *data;

    f = USER_GET(user_, 0);
    user = USER_GET(user_, 1);

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (!str_endswith(path, ".goxcf")) return -1;
    if (f) {
        data = read_file(path, NULL);
        f(user, path, name, data, false);
        free(data);
    }
    return 0;
}

// List all the programs in data/progs.
static void proc_list_examples(void *user,
        void (*f)(void *user, const char *path, const char *name,
                  const char *code, bool is_asset))
{
    char dir[1024];
    if (sys_get_user_dir()) {
        snprintf(dir, sizeof(dir), "%s/progs", sys_get_user_dir());
        sys_list_dir(dir, on_user_path, USER_PASS(f, user));
    }
    assets_list("data/progs", USER_PASS(f, user), on_asset_path);
}

static void on_example(void *user, const char *path, const char *name,
                       const char *code, bool is_asset)
{
    tool_procedural_t *proc = (void*)user;
    prog_t *prog = calloc(1, sizeof(*prog));
    if (path) prog->path = strdup(path);
    prog->name = strdup(name);
    prog->code = strdup(code);
    prog->is_asset = is_asset;
    DL_APPEND(proc->progs, prog);
}

static void prog_revert(prog_t *prog)
{
    free(prog->code);
    if (prog->is_asset)
        prog->code = strdup(assets_get(prog->path, NULL));
    else
        prog->code = read_file(prog->path, NULL);
}

static void prog_save(prog_t *prog)
{
    FILE *file;
    assert(!prog->is_asset);
    file = fopen(prog->path, "w");
    if (!file) {
        gui_alert("Error", "Cannot save");
        return;
    }
    fwrite(prog->code, 1, strlen(prog->code), file);
    fclose(file);
}

static int gui(tool_t *tool)
{
    tool_procedural_t *p = (tool_procedural_t*)tool;
    bool enabled;
    gox_proc_t *proc;
    prog_t *prog;
    char *path;
    int buf_size;
    char *buf;

    gui_request_panel_width(GUI_PANEL_WIDTH_LARGE);
    if (!p->initialized) {
        p->initialized = true;
        proc_list_examples(p, on_example);
        assert(p->progs);
        p->current = p->progs;
        proc_parse(p->current->code, &p->proc);
    }
    proc = &p->proc;

    buf_size = 64 * 1024 + strlen(p->current->code);
    buf = malloc(buf_size);
    snprintf(buf, buf_size, "%s", p->current->code);
    if (gui_input_text_multiline("", buf, buf_size, -1, 400)) {
        p->timer = 0;
        free(p->current->code);
        p->current->code = strdup(buf);
        proc_parse(p->current->code, proc);
    }
    free(buf);

    if (proc->error.str) {
        gui_input_text_multiline_highlight(proc->error.line);
        gui_text(proc->error.str);
    }
    enabled = proc->state >= PROC_READY;

    if (proc->state == PROC_RUNNING) {
        if (gui_button("Stop", 0, 0)) proc_stop(proc);
    } else {
        gui_enabled_begin(enabled);
        if (gui_button("Run", 0, 0) && enabled) {
            mesh_clear(goxel.image->active_layer->mesh);
            proc_start(proc, NULL);
            p->timer = 0;
        }
        gui_enabled_end();
    }

    if (gui_combo_begin("Examples", p->current->name)) {
        DL_FOREACH(p->progs, prog) {
            if (gui_combo_item(prog->name, prog == p->current)) {
                p->current = prog;
                proc_parse(prog->code, proc);
            }
        }
        gui_combo_end();
    }

    if (gui_button("Revert", 0, 0)) {
        prog_revert(p->current);
    }
    if (!p->current->is_asset) {
        gui_same_line();
        if (gui_button("Save", 0, 0)) {
            prog_save(p->current);
        }
    }

    if (proc->state == PROC_RUNNING && p->export_animation
            && !proc->in_frame) {
        asprintf(&path, "%s/img_%04d.png",
                p->export_animation_path, proc->frame);
        action_exec2("export_as", "pp", "png", path);
        free(path);
    }
    if (proc->state != PROC_RUNNING) p->export_animation = false;

    if (proc->state == PROC_RUNNING) {
        proc_iter(proc, goxel.image->active_layer->mesh, &goxel.painter);
    }
    return 0;
}

TOOL_REGISTER(TOOL_PROCEDURAL, procedural, tool_procedural_t,
              .name = "Procedural",
              .gui_fn = gui,
              .iter_fn = iter,
              .flags = TOOL_REQUIRE_CAN_EDIT,
)
