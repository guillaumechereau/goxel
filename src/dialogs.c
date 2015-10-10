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

#if defined __linux__

// GTK implementation.
#include <gtk/gtk.h>

bool sys_save_dialog(const char *type, char **path)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;
    GtkFileChooser *chooser;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
    gint res;

    gtk_init_check(NULL, NULL);
    dialog = gtk_file_chooser_dialog_new("Save File",
                                         NULL,
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Save", GTK_RESPONSE_ACCEPT,
                                         NULL );
    chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

    chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

    while (type && *type) {
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, type);
        type += strlen(type) + 1;
        gtk_file_filter_add_pattern(filter, type);
        gtk_file_chooser_add_filter(chooser, filter);
        type += strlen(type) + 1;
    }

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        *path = gtk_file_chooser_get_filename(chooser);
    }
    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();
    return res == GTK_RESPONSE_ACCEPT;
}

bool sys_open_dialog(const char *type, char **path)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;
    GtkFileChooser *chooser;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    gtk_init_check(NULL, NULL);
    dialog = gtk_file_chooser_dialog_new("Open File",
                                         NULL,
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT,
                                         NULL );
    chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

    while (type && *type) {
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, type);
        type += strlen(type) + 1;
        gtk_file_filter_add_pattern(filter, type);
        gtk_file_chooser_add_filter(chooser, filter);
        type += strlen(type) + 1;
    }
    // Add a default 'any' file pattern.
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "*");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(chooser, filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        *path = gtk_file_chooser_get_filename(chooser);
    }
    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();
    return res == GTK_RESPONSE_ACCEPT;
}

#endif

# if TARGET_OS_MAC == 1
#include "nfd.h"
bool sys_save_dialog(const char *type, char **path)
{
    nfdresult_t result = NFD_SaveDialog(type, NULL, path);
    return result == NFD_OKAY;
}

bool sys_open_dialog(const char *type, char **path)
{
    nfdresult_t result = NFD_OpenDialog(type, NULL, path);
    return result == NFD_OKAY;
}

#endif

#ifdef WIN32

#include "Commdlg.h"

bool sys_save_dialog(const char *type, char **path)
{
    OPENFILENAME ofn;       // common dialog box structure
    char szFile[260];       // buffer for file name

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = type;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetSaveFileName(&ofn) == TRUE) {
        *path = strdup(szFile);
        return true;
    }
    return false;
}

bool sys_open_dialog(const char *type, char **path)
{
    OPENFILENAME ofn;       // common dialog box structure
    char szFile[260];       // buffer for file name

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = type;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileName(&ofn) == TRUE) {
        *path = strdup(szFile);
        return true;
    }
    return false;
}

#endif
