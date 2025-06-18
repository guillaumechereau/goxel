/* Goxel 3D voxels editor
 *
 * copyright (c) 2025 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

// Minecraft schematic format support.

#include "file_format.h"
#include "goxel.h"
#include "json/json.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Block mapping structure
typedef struct {
    uint8_t rgb[3];
    uint8_t block_id;
    uint8_t metadata;
    char display_name[256];
} block_mapping_t;

static block_mapping_t *g_block_mapping = NULL;
static int g_mapping_count = 0;

// NBT writing functions
static void write_nbt_byte(FILE *file, uint8_t value) {
    fwrite(&value, 1, 1, file);
}

static void write_nbt_short(FILE *file, uint16_t value) {
    value = htons(value);  // Convert to big-endian
    fwrite(&value, 2, 1, file);
}

static void write_nbt_int(FILE *file, uint32_t value) {
    value = htonl(value);  // Convert to big-endian
    fwrite(&value, 4, 1, file);
}

static void write_nbt_string(FILE *file, const char *str) {
    uint16_t len = strlen(str);
    write_nbt_short(file, len);
    fwrite(str, len, 1, file);
}

static void write_nbt_byte_array(FILE *file, const uint8_t *data, uint32_t length) {
    write_nbt_int(file, length);
    fwrite(data, length, 1, file);
}

// NBT tag types
#define NBT_TAG_BYTE       1
#define NBT_TAG_SHORT      2
#define NBT_TAG_INT        3
#define NBT_TAG_STRING     8
#define NBT_TAG_BYTE_ARRAY 7
#define NBT_TAG_COMPOUND   10
#define NBT_TAG_END        0

static void write_nbt_tag_header(FILE *file, uint8_t tag_type, const char *name) {
    write_nbt_byte(file, tag_type);
    write_nbt_string(file, name);
}

// Find best matching block for RGB color
static void rgb_to_block(uint8_t rgb[3], uint8_t *block_id, uint8_t *metadata) {
    int best_dist = INT_MAX;
    int best_idx = 0;  // Default to air
    
    for (int i = 0; i < g_mapping_count; i++) {
        int dr = (int)rgb[0] - (int)g_block_mapping[i].rgb[0];
        int dg = (int)rgb[1] - (int)g_block_mapping[i].rgb[1];
        int db = (int)rgb[2] - (int)g_block_mapping[i].rgb[2];
        int dist = dr*dr + dg*dg + db*db;
        
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }
        
        if (dist == 0) break;  // Perfect match
    }
    
    *block_id = g_block_mapping[best_idx].block_id;
    *metadata = g_block_mapping[best_idx].metadata;
}

// Load block mapping from JSON file
static int load_block_mapping(void) {
    const char *json_str = NULL;
    int file_size;
    json_value *root = NULL;
    int ret = -1;
    
    // Get the mapping file from assets
    json_str = (const char*)assets_get("asset://data/other/minecraft_block_mapping.json", &file_size);
    if (!json_str) {
        LOG_E("Could not load asset://data/other/minecraft_block_mapping.json");
        return -1;
    }
    
    // Parse JSON
    root = json_parse(json_str, file_size);
    if (!root) {
        LOG_E("Failed to parse minecraft_block_mapping.json");
        goto cleanup;
    }
    
    // Get mapping object
    json_value *mapping = NULL;
    for (unsigned int i = 0; i < root->u.object.length; i++) {
        if (strcmp(root->u.object.values[i].name, "mapping") == 0) {
            mapping = root->u.object.values[i].value;
            break;
        }
    }
    
    if (!mapping || mapping->type != json_object) {
        LOG_E("No 'mapping' object found in JSON");
        goto cleanup;
    }
    
    // Allocate mapping array
    g_mapping_count = mapping->u.object.length;
    g_block_mapping = calloc(g_mapping_count, sizeof(block_mapping_t));
    if (!g_block_mapping) goto cleanup;
    
    // Parse each mapping entry
    for (unsigned int i = 0; i < mapping->u.object.length; i++) {
        json_value *entry = mapping->u.object.values[i].value;
        if (entry->type != json_object) continue;
        
        // Parse RGB values
        json_value *rgb_array = NULL;
        json_value *display_name = NULL;
        json_value *block_id = NULL;
        
        for (unsigned int j = 0; j < entry->u.object.length; j++) {
            const char *key = entry->u.object.values[j].name;
            json_value *value = entry->u.object.values[j].value;
            
            if (strcmp(key, "rgb") == 0 && value->type == json_array) {
                rgb_array = value;
            } else if (strcmp(key, "display_name") == 0 && value->type == json_string) {
                display_name = value;
            } else if (strcmp(key, "block_id") == 0 && value->type == json_string) {
                block_id = value;
            }
        }
        
        if (rgb_array && rgb_array->u.array.length >= 3) {
            g_block_mapping[i].rgb[0] = (uint8_t)rgb_array->u.array.values[0]->u.integer;
            g_block_mapping[i].rgb[1] = (uint8_t)rgb_array->u.array.values[1]->u.integer;
            g_block_mapping[i].rgb[2] = (uint8_t)rgb_array->u.array.values[2]->u.integer;
        }
        
        if (display_name) {
            snprintf(g_block_mapping[i].display_name, sizeof(g_block_mapping[i].display_name),
                    "%s", display_name->u.string.ptr);
        }
        
        if (block_id) {
            // Parse block_id:metadata format
            const char *colon = strchr(block_id->u.string.ptr, ':');
            if (colon) {
                g_block_mapping[i].block_id = atoi(block_id->u.string.ptr);
                g_block_mapping[i].metadata = atoi(colon + 1);
            } else {
                g_block_mapping[i].block_id = atoi(block_id->u.string.ptr);
                g_block_mapping[i].metadata = 0;
            }
        }
    }
    
    ret = 0;
    LOG_I("Loaded %d block mappings", g_mapping_count);
    
cleanup:
    if (root) json_value_free(root);
    return ret;
}

static int schematic_export(const file_format_t *format, const image_t *image,
                           const char *path) {
    FILE *file;
    gzFile gz_file;
    const volume_t *volume;
    volume_iterator_t iter;
    int pos[3];
    uint8_t color[4];
    int ret = -1;
    
    // Bounding box calculation
    int xmin = INT_MAX, ymin = INT_MAX, zmin = INT_MAX;
    int xmax = INT_MIN, ymax = INT_MIN, zmax = INT_MIN;
    int voxel_count = 0;
    
    // Load block mapping if not already loaded
    if (!g_block_mapping && load_block_mapping() != 0) {
        return -1;
    }
    
    volume = goxel_get_layers_volume(image);
    
    // First pass: calculate bounding box and count voxels
    iter = volume_get_iterator(volume, VOLUME_ITER_VOXELS);
    while (volume_iter(&iter, pos)) {
        volume_get_at(volume, &iter, pos, color);
        if (color[3] < 127) continue;  // Skip transparent voxels
        
        voxel_count++;
        xmin = min(xmin, pos[0]);
        ymin = min(ymin, pos[1]);
        zmin = min(zmin, pos[2]);
        xmax = max(xmax, pos[0] + 1);
        ymax = max(ymax, pos[1] + 1);
        zmax = max(zmax, pos[2] + 1);
    }
    
    if (voxel_count == 0) {
        LOG_W("No voxels to export");
        return 0;
    }
    
    // Calculate dimensions
    int width = xmax - xmin;
    int height = ymax - ymin;
    int length = zmax - zmin;
    int total_blocks = width * height * length;
    
    LOG_I("Exporting schematic: %dx%dx%d (%d voxels)", width, height, length, voxel_count);
    
    // Allocate arrays for blocks and data
    uint8_t *blocks = calloc(total_blocks, sizeof(uint8_t));
    uint8_t *data = calloc(total_blocks, sizeof(uint8_t));
    if (!blocks || !data) goto cleanup;
    
    // Second pass: fill the arrays
    iter = volume_get_iterator(volume, VOLUME_ITER_VOXELS);
    while (volume_iter(&iter, pos)) {
        volume_get_at(volume, &iter, pos, color);
        if (color[3] < 127) continue;
        
        // Convert Goxel coordinates to Minecraft coordinates
        int mc_x = pos[0] - xmin;
        int mc_y = pos[2] - zmin;  // Goxel Z -> Minecraft Y
        int mc_z = pos[1] - ymin;  // Goxel Y -> Minecraft Z
        
        // Calculate index in YZX order for Minecraft schematic format
        int index = mc_y * width * length + mc_z * width + mc_x;
        if (index >= 0 && index < total_blocks) {
            uint8_t block_id, metadata;
            rgb_to_block(color, &block_id, &metadata);
            blocks[index] = block_id;
            data[index] = metadata;
        }
    }
    
    // Create temporary file for uncompressed NBT
    char temp_path[1024];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    
    file = fopen(temp_path, "wb");
    if (!file) {
        LOG_E("Could not create temporary file: %s", temp_path);
        goto cleanup;
    }
    
    // Write NBT structure
    write_nbt_tag_header(file, NBT_TAG_COMPOUND, "Schematic");
    
    write_nbt_tag_header(file, NBT_TAG_SHORT, "Width");
    write_nbt_short(file, width);
    
    write_nbt_tag_header(file, NBT_TAG_SHORT, "Height");
    write_nbt_short(file, height);
    
    write_nbt_tag_header(file, NBT_TAG_SHORT, "Length");
    write_nbt_short(file, length);
    
    write_nbt_tag_header(file, NBT_TAG_STRING, "Materials");
    write_nbt_string(file, "Alpha");
    
    write_nbt_tag_header(file, NBT_TAG_BYTE_ARRAY, "Blocks");
    write_nbt_byte_array(file, blocks, total_blocks);
    
    write_nbt_tag_header(file, NBT_TAG_BYTE_ARRAY, "Data");
    write_nbt_byte_array(file, data, total_blocks);
    
    write_nbt_byte(file, NBT_TAG_END);  // End compound tag
    
    fclose(file);
    
    // Compress with gzip
    FILE *temp_file = fopen(temp_path, "rb");
    if (!temp_file) goto cleanup;
    
    gz_file = gzopen(path, "wb");
    if (!gz_file) {
        fclose(temp_file);
        goto cleanup;
    }
    
    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), temp_file)) > 0) {
        gzwrite(gz_file, buffer, bytes_read);
    }
    
    fclose(temp_file);
    gzclose(gz_file);
    remove(temp_path);  // Clean up temporary file
    
    ret = 0;
    LOG_I("Successfully exported Minecraft schematic to %s", path);
    
cleanup:
    free(blocks);
    free(data);
    return ret;
}

FILE_FORMAT_REGISTER(schematic,
    .name = "Minecraft Schematic",
    .exts = {"*.schematic"},
    .exts_desc = "schematic",
    .export_func = schematic_export,
)