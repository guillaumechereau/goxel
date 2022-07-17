#include "goxel.h"

#ifdef GLES2
#   define GLFW_INCLUDE_ES2
#endif

#include <GLFW/glfw3.h>
#include <getopt.h>

static inputs_t     *g_inputs = NULL;
static GLFWwindow   *g_window = NULL;
static float        g_scale = 1;

typedef struct {
	char *input;
	char *export_path; // File Path To Save To
	float scale; // UI Scale
} args_t;

#define OPT_HELP 1
#define OPT_VERSION 2

typedef struct {
	const char *name;
	int val;
	int has_arg;
	const char *arg_name;
	const char *help;
} gox_option_t;

static const gox_option_t OPTIONS[] = {
	{"export", 'e', required_argument, "FILENAME",
		.help="Export the image to a file"},
	{"scale", 's', required_argument, "FLOAT", .help="Set UI scale"},
	{"help", OPT_HELP, .help="Give this help list"},
	{"version", OPT_VERSION, .help="Print program version"},
	{}
};

static void on_glfw_error(int code, const char *msg) {
	fprintf(stderr, "glfw error %d (%s)\n", code, msg);
	assert(false);
}

void on_scroll(GLFWwindow *win, double x, double y) {
	g_inputs->mouse_wheel = y;
}

void on_char(GLFWwindow *win, unsigned int c) {
	inputs_insert_char(g_inputs, c);
}

void on_drop(GLFWwindow* win, int count, const char** paths) {
	int i;
	for (i = 0;  i < count;  i++)
		goxel_import_file(paths[i], NULL);
}

void on_close(GLFWwindow *win) {
	glfwSetWindowShouldClose(win, GLFW_FALSE);
	gui_query_quit();
}

static void parse_options(int argc, char **argv, args_t *args) {
	int i, c, option_index;
	const gox_option_t *opt;
	struct option long_options[ARRAY_SIZE(OPTIONS)] = {};

	for (i = 0; i < (int)ARRAY_SIZE(OPTIONS); i++) {
		opt = &OPTIONS[i];
		long_options[i] = (struct option) {
			opt->name,
			opt->has_arg,
			NULL,
			opt->val,
		};
	}

	while (true) {
		c = getopt_long(argc, argv, "e:s:", long_options, &option_index);
		if (c == -1) break;
		switch (c) {
		case 'e':
			args->export_path = optarg;
			break;
		case 's':
			args->scale = atof(optarg);
			break;
		case OPT_HELP: {
			const gox_option_t *opt;
			char buf[128];

			printf("Usage: goxel2 [OPTION...] [INPUT]\n");
			printf("a 3D voxel art editor\n");
			printf("\n");

			for (opt = OPTIONS; opt->name; opt++) {
				if (opt->val >= 'a')
					printf("  -%c, ", opt->val);
				else
					printf("      ");

				if (opt->has_arg)
					snprintf(buf, sizeof(buf), "--%s=%s", opt->name, opt->arg_name);
				else
					snprintf(buf, sizeof(buf), "--%s", opt->name);
				printf("%-23s %s\n", buf, opt->help);
			}
			printf("\n");
			printf("Report bugs to https://github.com/pegvin/goxel2/issues\n");
			exit(0);
		}
		case OPT_VERSION:
			printf("Goxel2 " GOXEL_VERSION_STR "\n");
			exit(0);
		case '?':
			exit(-1);
		}
	}
	if (optind < argc) {
		args->input = argv[optind];
	}
}

#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 2
	static void load_icon(GLFWimage *image, const char *path) {
		uint8_t *img;
		int w, h, bpp = 0, size;
		const void *data;
		data = assets_get(path, &size);
		assert(data);
		img = img_read_from_mem((const char*)data, size, &w, &h, &bpp);
		assert(img);
		assert(bpp == 4);
		image->width = w;
		image->height = h;
		image->pixels = img;
	}

	static void set_window_icon(GLFWwindow *window) {
		GLFWimage icons[7];
		int i;
		load_icon(&icons[0], "asset://data/icons/icon16.png");
		load_icon(&icons[1], "asset://data/icons/icon24.png");
		load_icon(&icons[2], "asset://data/icons/icon32.png");
		load_icon(&icons[3], "asset://data/icons/icon48.png");
		load_icon(&icons[4], "asset://data/icons/icon64.png");
		load_icon(&icons[5], "asset://data/icons/icon128.png");
		load_icon(&icons[6], "asset://data/icons/icon256.png");
		glfwSetWindowIcon(window, 7, icons);
		for (i = 0; i < 7; i++) free(icons[i].pixels);
	}
#else
	static void set_window_icon(GLFWwindow *window) {}
#endif

static void set_window_title(void *user, const char *title) {
	glfwSetWindowTitle(g_window, title);
}

int main(int argc, char **argv) {
	args_t args = {.scale = 1};
	GLFWwindow *window;
	GLFWmonitor *monitor;
	const GLFWvidmode *mode;
	int width = 640, height = 480, ret = 0;
	inputs_t inputs = {};
	g_inputs = &inputs;

	// Setup sys callbacks.
	sys_callbacks.set_window_title = set_window_title;
	parse_options(argc, argv, &args);

	g_scale = args.scale;

	glfwSetErrorCallback(on_glfw_error);
	glfwInit();
	glfwWindowHint(GLFW_SAMPLES, 4);

	monitor = glfwGetPrimaryMonitor();
	mode = glfwGetVideoMode(monitor);

	if (mode) {
		width = mode->width ?: 640;
		height = mode->height ?: 480;
	}

	window = glfwCreateWindow(width, height, "Goxel2", NULL, NULL);
	assert(window);
	g_window = window;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // No Swap Interval, Swap ASAP
	glfwSetScrollCallback(window, on_scroll);
	glfwSetDropCallback(window, on_drop);
	glfwSetCharCallback(window, on_char);
	glfwSetWindowCloseCallback(window, on_close);
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, false);
	set_window_icon(window);

#ifdef WIN32
	glewInit();
#endif
	goxel_init();
	// Run the unit tests in debug.
	if (DEBUG) {
		tests_run();
		goxel_reset();
	}

	if (args.input)
		goxel_import_file(args.input, NULL);

	if (args.export_path) {
		if (!args.input) {
			LOG_E("trying to export an empty image");
			ret = -1;
		} else {
			ret = goxel_export_to_file(args.export_path, NULL);
		}

		glfwTerminate();
		goxel_release();
		return ret;
	}

#ifndef NDEBUG
	double lastTime = glfwGetTime();
	int nbFrames = 0; // Number Of Frames Rendered
#endif

	while (!glfwWindowShouldClose(g_window)) {
#ifndef NDEBUG
		double currentTime = glfwGetTime();
		nbFrames++;
		if (currentTime - lastTime >= 1.0) {
			printf("%f ms/frame\n", 1000.0 / (double)nbFrames);
			nbFrames = 0;
			lastTime += 1.0;
		}
#endif

		int fb_size[2], win_size[2];
		int i;
		double xpos, ypos;
		float scale;

		// If Window is Not Visible Or Iconified Don't Render Anything
		if (
			!glfwGetWindowAttrib(g_window, GLFW_FOCUSED) ||
			!glfwGetWindowAttrib(g_window, GLFW_VISIBLE) ||
			glfwGetWindowAttrib(g_window, GLFW_ICONIFIED)
		) {
			glfwWaitEvents();
			glfwPollEvents();

			if (goxel.quit) {
				break;
			}
			continue;
		}
		// The input struct gets all the values in framebuffer coordinates,
		// On retina display, this might not be the same as the window
		// size.
		glfwGetWindowSize(g_window, &win_size[0], &win_size[1]);

		glfwGetFramebufferSize(g_window, &fb_size[0], &fb_size[1]);
		scale = g_scale * (float)fb_size[0] / win_size[0];

		g_inputs->window_size[0] = win_size[0];
		g_inputs->window_size[1] = win_size[1];
		g_inputs->scale = scale;

		GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

		for (i = GLFW_KEY_SPACE; i <= GLFW_KEY_LAST; i++) {
			g_inputs->keys[i] = glfwGetKey(g_window, i) == GLFW_PRESS;
		}
		glfwGetCursorPos(g_window, &xpos, &ypos);
		vec2_set(g_inputs->touches[0].pos, xpos, ypos);
		g_inputs->touches[0].down[0] =
			glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
		g_inputs->touches[0].down[1] =
			glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
		g_inputs->touches[0].down[2] =
			glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

		goxel_iter(g_inputs);
		goxel_render();

		memset(g_inputs, 0, sizeof(*g_inputs));
		glfwSwapBuffers(g_window);
		glfwPollEvents();

		if (goxel.quit) break;
	}

	glfwTerminate();
	goxel_release();
	return ret;
}
