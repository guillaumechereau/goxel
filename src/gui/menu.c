#include "goxel.h"

#include "file_format.h"

int gui_settings_popup(void *data);
int gui_about_popup(void *data);

#if defined(_WIN32)
    #include <windows.h>
    #include <shellapi.h>
#endif

void OpenUrlInBrowser(const char* url) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
    char command[4096];
#endif

#if defined(__linux__) || defined(__FreeBSD__)
    sprintf(command, "xdg-open \"%s\"", url);
    system(command);
#elif defined(__APPLE__)
    sprintf(command, "open \"%s\"", url);
    system(command);
#elif defined(_WIN32)
    ShellExecute(0, 0, url, 0, 0, SW_SHOW);
#endif
}

static void import_image_plane(void)
{
    const char *path;
    path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN,
            "png\0*.png\0jpg\0*.jpg;*.jpeg\0", NULL, NULL);
    if (!path) return;
    goxel_import_image_plane(path);
}

static void import_menu_callback(void *user, const file_format_t *f)
{
    if (gui_menu_item(0, f->name, true))
        goxel_import_file(NULL, f->name);
}

static void export_menu_callback(void *user, const file_format_t *f)
{
    if (gui_menu_item(0, f->name, true))
        goxel_export_to_file(NULL, f->name);
}

void gui_menu(void)
{
    if (gui_menu_begin("File")) {
        gui_menu_item(ACTION_reset, "New", true);
        gui_menu_item(ACTION_open, "Open", true);
        gui_menu_item(ACTION_save, "Save", image_get_key(goxel.image) != goxel.image->saved_key);
        gui_menu_item(ACTION_save_as, "Save as", true);
        gui_menu_item(ACTION_open_run_lua_plugin, "Run Plugin", true);

        if (gui_menu_begin("Import...")) {
            if (gui_menu_item(0, "image plane", true))
                import_image_plane();
            file_format_iter("r", NULL, import_menu_callback);
            gui_menu_end();
        }

        if (gui_menu_begin("Export As..")) {
            file_format_iter("w", NULL, export_menu_callback);
            gui_menu_end();
        }

        gui_menu_item(ACTION_quit, "Quit", true);
        gui_menu_end();
    }

    if (gui_menu_begin("Edit")) {
        gui_menu_item(ACTION_layer_clear, "Clear", true);
        gui_menu_item(ACTION_undo, "Undo", true);
        gui_menu_item(ACTION_redo, "Redo", true);
        gui_menu_item(ACTION_copy, "Copy", true);
        gui_menu_item(ACTION_past, "Paste", true);
        if (gui_menu_item(0, "Settings", true))
            gui_open_popup("Settings", GUI_POPUP_FULL | GUI_POPUP_RESIZE,
                           NULL, gui_settings_popup);
        gui_menu_end();
    }
    if (gui_menu_begin("View")) {
        gui_menu_item(ACTION_view_left, "Left", true);
        gui_menu_item(ACTION_view_right, "Right", true);
        gui_menu_item(ACTION_view_front, "Front", true);
        gui_menu_item(ACTION_view_top, "Top", true);
        gui_menu_item(ACTION_view_default, "Default", true);
        gui_menu_end();
    }
    if (gui_menu_begin("Help")) {
        if (gui_menu_item(0, "About", true))
            gui_open_popup("About", 0, NULL, gui_about_popup);

        if (gui_menu_item(0, "Discord", true))
            OpenUrlInBrowser("https://discord.gg/YXx3afnzzW");

        if (gui_menu_item(0, "Discussions", true))
            OpenUrlInBrowser("https://github.com/pegvin/goxel2/discussions");

        if (gui_menu_item(0, "How To Lua?", true))
            OpenUrlInBrowser("https://github.com/pegvin/goxel2/wiki/Lua-API");

        if (gui_menu_item(0, "Found a bug?", true))
            OpenUrlInBrowser("https://github.com/pegvin/goxel2/issues/new/choose");

        if (gui_menu_item(0, "Want to contribute?", true))
            OpenUrlInBrowser("https://github.com/pegvin/goxel2/blob/master/CONTRIBUTING.md");

        gui_menu_end();
    }
}
