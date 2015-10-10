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

#ifndef WIN32
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
#else

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
