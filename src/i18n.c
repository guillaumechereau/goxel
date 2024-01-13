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
#include <string.h>

static const tr_lang_t LANGUAGES[] = {
    {"en", "English"},
    {"fr", "Français"},
    {}
};

typedef struct {
    union {
        struct {
            const char *en;
            const char *fr;
        };
        const char *values[2];
    };
} string_t;

static const string_t STRINGS[];

static int current_lang_idx = 0;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

const char *tr(int id)
{
    // Only return the English for now.
    if (!STRINGS[id].en) fprintf(stderr, "Str %d not defined.\n", id);
    assert(STRINGS[id].en);
    return STRINGS[id].values[current_lang_idx];
}

void tr_set_language(const char *id)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(LANGUAGES); i++) {
        if (strcmp(LANGUAGES[i].id, id) == 0) {
            current_lang_idx = i;
            break;
        }
    }
}

const tr_lang_t *tr_get_language(void)
{
    return &LANGUAGES[current_lang_idx];
}

const tr_lang_t *tr_get_supported_languages(void)
{
    return LANGUAGES;
}

static const string_t STRINGS[] = {
    [STR_2D_IMAGE] = {
        .en = "2D Image",
        .fr = "Image 2D",
    },
    [STR_ACTION_ADD_SELECTION_HELP] = {
        .en = "Adds selected area to the mask",
        .fr = "Ajoute la zone sélectionnée au masque",
    },
    [STR_ACTION_CLONE_LAYER_HELP] = {
        .en = "Creates a linked copy of the selected layer",
        .fr = "Crée une copie liée du calque sélectionné",
    },
    [STR_ACTION_CUT_AS_NEW_LAYER] = {
        .en = "Cuts the selected area into a new layer",
        .fr = "Coupe la zone sélectionnée dans un nouveau calque",
    },
    [STR_ACTION_FILL_SELECTION_HELP] = {
        .en = "Fills the selected area with the current color",
        .fr = "Remplit la zone sélectionnée avec la couleur actuelle",
    },
    [STR_ACTION_IMAGE_AUTO_RESIZE_HELP] = {
        .en = "Resizes the image to fit the layers",
        .fr = "Redimensionne l'image pour l'adapter aux calques",
    },
    [STR_ACTION_IMAGE_LAYER_TO_VOLUME_HELP] = {
        .en = "Converts a 2d image into a volume",
        .fr = "Convertit une image 2D en volume",
    },
    [STR_ACTION_LAYER_TO_VOLUME_HELP] = {
        .en = "Converts a layer into a volume",
        .fr = "Convertit un calque en volume",
    },
    [STR_ACTION_NEW_SHAPE_LAYER_HELP] = {
        .en = "Creates a shape layer",
        .fr = "Crée un calque de forme",
    },
    [STR_ACTION_RESET_HELP] = {
        .en = "Creates a new image (discards the current one)",
        .fr = "Crée une nouvelle image (rejete l'image actuelle)",
    },
    [STR_ACTION_SET_MODE_ADD_HELP] = {
        .en = "Adds to the voxels",
        .fr = "Ajoute aux voxels",
    },
    [STR_ACTION_SET_MODE_PAINT_HELP] = {
        .en = "Paints the voxels",
        .fr = "Peint les voxels",
    },
    [STR_ACTION_SET_MODE_SUB_HELP] = {
        .en = "Subtract to the voxels",
        .fr = "Soustraire aux voxels",
    },
    [STR_ACTION_SUB_SELECTION_HELP] = {
        .en = "Subtracts selected area from the mask",
        .fr = "Soustrait la zone sélectionnée du masque",
    },
    [STR_ACTION_TOGGLE_MODE_HELP] = {
        .en = "Switches the mode (add/subtract/paint)",
        .fr = "Change le mode (ajouter/soustraire/peindre)",
    },
    [STR_ACTION_UNLINK_LAYER_HELP] = {
        .en = "Unlinks the selected layer from its parent",
        .fr = "Dissocie le calque sélectionné de son parent",
    },
    [STR_ACTION_VIEW_TOGGLE_ORTHO_HELP] = {
        .en = "Switches the projection (perspective/orthographic)",
        .fr = "Change la projection (perspective/orthographique)",
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
    [STR_ANGLE] = {
        .en = "Angle",
        .fr = "Angle",
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
    [STR_DISTANCE] = {
        .en = "Distance",
        .fr = "Distance",
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
    [STR_ENVIRONMENT] = {
        .en = "Environment",
        .fr = "Environnement",
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
    [STR_FIT_TO_CONTENT] = {
        .en = "Fit to Content",
        .fr = "Ajuster au contenu",
    },
    [STR_FIXED] = {
        .en = "Fixed",
        .fr = "Fixe",
    },
    [STR_FLOOR] = {
        .en = "Floor",
        .fr = "Plancher",
    },
    [STR_FORMAT] = {
        .en = "Format",
        .fr = "Format",
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
    [STR_INTENSITY] = {
        .en = "Intensity",
        .fr = "Intensité",
    },
    [STR_IMAGE] = {
        .en = "Image",
        .fr = "Image",
    },
    [STR_IMPORT] = {
        .en = "Import",
        .fr = "Importer",
    },
    [STR_LANGUAGE] = {
        .en = "Language",
        .fr = "Langue",
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
    [STR_NONE] = {
        .en = "None",
        .fr = "Aucun",
    },
    [STR_OCCLUSION] = {
        .en = "Occlusion",
        .fr = "Occlusion",
    },
    [STR_OFFSET] = {
        .en = "Offset",
        .fr = "Décalage",
    },
    [STR_OK] = {
        .en = "OK",
        .fr = "OK",
    },
    [STR_OPEN] = {
        .en = "Open",
        .fr = "Ouvrir",
    },
    [STR_ORIGIN] = {
        .en = "Origin",
        .fr = "Origine",
    },
    [STR_ORTHOGRAPHIC] = {
        .en = "Orthographic",
        .fr = "Orthogonale",
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
    [STR_PATHS] = {
        .en = "Paths",
        .fr = "Chemins",
    },
    [STR_PERSPECTIVE] = {
        .en = "Perspective",
        .fr = "Perspective",
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
    [STR_RESTART] = {
        .en = "Restart",
        .fr = "Redémarrer",
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
    [STR_SAMPLES] = {
        .en = "Samples",
        .fr = "Échantillons",
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
    [STR_SHADOW] = {
        .en = "Shadow",
        .fr = "Ombre",
    },
    [STR_SHAPE] = {
        .en = "Shape",
        .fr = "Forme",
    },
    [STR_SHORTCUTS] = {
        .en = "Shortcuts",
        .fr = "Raccourcis",
    },
    [STR_SIZE] = {
        .en = "Size",
        .fr = "Taille",
    },
    [STR_SKY] = {
        .en = "Sky",
        .fr = "Ciel",
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
    [STR_START] = {
        .en = "Start",
        .fr = "Démarrer",
    },
    [STR_STOP] = {
        .en = "Stop",
        .fr = "Stopper",
    },
    [STR_SUCCESS] = {
        .en = "Success",
        .fr = "Succès",
    },
    [STR_SYMMETRY] = {
        .en = "Symmetry",
        .fr = "Symétrie",
    },
    [STR_THEME] = {
        .en = "Theme",
        .fr = "Thème",
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
    [STR_UNIFORM] = {
        .en = "Uniform",
        .fr = "Uniforme",
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
    [STR_WORLD] = {
        .en = "World",
        .fr = "Monde",
    },
};
