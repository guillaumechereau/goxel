/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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
#include "file_format.h"
#include <errno.h>

static int import_as_txt(image_t *image, const char *path)
{
    FILE *file;
    char line[2048];
    layer_t *layer;
    mesh_iterator_t iter = {0};
    int pos[3];
    unsigned int c[4];
    char *token;

    LOG_I("Reading text file. One line per voxel. Format should be: X Y Z RRGGBB");
    file = fopen(path, "r");

    //Checks if file is empty
    if (file == NULL) {
        LOG_E("Can not open file for reading: %s", path);
        return 1;
    }

    layer = image->active_layer;

    while (fgets(line, 2048, file)) {
        token = strtok(line, " "); // get first token (X)
        if (strcmp(token, "#") == 0)
            continue;
        pos[0] = atoi(token);
        token = strtok(NULL, " "); // get second token (Y)
        pos[1] = atoi(token);
        token = strtok(NULL, " "); // get third token (Z)
        pos[2] = atoi(token);
        token = strtok(NULL, " "); // get forth token (RRGGBB)
        sscanf(token, "%02x%02x%02x", &c[0], &c[1], &c[2]);
        mesh_set_at(layer->mesh, &iter, pos, (uint8_t[]){c[0], c[1], c[2], 255});
    }

    fclose(file);
    return 0;
}


static int export_as_txt(const image_t *image, const char *path)
{
    FILE *out;
    const mesh_t *mesh = goxel_get_layers_mesh(image);
    int p[3];
    uint8_t v[4];
    mesh_iterator_t iter;

    out = fopen(path, "w");
    if (!out) {
        LOG_E("Cannot save to %s: %s", path, strerror(errno));
        return -1;
    }
    fprintf(out, "# Goxel " GOXEL_VERSION_STR "\n");
    fprintf(out, "# One line per voxel\n");
    fprintf(out, "# X Y Z RRGGBB\n");

    iter = mesh_get_iterator(mesh, MESH_ITER_VOXELS);
    while (mesh_iter(&iter, p)) {
        mesh_get_at(mesh, &iter, p, v);
        if (v[3] < 127) continue;
        fprintf(out, "%d %d %d %02x%02x%02x\n",
                p[0], p[1], p[2], v[0], v[1], v[2]);
    }
    fclose(out);
    return 0;
}

FILE_FORMAT_REGISTER(txt,
    .name = "text",
    .ext = "text\0*.txt\0",
    .import_func = import_as_txt,
    .export_func = export_as_txt,
)
