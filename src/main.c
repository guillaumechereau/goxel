/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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
#include <GLFW/glfw3.h>

static inputs_t *g_inputs = NULL;

void on_scroll(GLFWwindow *win, double x, double y)
{
    g_inputs->mouse_wheel = y;
}

void on_char(GLFWwindow *win, unsigned int c)
{
    int i;
    if (c > 0 && c < 0x10000) {
        for (i = 0; i < ARRAY_SIZE(g_inputs->chars); i++) {
            if (!g_inputs->chars[i]) {
                g_inputs->chars[i] = c;
                break;
            }
        }
    }
}

static void generate_icons(goxel_t *goxel)
{
    layer_t *layer, *tmp;
    char path[256];
    goxel->image->export_width = 64;
    goxel->image->export_height = 64;

    DL_FOREACH(goxel->image->layers, layer) {
        DL_FOREACH(goxel->image->layers, tmp)
            tmp->visible = tmp == layer;

        goxel->rend.border_shadow = 0;
        goxel->rend.material.smoothness = 1;
        goxel->rend.material.specular = 0.2;
        goxel->rend.material.diffuse = 1;
        goxel->rend.material.ambient = 0.2;
        goxel->camera.ofs = vec3(-2, -8, 0);
        goxel->camera.rot = quat_identity;
        quat_irotate(&goxel->camera.rot, M_PI / 4, 1, 0, 0);
        quat_irotate(&goxel->camera.rot, M_PI / 4, 0, 1, 0);
        if (strcmp(layer->name, "move") == 0) {
            goxel->camera.ofs = vec3(0, 0, 0);
            goxel->camera.rot = quat_identity;
            quat_irotate(&goxel->camera.rot, M_PI / 2, 1, 0, 0);
            goxel->rend.material.smoothness = 0;
        }
        if (strcmp(layer->name, "grid") == 0) {
            goxel->camera.ofs = vec3(0, 0, 0);
            goxel->camera.rot = quat_identity;
            quat_irotate(&goxel->camera.rot, M_PI / 2, 1, 0, 0);
            goxel->rend.material.smoothness = 0;
        }

        goxel_update_meshes(goxel, true);
        sprintf(path, "./data/icons/%s.png", layer->name);
        goxel_export_as_png(goxel, path);
    }
}

typedef struct
{
    bool    icons;
    char    *args[1];
} args_t;

#ifndef WIN32
#include <argp.h>

const char *argp_program_version = "goxel " GOXEL_VERSION_STR;
static char doc[] = "A 3D voxels editor";
static char args_doc[] = "[FILE]";
#define OPT_ICONS 1
static struct argp_option options[] = {
    {"icons", OPT_ICONS, NULL, 0, "Generate the icons" },
    { 0 }
};

/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    args_t *args = state->input;

    switch (key)
    {
    case OPT_ICONS:
        args->icons = true;
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 2)
            argp_usage(state);
        args->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        if (args->icons && !args->args[0])
            argp_usage(state);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };
#endif

int main(int argc, char **argv)
{
    args_t args = {};
    GLFWwindow *window;
    goxel_t goxel;
    inputs_t inputs;
    g_inputs = &inputs;
    int w = 640;
    int h = 480;
    int i;
    double xpos, ypos;

#ifndef WIN32
    argp_parse (&argp, argc, argv, 0, 0, &args);
#endif

    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 2);

    window = glfwCreateWindow(w, h, "Goxel " GOXEL_VERSION_STR, NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetScrollCallback(window, on_scroll);
    glfwSetCharCallback(window, on_char);
    glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, false);
#ifdef WIN32
    glewInit();
#endif

    goxel_init(&goxel);
    if (args.args[0])
        load_from_file(&goxel, args.args[0]);
    if (args.icons) {
        generate_icons(&goxel);
        glfwTerminate();
        return 0;
    }

    while (!glfwWindowShouldClose(window)) {
        GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
        glfwGetWindowSize(window, &inputs.window_size[0],
                                  &inputs.window_size[1]);
        for (i = 0; i <= GLFW_KEY_LAST; i++) {
            inputs.keys[i] = glfwGetKey(window, i) == GLFW_PRESS;
        }
        glfwGetCursorPos(window, &xpos, &ypos);
        inputs.mouse_pos = vec2(xpos, ypos);
        inputs.mouse_down[0] =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        inputs.mouse_down[1] =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        inputs.mouse_down[2] =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        goxel_iter(&goxel, &inputs);
        goxel_render(&goxel);

        memset(&inputs, 0, sizeof(inputs));
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
