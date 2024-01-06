/* Goxel 3D voxels editor
 *
 * copyright (c) 2015-present Guillaume Chereau <guillaume@noctua-software.com>
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

#include "strings.h"

#include <assert.h>
#include <stdio.h>

typedef struct {
    const char *en;
    const char *fr;
} string_t;

static const string_t STRINGS[];

const char *tr(int id)
{
    // Only return the English for now.
    if (!STRINGS[id].en) fprintf(stderr, "Str %d not defined.\n", id);
    assert(STRINGS[id].en);
    return STRINGS[id].en;
}

static const string_t STRINGS[] = {
    [STR_ADD] = {
        .en = "Add",
        .fr = "Ajouter"
    },
    [STR_BRUSH] = {
        .en = "Brush",
        .fr = "Brosse",
    },
    [STR_COLOR_PICKER] = {
        .en = "Color Piper",
        .fr = "Pipette de couleur",
    },
    [STR_EXTRUDE] = {
        .en = "Extrude",
        .fr = "Extruder",
    },
    [STR_FUZZY_SELECT] = {
        .en = "Fuzzy Select",
        .fr = "Sélection contiguë",
    },
    [STR_IMAGE_CUBE] = {
        .en = "Image Cube",
        .fr = "Image cube",
    },
    [STR_LASER] = {
        .en = "Laser",
        .fr = "Laser",
    },
    [STR_LINE] = {
        .en = "Line",
        .fr = "Ligne",
    },
    [STR_MOVE] = {
        .en = "Move",
        .fr = "Déplacer",
    },
    [STR_OFFSET] = {
        .en = "Offset",
        .fr = "Décalage",
    },
    [STR_PLANE] = {
        .en = "Plane",
        .fr = "Plan",
    },
    [STR_RECT_SELECT] = {
        .en = "Rectangle Select",
        .fr = "Sélection rectangulaire",
    },
    [STR_SELECTION] = {
        .en = "Selection",
        .fr = "Sélection",
    },
    [STR_SELECTION_IN] = {
        .en = "Selection (In)",
        .fr = "Sélection (Int.)",
    },
    [STR_SELECTION_OUT] = {
        .en = "Selection (Out)",
        .fr = "Sélection (Ext.)",
    },
    [STR_SET] = {
        .en = "Set",
        .fr = "Définir",
    },
    [STR_SHAPE] = {
        .en = "Shape",
        .fr = "Forme",
    },
    [STR_SUB] = {
        .en = "Sub",
        .fr = "Soustraire",
    },
    [STR_SNAP] = {
        .en = "Snap",
        .fr = "Aimanter",
    },
    [STR_VOLUME] = {
        .en = "Volume",
        .fr = "Volume",
    },
};
