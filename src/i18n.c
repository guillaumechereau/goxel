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

#include "i18n.h"

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
    [STR_2D_IMAGE] = {
        .en = "2D Image",
        .fr = "Image 2D",
    },
    [STR_ADD] = {
        .en = "Add",
        .fr = "Ajouter"
    },
    [STR_ADJUSTEMENTS] = {
        .en = "Adjustments",
        .fr = "Ajustements",
    },
    [STR_ALPHA] = {
        .en = "Alpha",
        .fr = "Alpha",
    },
    [STR_ANTIALIASING] = {
        .en = "Antialiasing",
        .fr = "Anti-aliasing",
    },
    [STR_BACKGROUND] = {
        .en = "Background",
        .fr = "Arrière-plan",
    },
    [STR_BORDER] = {
        .en = "Border",
        .fr = "Bordures",
    },
    [STR_BOUNDED] = {
        .en = "Bounded",
        .fr = "Bornée",
    },
    [STR_BOX] = {
        .en = "Box",
        .fr = "Boîte",
    },
    [STR_BRUSH] = {
        .en = "Brush",
        .fr = "Brosse",
    },
    [STR_CAMERAS] = {
        .en = "Cameras",
        .fr = "Caméras",
    },
    [STR_CLEAR] = {
        .en = "Clear",
        .fr = "Effacer",
    },
    [STR_CLONE] = {
        .en = "Clone",
        .fr = "Cloner",
    },
    [STR_COLOR] = {
        .en = "Color",
        .fr = "Couleur",
    },
    [STR_COLORS] = {
        .en = "Colors",
        .fr = "Couleurs",
    },
    [STR_COLOR_PICKER] = {
        .en = "Color Piper",
        .fr = "Pipette de couleur",
    },
    [STR_COPY] = {
        .en = "Copy",
        .fr = "Copier",
    },
    [STR_CROP] = {
        .en = "Crop",
        .fr = "Rogner",
    },
    [STR_CROP_TO_IMAGE] = {
        .en = "Crop to Image",
        .fr = "Rogner selon l'image",
    },
    [STR_CUBE] = {
        .en = "Cube",
        .fr = "Cube",
    },
    [STR_CYLINDER] = {
        .en = "Cylinder",
        .fr = "Cylindre",
    },
    [STR_DEBUG] = {
        .en = "Debug",
        .fr = "Débug",
    },
    [STR_DUPLICATE] = {
        .en = "Duplicate",
        .fr = "Dupliquer",
    },
    [STR_EDIT] = {
        .en = "Edit",
        .fr = "Édition",
    },
    [STR_EDGES] = {
        .en = "Edges",
        .fr = "Arêtes",
    },
    [STR_EFFECTS] = {
        .en = "Effects",
        .fr = "Effets",
    },
    [STR_EXPORT] = {
        .en = "Export",
        .fr = "Exporter",
    },
    [STR_EXTRUDE] = {
        .en = "Extrude",
        .fr = "Extruder",
    },
    [STR_FILE] = {
        .en = "File",
        .fr = "Fichier",
    },
    [STR_FRONT] = {
        .en = "Front",
        .fr = "Avant",
    },
    [STR_FUZZY_SELECT] = {
        .en = "Fuzzy Select",
        .fr = "Sélection contiguë",
    },
    [STR_GRID] = {
        .en = "Grid",
        .fr = "Grille",
    },
    [STR_HIDE_BOX] = {
        .en = "Hide Box",
        .fr = "Cacher la Boîte",
    },
    [STR_IMAGE] = {
        .en = "Image",
        .fr = "Image",
    },
    [STR_IMPORT] = {
        .en = "Import",
        .fr = "Importer",
    },
    [STR_LASER] = {
        .en = "Laser",
        .fr = "Laser",
    },
    [STR_LAYERS] = {
        .en = "Layers",
        .fr = "Calques",
    },
    [STR_LEFT] = {
        .en = "Left",
        .fr = "Gauche",
    },
    [STR_LIGHT] = {
        .en = "Light",
        .fr = "Éclairage",
    },
    [STR_LINE] = {
        .en = "Line",
        .fr = "Ligne",
    },
    [STR_MARCHING_CUBES] = {
        .en = "Marching Cubes",
        .fr = "Marching cubes",
    },
    [STR_MASK] = {
        .en = "Mask",
        .fr = "Masque",
    },
    [STR_MATERIAL] = {
        .en = "Material",
        .fr = "Matériau",
    },
    [STR_MATERIALS] = {
        .en = "Materials",
        .fr = "Matériaux",
    },
    [STR_MERGE] = {
        .en = "Merge",
        .fr = "Fusionner",
    },
    [STR_MOVE] = {
        .en = "Move",
        .fr = "Déplacer",
    },
    [STR_NEW] = {
        .en = "New",
        .fr = "Nouveau",
    },
    [STR_OCCLUSION] = {
        .en = "Occlusion",
        .fr = "Occlusion",
    },
    [STR_OFFSET] = {
        .en = "Offset",
        .fr = "Décalage",
    },
    [STR_OPEN] = {
        .en = "Open",
        .fr = "Ouvrir",
    },
    [STR_PAINT] = {
        .en = "Paint",
        .fr = "Peindre",
    },
    [STR_PALETTE] = {
        .en = "Palette",
        .fr = "Palette",
    },
    [STR_PASTE] = {
        .en = "Paste",
        .fr = "Coller",
    },
    [STR_PLANE] = {
        .en = "Plane",
        .fr = "Plan",
    },
    [STR_QUIT] = {
        .en = "Quit",
        .fr = "Quitter",
    },
    [STR_RENDER] = {
        .en = "Render",
        .fr = "Rendu",
    },
    [STR_RECT_SELECT] = {
        .en = "Rectangle Select",
        .fr = "Sélection rectangulaire",
    },
    [STR_REDO] = {
        .en = "Redo",
        .fr = "Refaire",
    },
    [STR_RESET] = {
        .en = "Reset",
        .fr = "Réinitialiser",
    },
    [STR_RESIZE] = {
        .en = "Resize",
        .fr = "Redimensionner",
    },
    [STR_RIGHT] = {
        .en = "Right",
        .fr = "Droite",
    },
    [STR_SAVE] = {
        .en = "Save",
        .fr = "Enregistrer",
    },
    [STR_SAVE_AS] = {
        .en = "Save As",
        .fr = "Enregistrer sous",
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
    [STR_SETTINGS] = {
        .en = "Settings",
        .fr = "Réglages",
    },
    [STR_SHADELESS] = {
        .en = "Shadeless",
        .fr = "Sans ombrage",
    },
    [STR_SHAPE] = {
        .en = "Shape",
        .fr = "Forme",
    },
    [STR_SIZE] = {
        .en = "Size",
        .fr = "Taille",
    },
    [STR_SMOOTH] = {
        .en = "Smooth",
        .fr = "Adoucir",
    },
    [STR_SMOOTHNESS] = {
        .en = "Smoothness",
        .fr = "Adoucissement",
    },
    [STR_SPHERE] = {
        .en = "Sphere",
        .fr = "Sphère",
    },
    [STR_SUB] = {
        .en = "Sub",
        .fr = "Soustraire",
    },
    [STR_SNAP] = {
        .en = "Snap",
        .fr = "Aimanter",
    },
    [STR_SYMMETRY] = {
        .en = "Symmetry",
        .fr = "Symétrie",
    },
    [STR_TOOLS] = {
        .en = "Tools",
        .fr = "Outils",
    },
    [STR_TOP] = {
        .en = "Top",
        .fr = "Dessus",
    },
    [STR_TRANSPARENT] = {
        .en = "Transparent",
        .fr = "Transparent",
    },
    [STR_UNDO] = {
        .en = "Undo",
        .fr = "Annuler",
    },
    [STR_UNLINK] = {
        .en = "Unlink",
        .fr = "Dé-lier",
    },
    [STR_VIEW] = {
        .en = "View",
        .fr = "Vue",
    },
    [STR_VOLUME] = {
        .en = "Volume",
        .fr = "Volume",
    },
};
