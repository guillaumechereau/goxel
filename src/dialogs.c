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

bool dialog_open(int flags, const char *filters, char **out)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;
    GtkFileChooser *chooser;
    GtkFileChooserAction action;
    gint res;

    action = flags & DIALOG_FLAG_SAVE ? GTK_FILE_CHOOSER_ACTION_SAVE :
                                        GTK_FILE_CHOOSER_ACTION_OPEN;
    if (flags & DIALOG_FLAG_DIR)
        action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

    gtk_init_check(NULL, NULL);
    dialog = gtk_file_chooser_dialog_new("Open File",
                                         NULL,
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT,
                                         NULL );
    chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

    while (filters && *filters) {
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, filters);
        filters += strlen(filters) + 1;
        gtk_file_filter_add_pattern(filter, filters);
        gtk_file_chooser_add_filter(chooser, filter);
        filters += strlen(filters) + 1;
    }
    // Add a default 'any' file pattern.
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "*");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(chooser, filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        *out = gtk_file_chooser_get_filename(chooser);
    }
    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();
    return res == GTK_RESPONSE_ACCEPT;
}

#endif

#ifdef WIN32

#include "Commdlg.h"
#include "Shlobj.h"


bool dialog_open(int flags, const char *filters, char **out)
{
    OPENFILENAME ofn;       // common dialog box structure
    BROWSEINFO   bif;       // only used to open directory
    LPITEMIDLIST lpItem;    // only for open directory
    char szFile[260];       // buffer for file name
    int ret;

    if (!(flags & DIALOG_FLAG_DIR)) {
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filters;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (flags & DIALOG_FLAG_OPEN)
            ret = GetOpenFileName(&ofn);
        else
            ret = GetSaveFileName(&ofn);
        if (ret == TRUE) {
            *out = strdup(szFile);
            return true;
        } else {
            return false;
        }
    } else { // Open a directory.
        ZeroMemory(&bif, sizeof(bif));
        bif.pszDisplayName = szFile;

        lpItem = SHBrowseForFolder(&bif);
        if (lpItem) {
            SHGetPathFromIDList(lpItem, szFile);
            *out = strdup(szFile);
            return true;
        } else {
            return false;
        }
    }
}

#endif
