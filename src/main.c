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

#if DEBUG
#   define DEBUG_ONLY(x) x
#else
#   define DEBUG_ONLY(x)
#endif

// For the moment I put an implementation using GLFW3 and one using GLUT.
// Both have some annoying problems, so I am not sure which one I should
// keep.  By default I use GLFW3, which is the that works the best so far.

#ifndef USE_GLUT
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
        if (strcmp(layer->name, "pick") == 0) {
            goxel->camera.ofs = vec3(0, 0, 0);
            goxel->camera.rot = quat_identity;
            quat_irotate(&goxel->camera.rot, M_PI / 2, 1, 0, 0);
            quat_irotate(&goxel->camera.rot, M_PI / 4, 0, 1, 0);
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

#ifndef NO_ARGP
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
    int fb_size[2], win_size[2];
    int i;
    double xpos, ypos;
    const char *title = "Goxel " GOXEL_VERSION_STR DEBUG_ONLY(" (debug)");

#ifndef NO_ARGP
    argp_parse (&argp, argc, argv, 0, 0, &args);
#endif

    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 2);

    window = glfwCreateWindow(w, h, title, NULL, NULL);
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
        // The input struct gets all the values in framebuffer coordinates,
        // On retina display, this might not be the same as the window
        // size.
        glfwGetFramebufferSize(window, &fb_size[0], &fb_size[1]);
        glfwGetWindowSize(window, &win_size[0], &win_size[1]);
        inputs.window_size[0] = fb_size[0];
        inputs.window_size[1] = fb_size[1];

        for (i = 0; i <= GLFW_KEY_LAST; i++) {
            inputs.keys[i] = glfwGetKey(window, i) == GLFW_PRESS;
        }
        glfwGetCursorPos(window, &xpos, &ypos);
        inputs.mouse_pos = vec2(xpos * fb_size[0] / win_size[0],
                                ypos * fb_size[1] / win_size[1]);
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

#else // GLUT implementation

#ifdef __APPLE__
#   include <GLUT/glut.h>
#else
#   include <GL/glut.h>
#endif


static goxel_t  *g_goxel = NULL;
static inputs_t *g_inputs = NULL;

static const int KEYS[] = {
    [GLUT_KEY_RIGHT]        = KEY_RIGHT,
    [GLUT_KEY_LEFT]         = KEY_LEFT,
    [GLUT_KEY_DOWN]         = KEY_DOWN,
    [GLUT_KEY_UP]           = KEY_UP,
};

static void on_display(void)
{
}

static void on_reshape(GLint width, GLint height)
{
    g_inputs->window_size[0] = width;
    g_inputs->window_size[1] = height;
}

static void set_modifiers(void)
{
    int v = glutGetModifiers();
    g_inputs->keys[KEY_SHIFT] = v & GLUT_ACTIVE_SHIFT;
    g_inputs->keys[KEY_CONTROL] = v & GLUT_ACTIVE_CTRL;
}

static void on_key_down(unsigned char key, int x, int y)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(g_inputs->chars); i++) {
        if (!g_inputs->chars[i]) {
            g_inputs->chars[i] = key;
            break;
        }
    }
    set_modifiers();
}

static void on_key_up(unsigned char key, int x, int y)
{
    set_modifiers();
}

static void on_special_key_down(int key, int x, int y)
{
    if (key < ARRAY_SIZE(KEYS))
        g_inputs->keys[KEYS[key]] = true;
}

static void on_special_key_up(int key, int x, int y)
{
    if (key < ARRAY_SIZE(KEYS))
        g_inputs->keys[KEYS[key]] = false;
}

static void on_mouse_button(int button, int state, int x, int y)
{
    if (button < 3) {
        g_inputs->mouse_pos = vec2(x, y);
        g_inputs->mouse_down[button] = state == GLUT_DOWN;
    }
    if (button == 3 && state == GLUT_DOWN)
        g_inputs->mouse_wheel = +1;
    if (button == 4 && state == GLUT_DOWN)
        g_inputs->mouse_wheel = -1;
    set_modifiers();
}

static void on_mouse_motion(int x, int y)
{
    g_inputs->mouse_pos = vec2(x, y);
}

static void on_passive_motion(int x, int y)
{
    g_inputs->mouse_pos = vec2(x, y);
}

static void on_timer(int value)
{
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    goxel_iter(g_goxel, g_inputs);
    g_inputs->mouse_wheel = 0;
    memset(g_inputs->chars, 0, sizeof(g_inputs->chars));
    goxel_render(g_goxel);
    glutSwapBuffers();
    glutTimerFunc(10, on_timer, 0);
}

int main(int argc, char **argv)
{
    int w = 640;
    int h = 480;
    goxel_t goxel;
    inputs_t inputs;

    g_goxel = &goxel;
    g_inputs = &inputs;
    memset(g_inputs, 0, sizeof(*g_inputs));

    glutInit(&argc, argv);
    glutInitWindowSize(w, h);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutCreateWindow("Goxel " GOXEL_VERSION_STR);

    goxel_init(&goxel);

    glutDisplayFunc(on_display);
    glutReshapeFunc(on_reshape);
    glutKeyboardFunc(on_key_down);
    glutKeyboardUpFunc(on_key_up);
    glutSpecialFunc(on_special_key_down);
    glutSpecialUpFunc(on_special_key_up);
    glutMouseFunc(on_mouse_button);
    glutMotionFunc(on_mouse_motion);
    glutPassiveMotionFunc(on_passive_motion);
    glutTimerFunc(10, on_timer, 0);

    glutMainLoop();
    return 0;
}

#endif
