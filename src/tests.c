/* Goxel 3D voxels editor
 *
 * copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
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

#define TEST(cond) \
    do { \
        if (!(cond)) { \
            LOG_E("Test fail: %s (%s: %d)", __func__, __FILE__, __LINE__); \
            exit(-1); \
        } \
    } while(0)

static void test_load_file_v2(void)
{
    // Load a simple file (encoded here as a base64 data) and check that
    // the crc32 of the mesh match.
    FILE *file;
    size_t data_size;
    uint32_t crc32;
    static const char *b64_data =
        "R09YIAIAAABJTUcgAAAAAAAAAABCTDE2LAEAAIlQTkcNChoKAAAADUlIRFIA"
        "AABAAAAAQAgGAAAAqmlx3gAAAPNJREFUeF5j/A8EDAwMjAwUgO+MjP85//8n"
        "14z/A2k/E8MwABQEPgMjJAEwjFjAxDDCAROjyvf/lIYB43cVss0YaPuHRQr4"
        "z3mH7EJ8tAwY8WXAaACM9FpgNAWMpoCRHQKjLcHRMmC0DBgtA0Z0CIwWgqOF"
        "4GghOFoIjhaCIzkERmuB0VpgtBYYrQVGa4HRWmAEh8BoNThaDY5Wg6PV4Gg1"
        "OFoNjlaDIzcERtsBo+2A0XbAaDtgtB0w2g4YbQeMtgNGbAiMNoRGG0KjDaHR"
        "htBoQ2i0ITTaEBptCI02hEZqCIy2BEdbgqMtwdGW4GhLcCSHAACDDx/OAZLa"
        "WgAAAABJRU5ErkJgggAAAABMQVlSmgAAAAEAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAQAAABuYW1lCgAAAGJhY2tncm91bmQDAAAAbWF0QAAAAAAAgD8AAAAA"
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAA"
        "AAAAAAAAAAAAgD8CAAAAaWQEAAAAAQAAAAcAAABiYXNlX2lkBAAAAAAAAAAA"
        "AAAA";
    uint8_t *data;
    data = calloc(b64_decode(b64_data, NULL), 1);
    data_size = b64_decode(b64_data, data);
    file = fopen("/tmp/goxel_test.gox", "w");
    fwrite(data, data_size, 1, file);
    fclose(file);
    free(data);
    action_exec2("import", "p", "/tmp/goxel_test.gox");
    crc32 = mesh_crc32(goxel->image->active_layer->mesh);
    TEST(crc32 == 0xf6aabf81);
    image_delete(goxel->image);
    goxel->image = image_new();
    goxel_update_meshes(goxel, -1);
}

void tests_run(void)
{
    test_load_file_v2();
}
