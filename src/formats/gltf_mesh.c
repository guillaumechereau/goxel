/* Greedy voxel meshing and per-layer texture atlases for glTF export.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "goxel.h"
#include "gltf_mesh.h"

typedef struct {
    int direction, plane, v, u;
    uint8_t color[4];
    bool used;
} face_t;

typedef struct {
    face_t *face;
    int width, height;
    int x, y; // Atlas origin, including a one-texel gutter.
    bool rotated;
} rect_t;

static int face_cmp(const void *a_, const void *b_)
{
    const face_t *a = a_, *b = b_;
#define CMP(field) \
    if (a->field != b->field) return a->field < b->field ? -1 : 1
    CMP(direction);
    CMP(plane);
    CMP(v);
    CMP(u);
#undef CMP
    return 0;
}

static face_t *find_face(face_t *faces, int count, const face_t *origin,
                         int u, int v)
{
    face_t key = {
        .direction = origin->direction, .plane = origin->plane,
        .u = u, .v = v,
    };
    return bsearch(&key, faces, count, sizeof(*faces), face_cmp);
}

static bool can_merge(const face_t *face, const face_t *origin,
                      bool vertex_color)
{
    return face && !face->used &&
           (!vertex_color || memcmp(face->color, origin->color, 4) == 0);
}

// Store only exposed faces, not a dense bounding-box mask: volumes may be
// sparse, and merging must work across tile boundaries and negative positions.
static face_t *collect_faces(const volume_t *volume, int *count)
{
    volume_iterator_t iter;
    volume_accessor_t accessor;
    face_t *faces = NULL, *face;
    int pos[3], neighbor[3], d, sign, capacity = 0;
    uint8_t color[4], adjacent[4];

    *count = 0;
    accessor = volume_get_accessor(volume);
    iter = volume_get_iterator(volume, VOLUME_ITER_SKIP_EMPTY);
    while (volume_iter(&iter, pos)) {
        volume_get_at(volume, &iter, pos, color);
        if (color[3] < 127) continue;
        color[3] = 255; // Match the block mesher's binary occupancy threshold.
        for (d = 0; d < 3; d++) {
            for (sign = -1; sign <= 1; sign += 2) {
                memcpy(neighbor, pos, sizeof(neighbor));
                neighbor[d] += sign;
                volume_get_at(volume, &accessor, neighbor, adjacent);
                if (adjacent[3] >= 127) continue;
                if (*count == capacity) {
                    capacity = max(256, capacity * 2);
                    faces = realloc(faces, capacity * sizeof(*faces));
                }
                face = &faces[(*count)++];
                *face = (face_t) {
                    .direction = d * 2 + (sign > 0),
                    .plane = pos[d] + (sign > 0),
                    .u = pos[(d + 1) % 3],
                    .v = pos[(d + 2) % 3],
                };
                memcpy(face->color, color, 4);
            }
        }
    }
    if (*count) qsort(faces, *count, sizeof(*faces), face_cmp);
    return faces;
}

static rect_t *merge_faces(face_t *faces, int count, bool vertex_color,
                          int atlas_size, int *rect_count)
{
    rect_t *rects = NULL, *rect;
    face_t *face, *next;
    int i, u, v, width, height, capacity = 0;
    bool full_row;

    *rect_count = 0;
    for (i = 0; i < count; i++) {
        face = &faces[i];
        if (face->used) continue;
        width = 1;
        while ((vertex_color || width < atlas_size - 2) &&
               can_merge(find_face(faces, count, face,
                                   face->u + width, face->v),
                         face, vertex_color)) width++;
        height = 1;
        while (vertex_color || height < atlas_size - 2) {
            full_row = true;
            for (u = 0; u < width; u++) {
                next = find_face(faces, count, face,
                                 face->u + u, face->v + height);
                if (!can_merge(next, face, vertex_color)) {
                    full_row = false;
                    break;
                }
            }
            if (!full_row) break;
            height++;
        }
        if (*rect_count == capacity) {
            capacity = max(64, capacity * 2);
            rects = realloc(rects, capacity * sizeof(*rects));
        }
        rect = &rects[(*rect_count)++];
        *rect = (rect_t) {.face = face, .width = width, .height = height};
        for (v = 0; v < height; v++) {
            for (u = 0; u < width; u++) {
                find_face(faces, count, face,
                          face->u + u, face->v + v)->used = true;
            }
        }
    }
    return rects;
}

static int rect_height_cmp(const void *a_, const void *b_)
{
    const rect_t *a = a_, *b = b_;
    int ah = min(a->width, a->height), bh = min(b->width, b->height);
    int aw = max(a->width, a->height), bw = max(b->width, b->height);
    if (ah != bh) return ah > bh ? -1 : 1;
    if (aw != bw) return aw > bw ? -1 : 1;
    return face_cmp(a->face, b->face);
}

static int pow2_ceil(int size)
{
    int ret = 1;
    while (ret < size) ret *= 2;
    return ret;
}

static uint8_t *make_atlas(rect_t *rects, int rect_count,
                           face_t *faces, int count, int atlas_size,
                           int size[2], int *packed)
{
    size_t area = 0;
    size_t max_area = (size_t)atlas_size * atlas_size;
    uint8_t *pixels;
    face_t *face;
    rect_t *rect;
    int i, x, y, row_height = 0, u, v, width = 1, rw, rh, used_height = 0;

    for (i = 0; i < rect_count; i++) {
        area += (size_t)(rects[i].width + 2) * (rects[i].height + 2);
        width = max(width, max(rects[i].width, rects[i].height) + 2);
        if (area >= max_area) {
            area = max_area;
            width = atlas_size;
            break;
        }
    }
    size[0] = pow2_ceil(max(width, (int)ceil(sqrt((double)area))));
    x = y = 0;
    for (i = 0; i < rect_count; i++) {
        rect = &rects[i];
        // Rotate tall patches to avoid a nearly empty square atlas for beams.
        rect->rotated = rect->height > rect->width;
        rw = max(rect->width, rect->height) + 2;
        rh = min(rect->width, rect->height) + 2;
        if (x + rw > size[0]) {
            x = 0;
            y += row_height;
            row_height = 0;
        }
        if (y + rh > atlas_size) break;
        rect->x = x;
        rect->y = y;
        row_height = max(row_height, rh);
        x += rw;
        used_height = max(used_height, y + rh);
    }
    *packed = i;
    assert(*packed > 0);
    size[1] = pow2_ceil(used_height);
    pixels = calloc((size_t)size[0] * size[1], 3);
    for (i = 0; i < *packed; i++) {
        rect = &rects[i];
        // Extrude edge colors into the gutter as well as filling the patch.
        for (v = -1; v <= rect->height; v++) {
            for (u = -1; u <= rect->width; u++) {
                face = find_face(faces, count, rect->face,
                        rect->face->u + max(0, min(u, rect->width - 1)),
                        rect->face->v + max(0, min(v, rect->height - 1)));
                x = rect->x + 1 + (rect->rotated ? v : u);
                y = rect->y + 1 + (rect->rotated ? u : v);
                memcpy(pixels + ((size_t)y * size[0] + x) * 3,
                       face->color, 3);
            }
        }
    }
    return pixels;
}

static volume_mesh_t *make_mesh(rect_t *rects, int rect_count,
                                bool vertex_color, float simplify,
                                const int texture_size[2])
{
    static const int corners[2][4][2] = {
        {{0, 0}, {0, 1}, {1, 1}, {1, 0}}, // Negative normal.
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}}, // Positive normal.
    };
    static const int indices[6] = {0, 1, 2, 2, 3, 0};
    volume_mesh_t *mesh = calloc(1, sizeof(*mesh));
    rect_t *rect;
    int i, j, d, u, v, sign, idx, k;

    mesh->vertices_count = rect_count * 4;
    mesh->indices_count = rect_count * 6;
    mesh->vertices = calloc(mesh->vertices_count, sizeof(*mesh->vertices));
    mesh->indices = calloc(mesh->indices_count, sizeof(*mesh->indices));
    for (i = 0; i < rect_count; i++) {
        rect = &rects[i];
        d = rect->face->direction / 2;
        sign = rect->face->direction % 2;
        for (j = 0; j < 4; j++) {
            idx = i * 4 + j;
            u = corners[sign][j][0] * rect->width;
            v = corners[sign][j][1] * rect->height;
            mesh->vertices[idx].pos[d] = rect->face->plane;
            mesh->vertices[idx].pos[(d + 1) % 3] = rect->face->u + u;
            mesh->vertices[idx].pos[(d + 2) % 3] = rect->face->v + v;
            mesh->vertices[idx].normal[d] = sign ? 1 : -1;
            if (vertex_color) {
                srgba8_to_rgba(rect->face->color, mesh->vertices[idx].color);
            } else {
                mesh->vertices[idx].texcoord[0] =
                    (float)(rect->x + 1 + (rect->rotated ? v : u)) /
                    texture_size[0];
                mesh->vertices[idx].texcoord[1] =
                    (float)(rect->y + 1 + (rect->rotated ? u : v)) /
                    texture_size[1];
            }
        }
        for (j = 0; j < 6; j++)
            mesh->indices[i * 6 + j] = i * 4 + indices[j];
    }
    volume_mesh_optimize(mesh, simplify);
    for (k = 0; k < 3; k++) {
        mesh->pos_min[k] = FLT_MAX;
        mesh->pos_max[k] = -FLT_MAX;
        for (i = 0; i < mesh->vertices_count; i++) {
            mesh->pos_min[k] = min(mesh->pos_min[k], mesh->vertices[i].pos[k]);
            mesh->pos_max[k] = max(mesh->pos_max[k], mesh->vertices[i].pos[k]);
        }
    }
    return mesh;
}

gltf_mesh_part_t *gltf_generate_greedy_meshes(
        const volume_t *volume, bool vertex_color, float simplify,
        int atlas_size)
{
    gltf_mesh_part_t *parts = NULL, **tail = &parts, *part;
    face_t *faces;
    rect_t *rects;
    int count, rect_count, offset = 0, packed;

    assert(atlas_size >= 4 && atlas_size <= GLTF_ATLAS_SIZE &&
           (atlas_size & (atlas_size - 1)) == 0);
    faces = collect_faces(volume, &count);
    rects = merge_faces(faces, count, vertex_color, atlas_size, &rect_count);
    if (!vertex_color && rect_count)
        qsort(rects, rect_count, sizeof(*rects), rect_height_cmp);
    while (offset < rect_count) {
        part = calloc(1, sizeof(*part));
        packed = rect_count - offset;
        if (!vertex_color) {
            part->texture = make_atlas(rects + offset, packed, faces, count,
                                      atlas_size, part->texture_size, &packed);
        }
        part->mesh = make_mesh(rects + offset, packed, vertex_color, simplify,
                               part->texture_size);
        *tail = part;
        tail = &part->next;
        offset += packed;
    }
    free(rects);
    free(faces);
    return parts;
}

void gltf_mesh_parts_free(gltf_mesh_part_t *parts)
{
    gltf_mesh_part_t *next;

    while (parts) {
        next = parts->next;
        volume_mesh_free(parts->mesh);
        free(parts->texture);
        free(parts);
        parts = next;
    }
}
