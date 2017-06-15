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
    STATE_IDLE      = 0,
    STATE_CANCEL    = 1,
    STATE_END       = 2,

    STATE_SNAPED,
    STATE_PAINT,

    STATE_ENTER     = 0x0100,
};

static int iter(int state, void **data,
                const vec4_t *view, bool inside)
{
    box_t box;
    gox_proc_t *proc = &goxel->proc;
    cursor_t *curs = &goxel->cursor;

    if (proc->state == PROC_PARSE_ERROR) return 0;

    switch (state) {
    case STATE_IDLE:
        if (curs->snaped) return STATE_SNAPED;
        break;

    case STATE_SNAPED:
        if (!curs->snaped) return STATE_IDLE;
        box = bbox_from_extents(curs->pos, 0.5, 0.5, 0.5);
        render_box(&goxel->rend, &box, NULL, EFFECT_WIREFRAME);
        if (curs->flags & CURSOR_PRESSED) {
            image_history_push(goxel->image);
            proc_stop(proc);
            proc_start(proc, &box);
            return STATE_PAINT;
        }
        break;

    case STATE_PAINT:
        if (!(curs->flags & CURSOR_PRESSED)) return STATE_IDLE;
        break;
    }

    return state;
}

static void on_example(int i, const char *name, const char *code,
                       void *user)
{
    char **progs = USER_GET(user, 0);
    char **names = USER_GET(user, 1);
    progs[i] = strdup(code);
    names[i] = strdup(name);
}

static int gui(void)
{
    static char **progs = NULL;
    static char **names = NULL;
    static int nb_progs = 0;
    static bool first_time = true;
    int i;
    static int current = -1;
    gox_proc_t *proc = &goxel->proc;
    bool enabled;
    static bool auto_run;
    static int timer = 0;
    static char prog_path[1024];       // "\0" if no loaded prog.
    static char prog_buff[64 * 1024];  // XXX: make it dynamic?
    static bool prog_export_animation;
    static char prog_export_animation_path[1024];

    if (first_time) {
        first_time = false;
        strcpy(prog_buff, "shape main {\n    cube[s 3]\n}");
        for (i = 0; i < nb_progs; i++) {free(progs[i]); free(names[i]);}
        free(progs);
        free(names);
        nb_progs = proc_list_examples(NULL, NULL);
        progs = (char**)calloc(nb_progs, sizeof(*progs));
        names = (char**)calloc(nb_progs, sizeof(*names));
        proc_list_examples(on_example, USER_PASS(progs, names));
        proc_parse(prog_buff, proc);
    }

    if (gui_input_text_multiline("", prog_buff,
                                 ARRAY_SIZE(prog_buff), 400)) {
        timer = 0;
        proc_parse(prog_buff, proc);
    }
    if (proc->error.str) {
        gui_input_text_multiline_highlight(goxel->proc.error.line);
        gui_text(proc->error.str);
    }
    enabled = proc->state >= PROC_READY;

    if (auto_run && proc->state == PROC_READY && timer == 0) timer = 1;
    if (proc->state == PROC_RUNNING) {
        if (gui_button("Stop", 0)) proc_stop(proc);
    } else {
        gui_enabled_begin(enabled);
        if (    (gui_button("Run", 0) && enabled) ||
                (auto_run && proc->state == PROC_READY &&
                 timer && timer++ >= 16)) {
            mesh_clear(goxel->image->active_layer->mesh);
            proc_start(proc, NULL);
            timer = 0;
        }
        gui_enabled_end();
    }
    gui_same_line();
    if (gui_checkbox("Auto", &auto_run, NULL))
        proc_parse(prog_buff, proc);
    gui_same_line();

    if (gui_button("Export Animation", 0)) {
        const char *dir_path;
        dir_path = noc_file_dialog_open(
                    NOC_FILE_DIALOG_SAVE | NOC_FILE_DIALOG_DIR,
                    NULL, NULL, NULL);
        if (dir_path) {
            mesh_clear(goxel->image->active_layer->mesh);
            proc_start(proc, NULL);
            prog_export_animation = true;
            sprintf(prog_export_animation_path, "%s", dir_path);
        }
    }

    // File load / save.  No error check yet!
    if (*prog_path) {
        gui_input_text("##path", prog_path, sizeof(prog_path));
    }
    if (gui_button("Load", 0)) {
        const char *path;
        path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
                                    "goxcf\0*.goxcf\0", NULL, NULL);
        if (path) {
            FILE *f = fopen(path, "r");
            int nb;
            nb = (int)fread(prog_buff, 1, sizeof(prog_buff), f);
            prog_buff[nb] = '\0';
            fclose(f);
            strcpy(prog_path, path);
        }
        proc_parse(prog_buff, proc);
    }
    gui_same_line();
    if (gui_button("Save", 0)) {
        if (!*prog_path) {
            const char *path;
            path = noc_file_dialog_open(NOC_FILE_DIALOG_SAVE,
                                   "goxcf\0*.goxcf\0", NULL, NULL);
            if (path)
                strcpy(prog_path, path);
        }
        if (*prog_path) {
            FILE *f = fopen(prog_path, "w");
            fwrite(prog_buff, strlen(prog_buff), 1, f);
            fclose(f);
        }
    }

    if (gui_combo("Examples", &current, (const char**)names, nb_progs)) {
        strcpy(prog_buff, progs[current]);
        proc_parse(prog_buff, proc);
    }

    if (proc->state == PROC_RUNNING && prog_export_animation
            && !proc->in_frame) {
        char path[1024];
        sprintf(path, "%s/img_%04d.png",
                prog_export_animation_path, proc->frame);
        action_exec2("export_as", "pp", "png", path);
    }
    if (proc->state != PROC_RUNNING) prog_export_animation = false;

    if (proc->state == PROC_RUNNING) {
        proc_iter(proc);
        if (!proc->in_frame)
            goxel_update_meshes(goxel, MESH_LAYERS);
    }
    return 0;
}

TOOL_REGISTER(TOOL_PROCEDURAL, procedural,
              .gui_fn = gui,
              .iter_fn = iter,
)
