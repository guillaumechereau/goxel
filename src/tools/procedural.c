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

#include "goxel.h"

enum {
    STATE_IDLE = 0,
    STATE_SNAPED,
    STATE_PAINT,
};


typedef struct {
    tool_t  tool;
    gox_proc_t proc;

    bool auto_run;
    int timer;

    bool initialized;
    char **progs;
    char **names;
    int nb_progs;
    int current;

    char prog_path[1024];       // "\0" if no loaded prog.
    char prog_buff[64 * 1024];  // XXX: make it dynamic?

    bool export_animation;
    char export_animation_path[1024];

} tool_procedural_t;


static int iter(tool_t *tool, const float viewport[4])
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

static int list_saved_on_path(int i, const char *path, void *user_)
{
   const char *data, *name;
   void (*f)(int, const char*, const char*, void*) = USER_GET(user_, 0);
   void *user = USER_GET(user_, 1);
   if (!str_endswith(path, ".goxcf")) return -1;
   if (f) {
       data = assets_get(path, NULL);
       name = strrchr(path, '/') + 1;
       f(i, name, data, user);
   }
   return 0;
}

// List all the programs in data/progs.
static int proc_list_examples(void (*f)(int index,
                              const char *name, const char *code,
                              void *user), void *user)
{
    return assets_list("data/progs", USER_PASS(f, user), list_saved_on_path);
}

static void on_example(int i, const char *name, const char *code,
                       void *user)
{
    char **progs = USER_GET(user, 0);
    char **names = USER_GET(user, 1);
    progs[i] = strdup(code);
    names[i] = strdup(name);
}

static void load(tool_procedural_t *p)
{
    const char *path;
    FILE *f;
    int nb;

    path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
                                "goxcf\0*.goxcf\0", NULL, NULL);
    if (!path) return;
    f = fopen(path, "r");
    nb = (int)fread(p->prog_buff, 1, sizeof(p->prog_buff), f);
    p->prog_buff[nb] = '\0';
    fclose(f);
    strcpy(p->prog_path, path);
    proc_parse(p->prog_buff, &p->proc);
}

static void save(tool_procedural_t *p)
{
    const char *path;
    FILE *f;
    if (!*p->prog_path) {
        path = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                "goxcf\0*.goxcf\0", NULL, NULL);
        if (path) strcpy(p->prog_path, path);
    }
    if (!*p->prog_path) return;
    f = fopen(p->prog_path, "w");
    fwrite(p->prog_buff, strlen(p->prog_buff), 1, f);
    fclose(f);
}

static int gui(tool_t *tool)
{
    tool_procedural_t *p = (tool_procedural_t*)tool;
    bool enabled;
    gox_proc_t *proc;
    const char *dir_path;
    char *path;

    if (!p->initialized) {
        p->initialized = true;
        strcpy(p->prog_buff, "shape main {\n    cube[s 3]\n}");
        p->nb_progs = proc_list_examples(NULL, NULL);
        p->progs = (char**)calloc(p->nb_progs, sizeof(*p->progs));
        p->names = (char**)calloc(p->nb_progs, sizeof(*p->names));
        proc_list_examples(on_example,
                USER_PASS(p->progs, p->names));
        proc_parse(p->prog_buff, &p->proc);
        p->current = -1;
    }
    proc = &p->proc;

    if (gui_input_text_multiline("", p->prog_buff,
                                 ARRAY_SIZE(p->prog_buff), 300, 400)) {
        p->timer = 0;
        proc_parse(p->prog_buff, proc);
    }
    if (proc->error.str) {
        gui_input_text_multiline_highlight(proc->error.line);
        gui_text(proc->error.str);
    }
    enabled = proc->state >= PROC_READY;

    if (p->auto_run && proc->state == PROC_READY && p->timer == 0)
        p->timer = 1;
    if (proc->state == PROC_RUNNING) {
        if (gui_button("Stop", 0, 0)) proc_stop(proc);
    } else {
        gui_enabled_begin(enabled);
        if (    (gui_button("Run", 0, 0) && enabled) ||
                (p->auto_run && proc->state == PROC_READY &&
                 p->timer && p->timer++ >= 16)) {
            mesh_clear(goxel.image->active_layer->mesh);
            proc_start(proc, NULL);
            p->timer = 0;
        }
        gui_enabled_end();
    }
    gui_same_line();
    if (gui_checkbox("Auto", &p->auto_run, NULL))
        proc_parse(p->prog_buff, proc);
    gui_same_line();

    if (gui_button("Export Animation", 0, 0)) {
        dir_path = noc_file_dialog_open(
                    NOC_FILE_DIALOG_SAVE | NOC_FILE_DIALOG_DIR,
                    NULL, NULL, NULL);
        if (dir_path) {
            mesh_clear(goxel.image->active_layer->mesh);
            proc_start(proc, NULL);
            p->export_animation = true;
            sprintf(p->export_animation_path, "%s", dir_path);
        }
    }

    if (*p->prog_path) gui_text(p->prog_path);
    if (gui_button("Load", 0, 0)) load(p);
    gui_same_line();
    if (gui_button("Save", 0, 0)) save(p);

    if (gui_combo("Examples", &p->current,
                    (const char**)p->names, p->nb_progs)) {
        strcpy(p->prog_buff, p->progs[p->current]);
        proc_parse(p->prog_buff, proc);
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
        if (!proc->in_frame)
            goxel_update_meshes(MESH_RENDER);
    }
    return 0;
}

TOOL_REGISTER(TOOL_PROCEDURAL, procedural, tool_procedural_t,
              .gui_fn = gui,
              .iter_fn = iter,
              .flags = TOOL_REQUIRE_CAN_EDIT,
)
