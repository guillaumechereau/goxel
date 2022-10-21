//
// LICENCE:
//  The MIT License (MIT)
//
//  Copyright (c) 2016 Karim Naaji, karim.naaji@gmail.com
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE
//
// REFERENCES:
//  http://matthias-mueller-fischer.ch/publications/tetraederCollision.pdf
//  http://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/tribox2.txt
//
// HOWTO:
//  #define VOXELIZER_IMPLEMENTATION
//  #define VOXELIZER_DEBUG // Only if assertions need to be checked
//  #include "voxelizer.h"
//
// HISTORY:
//  - 0.10.0 (20-03-2017): Add vx_voxelize_snap_3d_grid to voxelize to 3d-textures
//  - 0.9.2  (03-01-2017): Fix triangle bounding bouxes bounds for bbox-triangle
//                         intersection test
//  - 0.9.1  (12-10-2016): Add vx_voxelize_pc to generate point cloud as a result
//                         of voxelization
//  - 0.9    (01-05-2016): Initial
//
// TODO:
//  - Triangle face merging
//

#ifndef VOXELIZER_H
#define VOXELIZER_H

// ------------------------------------------------------------------------------------------------
// VOXELIZER PUBLIC API
//

#ifndef VOXELIZER_HELPERS
#include <stdlib.h> // malloc, calloc, free
#endif

typedef struct vx_vertex {
    union {
        float v[3];
        struct {
            float x;
            float y;
            float z;
        };
        struct {
            float r;
            float g;
            float b;
        };
    };
} vx_vertex_t;

typedef vx_vertex_t vx_vec3_t;
typedef vx_vertex_t vx_color_t;

typedef struct vx_mesh {
    vx_vertex_t* vertices;          // Contiguous mesh vertices
    vx_color_t* colors;             // Contiguous vertices colors
    vx_vec3_t* normals;             // Contiguous mesh normals
    unsigned int* indices;          // Mesh indices
    unsigned int* normalindices;    // Mesh normal indices
    size_t nindices;                // The number of normal indices
    size_t nvertices;               // The number of vertices
    size_t nnormals;                // The number of normals
} vx_mesh_t;

typedef struct vx_point_cloud {
    vx_vertex_t* vertices;          // Contiguous point cloud vertices positions, each vertex corresponds
                                    // to the center of a voxel
    vx_color_t* colors;             // Contiguous point cloud vertices colors
    size_t nvertices;               // The number of vertices in the point cloud
} vx_point_cloud_t;

// vx_voxelize_pc: Voxelizes a triangle mesh to a point cloud
vx_point_cloud_t* vx_voxelize_pc(vx_mesh_t const* mesh, // The input mesh
                                 float voxelsizex,      // Voxel size on X-axis
                                 float voxelsizey,      // Voxel size on Y-axis
                                 float voxelsizez,      // Voxel size on Z-axis
                                 float precision);      // A precision factor that reduces "holes artifact
                                                        // usually a precision = voxelsize / 10. works ok

// vx_voxelize: Voxelizes a triangle mesh to a triangle mesh representing cubes
vx_mesh_t* vx_voxelize(vx_mesh_t const* mesh,       // The input mesh
        float voxelsizex,                           // Voxel size on X-axis
        float voxelsizey,                           // Voxel size on Y-axis
        float voxelsizez,                           // Voxel size on Z-axis
        float precision);                           // A precision factor that reduces "holes" artifact
                                                    // usually a precision = voxelsize / 10. works ok.

// vx_voxelize_snap_3d_grid: Voxelizes a triangle mesh to a 3d texture
// The texture data is aligned as RGBA8 and can be uploaded as a 3d texture with OpenGL like so:
// glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, width, height, depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, texturedata);
unsigned int* vx_voxelize_snap_3dgrid(vx_mesh_t const* mesh, // The input mesh
        unsigned int width,                                  // The texture resolution on x-axis
        unsigned int height,                                 // The texture resolution on y-axis
        unsigned int depth);                                 // The texture resolution on z-axis


// Allocates a mesh that can contain nvertices vertices, nindices indices
vx_mesh_t* vx_mesh_alloc(int nvertices, int nindices);

// Allocates a mesh that can contain nvertices vertices and colors, nindices indices
vx_mesh_t* vx_color_mesh_alloc(int nvertices, int nindices);

// Free a mesh allocated with vx_mesh_alloc, vx_color_mesh_alloc or after a call to vx_voxelize
void vx_mesh_free(vx_mesh_t* mesh);

// Free a point cloud allocated after a call of vx_voxelize_pc
void vx_point_cloud_free(vx_point_cloud_t* pointcloud);

// Voxelizer Helpers, define your own if needed
#ifndef VOXELIZER_HELPERS
#define VOXELIZER_HELPERS 1
#define VX_MIN(a, b) (a > b ? b : a)
#define VX_MAX(a, b) (a > b ? a : b)
#define VX_FINDMINMAX(x0, x1, x2, min, max) \
    min = max = x0;                         \
    if (x1 < min) min = x1;                 \
    if (x1 > max) max = x1;                 \
    if (x2 < min) min = x2;                 \
    if (x2 > max) max = x2;
#define VX_CLAMP(v, lo, hi) VX_MAX(lo, VX_MIN(hi, v))
#define VX_MALLOC(T, N) ((T*) malloc(N * sizeof(T)))
#define VX_FREE(T) free(T)
#define VX_CALLOC(T, N) ((T*) calloc(N * sizeof(T), 1))
#define VX_SWAP(T, A, B) { T tmp = B; B = A; A = tmp; }
#ifdef VOXELIZER_DEBUG
#define VX_ASSERT(STMT) if (!(STMT)) { *(int *)0 = 0; }
#else
#define VX_ASSERT(STMT)
#endif // VOXELIZER_DEBUG
#endif // VOXELIZER_HELPERS

//
// END VOXELIZER PUBLIC API
// ------------------------------------------------------------------------------------------------

#endif // VOXELIZER_H

#ifdef VOXELIZER_IMPLEMENTATION

#include <math.h>       // ceil, fabs & al.
#include <stdbool.h>    // hughh
#include <string.h>     // memcpy

#define VOXELIZER_EPSILON               (0.0000001)
#define VOXELIZER_NORMAL_INDICES_SIZE   (6)
#define VOXELIZER_INDICES_SIZE          (36)
#define VOXELIZER_HASH_TABLE_SIZE       (4096)

unsigned int vx_voxel_indices[VOXELIZER_INDICES_SIZE] = {
    0, 1, 2,
    0, 2, 3,
    3, 2, 6,
    3, 6, 7,
    0, 7, 4,
    0, 3, 7,
    4, 7, 5,
    7, 6, 5,
    0, 4, 5,
    0, 5, 1,
    1, 5, 6,
    1, 6, 2,
};

float vx_normals[18] = {
     0.0, -1.0,  0.0,
     0.0,  1.0,  0.0,
     1.0,  0.0,  0.0,
     0.0,  0.0,  1.0,
    -1.0,  0.0,  0.0,
     0.0,  0.0, -1.0,
};

unsigned int vx_normal_indices[VOXELIZER_NORMAL_INDICES_SIZE] = {
    3, 2, 1, 5, 4, 0,
};

typedef struct vx_aabb {
    vx_vertex_t min;
    vx_vertex_t max;
} vx_aabb_t;

typedef struct vx_edge {
    vx_vertex_t p1;
    vx_vertex_t p2;
} vx_edge_t;

typedef struct vx_triangle {
    union {
        vx_vertex_t vertices[3];
        struct {
            vx_vertex_t p1;
            vx_vertex_t p2;
            vx_vertex_t p3;
        };
    };
    vx_color_t colors[3];
} vx_triangle_t;

typedef struct vx_hash_table_node {
    struct vx_hash_table_node* next;
    struct vx_hash_table_node* prev;
    void* data;
} vx_hash_table_node_t;

typedef struct vx_hash_table {
    vx_hash_table_node_t** elements;
    size_t size;
} vx_hash_table_t;

typedef struct vx_voxel_data {
    vx_vec3_t position;
    vx_color_t color;
} vx_voxel_data_t;

vx_hash_table_t* vx__hash_table_alloc(size_t size)
{
    vx_hash_table_t* table = VX_MALLOC(vx_hash_table_t, 1);
    table->size = size;
    table->elements = VX_CALLOC(vx_hash_table_node_t*, size);

    return table;
}

void vx__hash_table_free(vx_hash_table_t* table)
{
    for (size_t i = 0; i < table->size; ++i) {
        vx_hash_table_node_t* node = table->elements[i];

        if (node) {
            if (node->next) {
                while (node->next) {
                    node = node->next;
                    VX_FREE(node->prev->data);
                    VX_FREE(node->prev);
                }
                VX_FREE(node);
            } else {
                VX_FREE(node->data);
                VX_FREE(node);
            }
        }
    }

    VX_FREE(table->elements);
    VX_FREE(table);
}

bool vx__hash_table_insert(vx_hash_table_t* table,
    size_t hash,
    void* data,
    bool (*compfunc)(void* d1, void* d2))
{
    if (!table->elements[hash]) {
        table->elements[hash] = VX_MALLOC(vx_hash_table_node_t, 1);
        table->elements[hash]->prev = NULL;
        table->elements[hash]->next = NULL;
        table->elements[hash]->data = data;
    } else {
        vx_hash_table_node_t* node = table->elements[hash];

        if (compfunc && compfunc(node->data, data)) {
            return false;
        }

        while (node->next) {
            node = node->next;
            if (compfunc && compfunc(node->data, data)) {
                return false;
            }
        }

        vx_hash_table_node_t* nnode = VX_MALLOC(vx_hash_table_node_t, 1);

        nnode->prev = node;
        nnode->next = NULL;
        nnode->data = data;
        node->next = nnode;
    }

    return true;
}

void vx_mesh_free(vx_mesh_t* mesh)
{
    VX_FREE(mesh->vertices);
    mesh->vertices = NULL;
    mesh->nvertices = 0;
    VX_FREE(mesh->indices);
    mesh->indices = NULL;
    VX_FREE(mesh->normalindices);
    mesh->normalindices = NULL;
    mesh->nindices = 0;
    VX_FREE(mesh->normals);
    mesh->normals = NULL;
    VX_FREE(mesh->colors);
    mesh->colors = NULL;
    VX_FREE(mesh);
}

void vx_point_cloud_free(vx_point_cloud_t* pc)
{
    VX_FREE(pc->vertices);
    pc->vertices = NULL;
    VX_FREE(pc->colors);
    pc->colors = NULL;
    pc->nvertices = 0;
    VX_FREE(pc);
}

vx_mesh_t* vx_mesh_alloc(int nvertices, int nindices)
{
    vx_mesh_t* mesh = VX_MALLOC(vx_mesh_t, 1);
    mesh->indices = VX_CALLOC(unsigned int, nindices);
    mesh->normalindices = VX_CALLOC(unsigned int, nindices);
    mesh->vertices = VX_CALLOC(vx_vertex_t, nvertices);
    mesh->normals = VX_CALLOC(vx_vec3_t, nvertices);
    mesh->colors = VX_CALLOC(vx_color_t, nvertices);
    mesh->nindices = nindices;
    mesh->nnormals = nvertices;
    mesh->nvertices = nvertices;
    return mesh;
}

vx_mesh_t* vx_color_mesh_alloc(int nvertices, int nindices)
{
    vx_mesh_t* mesh = vx_mesh_alloc(nvertices, nindices);
    mesh->colors = VX_CALLOC(vx_color_t, nvertices);
    if (!mesh->colors) { return NULL; }
    return mesh;
}

float vx__map_to_voxel(float position, float voxelSize, bool min)
{
    float vox = (position + (position < 0.f ? -1.f : 1.f) * voxelSize * 0.5f) / voxelSize;
    return (min ? floor(vox) : ceil(vox)) * voxelSize;
}

vx_vec3_t vx__vec3_cross(vx_vec3_t* v1, vx_vec3_t* v2)
{
    vx_vec3_t cross;
    cross.x = v1->y * v2->z - v1->z * v2->y;
    cross.y = v1->z * v2->x - v1->x * v2->z;
    cross.z = v1->x * v2->y - v1->y * v2->x;
    return cross;
}

bool vx__vertex_equals_epsilon(vx_vertex_t* v1, vx_vertex_t* v2) {
    return fabs(v1->x - v2->x) < VOXELIZER_EPSILON &&
           fabs(v1->y - v2->y) < VOXELIZER_EPSILON &&
           fabs(v1->z - v2->z) < VOXELIZER_EPSILON;
}

bool vx__vertex_comp_func(void* a, void* b)
{
    return vx__vertex_equals_epsilon((vx_vertex_t*) a, (vx_vertex_t*) b);
}

void vx__vec3_sub(vx_vec3_t* a, vx_vec3_t* b)
{
    a->x -= b->x;
    a->y -= b->y;
    a->z -= b->z;
}

float vx__vec3_length2(vx_vec3_t* v)
{
    return v->x * v->x + v->y * v->y + v->z * v->z;
}

void vx__vec3_add(vx_vec3_t* a, vx_vec3_t* b)
{
    a->x += b->x;
    a->y += b->y;
    a->z += b->z;
}

void vx__vec3_multiply(vx_vec3_t* a, float v)
{
    a->x *= v;
    a->y *= v;
    a->z *= v;
}

float vx__vec3_dot(vx_vec3_t* v1, vx_vec3_t* v2)
{
    return v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
}

int vx__plane_box_overlap(vx_vec3_t* normal,
    float d,
    vx_vertex_t* halfboxsize)
{
    vx_vertex_t vmin, vmax;

    for (int dim = 0; dim <= 2; dim++) {
        if (normal->v[dim] > 0.0f) {
            vmin.v[dim] = -halfboxsize->v[dim];
            vmax.v[dim] = halfboxsize->v[dim];
        } else {
            vmin.v[dim] = halfboxsize->v[dim];
            vmax.v[dim] = -halfboxsize->v[dim];
        }
    }

    if (vx__vec3_dot(normal, &vmin) + d > 0.0f) {
        return false;
    }

    if (vx__vec3_dot(normal, &vmax) + d >= 0.0f) {
        return true;
    }

    return false;
}

#define AXISTEST_X01(a, b, fa, fb)                 \
    p1 = a * v1.y - b * v1.z;                      \
    p3 = a * v3.y - b * v3.z;                      \
    if (p1 < p3) {                                 \
        min = p1; max = p3;                        \
    } else {                                       \
        min = p3; max = p1;                        \
    }                                              \
    rad = fa * halfboxsize.y + fb * halfboxsize.z; \
    if (min > rad || max < -rad) {                 \
        return false;                              \
    }                                              \

#define AXISTEST_X2(a, b, fa, fb)                  \
    p1 = a * v1.y - b * v1.z;                      \
    p2 = a * v2.y - b * v2.z;                      \
    if (p1 < p2) {                                 \
        min = p1; max = p2;                        \
    } else {                                       \
        min = p2; max = p1;                        \
    }                                              \
    rad = fa * halfboxsize.y + fb * halfboxsize.z; \
    if (min > rad || max < -rad) {                 \
        return false;                              \
    }                                              \

#define AXISTEST_Y02(a, b, fa, fb)                 \
    p1 = -a * v1.x + b * v1.z;                     \
    p3 = -a * v3.x + b * v3.z;                     \
    if (p1 < p3) {                                 \
        min = p1; max = p3;                        \
    } else {                                       \
        min = p3; max = p1;                        \
    }                                              \
    rad = fa * halfboxsize.x + fb * halfboxsize.z; \
    if (min > rad || max < -rad) {                 \
        return false;                              \
    }                                              \

#define AXISTEST_Y1(a, b, fa, fb)                  \
    p1 = -a * v1.x + b * v1.z;                     \
    p2 = -a * v2.x + b * v2.z;                     \
    if (p1 < p2) {                                 \
        min = p1; max = p2;                        \
    } else {                                       \
        min = p2; max = p1;                        \
    }                                              \
    rad = fa * halfboxsize.x + fb * halfboxsize.z; \
    if (min > rad || max < -rad) {                 \
        return false;                              \
    }

#define AXISTEST_Z12(a, b, fa, fb)                 \
    p2 = a * v2.x - b * v2.y;                      \
    p3 = a * v3.x - b * v3.y;                      \
    if (p3 < p2) {                                 \
        min = p3; max = p2;                        \
    } else {                                       \
        min = p2; max = p3;                        \
    }                                              \
    rad = fa * halfboxsize.x + fb * halfboxsize.y; \
    if (min > rad || max < -rad) {                 \
        return false;                              \
    }

#define AXISTEST_Z0(a, b, fa, fb)                  \
    p1 = a * v1.x - b * v1.y;                      \
    p2 = a * v2.x - b * v2.y;                      \
    if (p1 < p2) {                                 \
        min = p1; max = p2;                        \
    } else {                                       \
        min = p2; max = p1;                        \
    }                                              \
    rad = fa * halfboxsize.x + fb * halfboxsize.y; \
    if (min > rad || max < -rad) {                 \
        return false;                              \
    }

int vx__triangle_box_overlap(vx_vertex_t boxcenter,
    vx_vertex_t halfboxsize,
    vx_triangle_t triangle)
{
    vx_vec3_t v1, v2, v3, normal, e1, e2, e3;
    float min, max, d, p1, p2, p3, rad, fex, fey, fez;

    v1 = triangle.p1;
    v2 = triangle.p2;
    v3 = triangle.p3;

    vx__vec3_sub(&v1, &boxcenter);
    vx__vec3_sub(&v2, &boxcenter);
    vx__vec3_sub(&v3, &boxcenter);

    e1 = v2;
    e2 = v3;
    e3 = v1;

    vx__vec3_sub(&e1, &v1);
    vx__vec3_sub(&e2, &v2);
    vx__vec3_sub(&e3, &v3);

    fex = fabs(e1.x);
    fey = fabs(e1.y);
    fez = fabs(e1.z);

    AXISTEST_X01(e1.z, e1.y, fez, fey);
    AXISTEST_Y02(e1.z, e1.x, fez, fex);
    AXISTEST_Z12(e1.y, e1.x, fey, fex);

    fex = fabs(e2.x);
    fey = fabs(e2.y);
    fez = fabs(e2.z);

    AXISTEST_X01(e2.z, e2.y, fez, fey);
    AXISTEST_Y02(e2.z, e2.x, fez, fex);
    AXISTEST_Z0(e2.y, e2.x, fey, fex);

    fex = fabs(e3.x);
    fey = fabs(e3.y);
    fez = fabs(e3.z);

    AXISTEST_X2(e3.z, e3.y, fez, fey);
    AXISTEST_Y1(e3.z, e3.x, fez, fex);
    AXISTEST_Z12(e3.y, e3.x, fey, fex);

    VX_FINDMINMAX(v1.x, v2.x, v3.x, min, max);
    if (min > halfboxsize.x || max < -halfboxsize.x) {
        return false;
    }

    VX_FINDMINMAX(v1.y, v2.y, v3.y, min, max);
    if (min > halfboxsize.y || max < -halfboxsize.y) {
        return false;
    }

    VX_FINDMINMAX(v1.z, v2.z, v3.z, min, max);
    if (min > halfboxsize.z || max < -halfboxsize.z) {
        return false;
    }

    normal = vx__vec3_cross(&e1, &e2);
    d = -vx__vec3_dot(&normal, &v1);

    if (!vx__plane_box_overlap(&normal, d, &halfboxsize)) {
        return false;
    }

    return true;
}

#undef AXISTEST_X2
#undef AXISTEST_X01
#undef AXISTEST_Y1
#undef AXISTEST_Y02
#undef AXISTEST_Z0
#undef AXISTEST_Z12

float vx__triangle_area(vx_triangle_t* triangle) {
    vx_vec3_t ab = triangle->p2;
    vx_vec3_t ac = triangle->p3;

    vx__vec3_sub(&ab, &triangle->p1);
    vx__vec3_sub(&ac, &triangle->p1);

    float a0 = ab.y * ac.z - ab.z * ac.y;
    float a1 = ab.z * ac.x - ab.x * ac.z;
    float a2 = ab.x * ac.y - ab.y * ac.x;

    return sqrtf(powf(a0, 2.f) + powf(a1, 2.f) + powf(a2, 2.f)) * 0.5f;
}

void vx__aabb_init(vx_aabb_t* aabb)
{
    aabb->max.x = aabb->max.y = aabb->max.z = -INFINITY;
    aabb->min.x = aabb->min.y = aabb->min.z = INFINITY;
}

vx_aabb_t vx__triangle_aabb(vx_triangle_t* triangle)
{
    vx_aabb_t aabb;

    vx__aabb_init(&aabb);

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            aabb.max.v[i] = VX_MAX(aabb.max.v[i], triangle->vertices[j].v[i]);
            aabb.min.v[i] = VX_MIN(aabb.min.v[i], triangle->vertices[j].v[i]);
        }
    }

    return aabb;
}

vx_vertex_t vx__aabb_center(vx_aabb_t* a)
{
    vx_vertex_t boxcenter = a->min;
    vx__vec3_add(&boxcenter, &a->max);
    vx__vec3_multiply(&boxcenter, 0.5f);

    return boxcenter;
}

vx_vertex_t vx__aabb_half_size(vx_aabb_t* a)
{
    vx_vertex_t size;

    size.x = fabs(a->max.x - a->min.x) * 0.5f;
    size.y = fabs(a->max.y - a->min.y) * 0.5f;
    size.z = fabs(a->max.z - a->min.z) * 0.5f;

    return size;
}

vx_aabb_t vx__aabb_merge(vx_aabb_t* a, vx_aabb_t* b)
{
    vx_aabb_t merge;

    merge.min.x = VX_MIN(a->min.x, b->min.x);
    merge.min.y = VX_MIN(a->min.y, b->min.y);
    merge.min.z = VX_MIN(a->min.z, b->min.z);

    merge.max.x = VX_MAX(a->max.x, b->max.x);
    merge.max.y = VX_MAX(a->max.y, b->max.y);
    merge.max.z = VX_MAX(a->max.z, b->max.z);

    return merge;
}

size_t vx__vertex_hash(vx_vertex_t pos, size_t n)
{
    size_t a = (size_t)(pos.x * 73856093);
    size_t b = (size_t)(pos.y * 19349663);
    size_t c = (size_t)(pos.z * 83492791);

    return (a ^ b ^ c) % n;
}

void vx__add_voxel(vx_mesh_t* mesh,
    vx_vertex_t* pos,
    vx_color_t color,
    float* vertices)
{
    for (size_t i = 0; i < 8; ++i) {
        size_t index = i+mesh->nvertices;

        mesh->vertices[index].x = vertices[i*3+0] + pos->x;
        mesh->vertices[index].y = vertices[i*3+1] + pos->y;
        mesh->vertices[index].z = vertices[i*3+2] + pos->z;

        if (mesh->colors) {
            mesh->colors[index].r = color.r;
            mesh->colors[index].g = color.g;
            mesh->colors[index].b = color.b;
        }
    }

    int j = -1;
    for (size_t i = 0; i < VOXELIZER_INDICES_SIZE; ++i) {
        if (i % 6 == 0) {
            j++;
        }
        mesh->normalindices[i+mesh->nindices] = vx_normal_indices[j];
    }

    for (size_t i = 0; i < VOXELIZER_INDICES_SIZE; ++i) {
        mesh->indices[i+mesh->nindices] = vx_voxel_indices[i] + mesh->nvertices;
    }

    mesh->nindices += VOXELIZER_INDICES_SIZE;
    mesh->nvertices += 8;
}

vx_hash_table_t* vx__voxelize(vx_mesh_t const* m,
    vx_vertex_t vs,
    vx_vertex_t hvs,
    float precision,
    size_t* nvoxels)
{
    vx_hash_table_t* table = NULL;

    table = vx__hash_table_alloc(VOXELIZER_HASH_TABLE_SIZE);

    for (size_t i = 0; i < m->nindices; i += 3) {
        vx_triangle_t triangle;
        unsigned int i1, i2, i3;

        VX_ASSERT(m->indices[i+0] < m->nvertices);
        VX_ASSERT(m->indices[i+1] < m->nvertices);
        VX_ASSERT(m->indices[i+2] < m->nvertices);

        i1 = m->indices[i+0];
        i2 = m->indices[i+1];
        i3 = m->indices[i+2];

        triangle.p1 = m->vertices[i1];
        triangle.p2 = m->vertices[i2];
        triangle.p3 = m->vertices[i3];

        if (m->colors) {
            triangle.colors[0] = m->colors[i1];
            triangle.colors[1] = m->colors[i2];
            triangle.colors[2] = m->colors[i3];
        }

        if (vx__triangle_area(&triangle) < VOXELIZER_EPSILON) {
            // triangle with 0 area
            continue;
        }

        vx_aabb_t aabb = vx__triangle_aabb(&triangle);

        aabb.min.x = vx__map_to_voxel(aabb.min.x, vs.x, true);
        aabb.min.y = vx__map_to_voxel(aabb.min.y, vs.y, true);
        aabb.min.z = vx__map_to_voxel(aabb.min.z, vs.z, true);

        aabb.max.x = vx__map_to_voxel(aabb.max.x, vs.x, false);
        aabb.max.y = vx__map_to_voxel(aabb.max.y, vs.y, false);
        aabb.max.z = vx__map_to_voxel(aabb.max.z, vs.z, false);

        for (float x = aabb.min.x; x <= aabb.max.x; x += vs.x) {
            for (float y = aabb.min.y; y <= aabb.max.y; y += vs.y) {
                for (float z = aabb.min.z; z <= aabb.max.z; z += vs.z) {
                    vx_aabb_t saabb;

                    saabb.min.x = x - hvs.x;
                    saabb.min.y = y - hvs.y;
                    saabb.min.z = z - hvs.z;
                    saabb.max.x = x + hvs.x;
                    saabb.max.y = y + hvs.y;
                    saabb.max.z = z + hvs.z;

                    vx_vertex_t boxcenter = vx__aabb_center(&saabb);
                    vx_vertex_t halfsize = vx__aabb_half_size(&saabb);

                    // HACK: some holes might appear, this
                    // precision factor reduces the artifact
                    halfsize.x += precision;
                    halfsize.y += precision;
                    halfsize.z += precision;

                    if (vx__triangle_box_overlap(boxcenter, halfsize, triangle)) {
                        vx_vec3_t v1, v2, v3;
                        vx_color_t c1, c2, c3;
                        vx_voxel_data_t* nodedata;
                        float a1, a2, a3;
                        float area;

                        nodedata = VX_MALLOC(vx_voxel_data_t, 1);

                        if (m->colors != NULL) {
                            // Perform barycentric interpolation of colors
                            v1 = triangle.p1;
                            v2 = triangle.p2;
                            v3 = triangle.p3;

                            c1 = triangle.colors[0];
                            c2 = triangle.colors[1];
                            c3 = triangle.colors[2];

                            vx_triangle_t t1 = {{{v1, v2, boxcenter}}, {{{{0.0f, 0.0f, 0.0f}}}}};
                            vx_triangle_t t2 = {{{v2, v3, boxcenter}}, {{{{0.0f, 0.0f, 0.0f}}}}};
                            vx_triangle_t t3 = {{{v3, v1, boxcenter}}, {{{{0.0f, 0.0f, 0.0f}}}}};

                            a1 = vx__triangle_area(&t1);
                            a2 = vx__triangle_area(&t2);
                            a3 = vx__triangle_area(&t3);

                            area = a1 + a2 + a3;

                            vx__vec3_multiply(&c1, a2 / area);
                            vx__vec3_multiply(&c2, a3 / area);
                            vx__vec3_multiply(&c3, a1 / area);

                            vx__vec3_add(&c1, &c2);
                            vx__vec3_add(&c1, &c3);

                            nodedata->color = c1;
                        }

                        nodedata->position = boxcenter;

                        size_t hash = vx__vertex_hash(boxcenter, VOXELIZER_HASH_TABLE_SIZE);

                        bool insert = vx__hash_table_insert(table, hash, nodedata,
                                vx__vertex_comp_func);

                        if (insert) {
                            (*nvoxels)++;
                        }
                   }
                }
            }
        }
    }

    return table;
}

vx_mesh_t* vx_voxelize(vx_mesh_t const* m,
    float voxelsizex,
    float voxelsizey,
    float voxelsizez,
    float precision)
{
    vx_mesh_t* outmesh = NULL;
    vx_hash_table_t* table = NULL;
    size_t voxels = 0;

    vx_vertex_t vs = {{{voxelsizex, voxelsizey, voxelsizez}}};
    vx_vertex_t hvs = vs;

    vx__vec3_multiply(&hvs, 0.5f);

    table = vx__voxelize(m, vs, hvs, precision, &voxels);

    outmesh = VX_MALLOC(vx_mesh_t, 1);
    size_t nvertices = voxels * 8;
    size_t nindices = voxels * VOXELIZER_INDICES_SIZE;
    outmesh->nnormals = VOXELIZER_NORMAL_INDICES_SIZE;
    outmesh->vertices = VX_CALLOC(vx_vertex_t, nvertices);
    outmesh->normals = VX_CALLOC(vx_vec3_t, 6);
    outmesh->colors = m->colors != NULL ? VX_CALLOC(vx_color_t, nvertices) : NULL;
    outmesh->indices = VX_CALLOC(unsigned int, nindices);
    outmesh->normalindices = VX_CALLOC(unsigned int, nindices);
    outmesh->nindices = 0;
    outmesh->nvertices = 0;

    memcpy(outmesh->normals, vx_normals, 18 * sizeof(float));

    float vertices[24] = {
        -hvs.x,  hvs.y,  hvs.z,
        -hvs.x, -hvs.y,  hvs.z,
         hvs.x, -hvs.y,  hvs.z,
         hvs.x,  hvs.y,  hvs.z,
        -hvs.x,  hvs.y, -hvs.z,
        -hvs.x, -hvs.y, -hvs.z,
         hvs.x, -hvs.y, -hvs.z,
         hvs.x,  hvs.y, -hvs.z,
    };

    for (size_t i = 0; i < table->size; ++i) {
        if (table->elements[i] != NULL) {
            vx_hash_table_node_t* node = table->elements[i];
            vx_voxel_data_t* voxeldata;

            while (node) {
                voxeldata = (vx_voxel_data_t*) node->data;
                vx__add_voxel(outmesh, &voxeldata->position, voxeldata->color, vertices);
                node = node->next;
            }
        }
    }

    vx__hash_table_free(table);

    return outmesh;
}

vx_point_cloud_t* vx_voxelize_pc(vx_mesh_t const* mesh,
    float voxelsizex,
    float voxelsizey,
    float voxelsizez,
    float precision)
{
    vx_point_cloud_t* pc = NULL;
    vx_hash_table_t* table = NULL;
    size_t voxels = 0;

    vx_vec3_t vs = {{{voxelsizex, voxelsizey, voxelsizez}}};
    vx_vec3_t hvs = vs;

    vx__vec3_multiply(&hvs, 0.5f);

    table = vx__voxelize(mesh, vs, hvs, precision, &voxels);

    pc = VX_MALLOC(vx_point_cloud_t, 1);
    pc->vertices = VX_MALLOC(vx_vec3_t, voxels);
    pc->colors = mesh->colors != NULL ? VX_MALLOC(vx_color_t, voxels) : NULL;
    pc->nvertices = 0;

    for (size_t i = 0; i < table->size; ++i) {
        if (!table->elements[i]) { continue; }

        vx_hash_table_node_t* node = table->elements[i];
        vx_voxel_data_t* voxeldata;

        while (node) {
            voxeldata = (vx_voxel_data_t*) node->data;
            if (pc->colors) { pc->colors[pc->nvertices] = voxeldata->color; }
            pc->vertices[pc->nvertices++] = voxeldata->position;

            node = node->next;
        }
    }

    vx__hash_table_free(table);
    return pc;
}

unsigned int vx__rgbaf32_to_abgr8888(float rgba[4])
{
    unsigned int color =
       (((unsigned int)(255.0f * rgba[3]) & 0xff) << 24) |
       (((unsigned int)(255.0f * rgba[2]) & 0xff) << 16) |
       (((unsigned int)(255.0f * rgba[1]) & 0xff) <<  8) |
       (((unsigned int)(255.0f * rgba[0]) & 0xff) <<  0);
    return color;
}

void vx__abgr8888_to_rgbaf32(unsigned int abgr8888,
    float (*rgbaf32)[4])
{
    (*rgbaf32)[0] = ((abgr8888 >>  0) & 0xff) / 255.0f;
    (*rgbaf32)[1] = ((abgr8888 >>  8) & 0xff) / 255.0f;
    (*rgbaf32)[2] = ((abgr8888 >> 16) & 0xff) / 255.0f;
    (*rgbaf32)[3] = ((abgr8888 >> 24) & 0xff) / 255.0f;
}

unsigned int vx__mix(unsigned int abgr88880,
    unsigned int abgr88881)
{
    float rgba0[4];
    float rgba1[4];
    float out[4];

    vx__abgr8888_to_rgbaf32(abgr88880, &rgba0);
    vx__abgr8888_to_rgbaf32(abgr88881, &rgba1);

    for (int i = 0; i < 4; ++i) {
        out[i] = rgba0[i] * 0.5f + rgba1[i] * 0.5f;
    }

    return vx__rgbaf32_to_abgr8888(out);
}

unsigned int* vx_voxelize_snap_3dgrid(vx_mesh_t const* m,
    unsigned int width,
    unsigned int height,
    unsigned int depth)
{
    vx_aabb_t* aabb = NULL;
    vx_aabb_t* meshaabb = NULL;
    float ax, ay, az;

    VX_ASSERT(m->colors);

    for (size_t i = 0; i < m->nindices; i += 3) {
        vx_triangle_t triangle;
        unsigned int i1, i2, i3;

        VX_ASSERT(m->indices[i+0] < m->nvertices);
        VX_ASSERT(m->indices[i+1] < m->nvertices);
        VX_ASSERT(m->indices[i+2] < m->nvertices);

        i1 = m->indices[i+0];
        i2 = m->indices[i+1];
        i3 = m->indices[i+2];

        triangle.p1 = m->vertices[i1];
        triangle.p2 = m->vertices[i2];
        triangle.p3 = m->vertices[i3];

        if (!meshaabb) {
            meshaabb = VX_MALLOC(vx_aabb_t, 1);
            *meshaabb = vx__triangle_aabb(&triangle);
        } else {
            vx_aabb_t naabb = vx__triangle_aabb(&triangle);
            *meshaabb = vx__aabb_merge(meshaabb, &naabb);
        }
    }

    float resx = (meshaabb->max.x - meshaabb->min.x) / width;
    float resy = (meshaabb->max.y - meshaabb->min.y) / height;
    float resz = (meshaabb->max.z - meshaabb->min.z) / depth;

    vx_point_cloud_t* pc = vx_voxelize_pc(m, resx, resy, resz, 0.0);

    aabb = VX_MALLOC(vx_aabb_t, 1);

    vx__aabb_init(aabb);

    for (size_t i = 0; i < pc->nvertices; i++) {
        for (size_t j = 0; j < 3; j++) {
            aabb->max.v[j] = VX_MAX(aabb->max.v[j], pc->vertices[i].v[j]);
            aabb->min.v[j] = VX_MIN(aabb->min.v[j], pc->vertices[i].v[j]);
        }
    }

    ax = aabb->max.x - aabb->min.x;
    ay = aabb->max.y - aabb->min.y;
    az = aabb->max.z - aabb->min.z;

    unsigned int* data = VX_CALLOC(unsigned int, width * height * depth);

    for (size_t i = 0; i < pc->nvertices; ++i) {
        float rgba[4] = {pc->colors[i].r, pc->colors[i].g, pc->colors[i].b, 1.0};
        unsigned int color;
        float ox, oy, oz;
        int ix, iy, iz;
        unsigned int index;

        ox = pc->vertices[i].x + fabs(aabb->min.x);
        oy = pc->vertices[i].y + fabs(aabb->min.y);
        oz = pc->vertices[i].z + fabs(aabb->min.z);

        VX_ASSERT(ox >= 0.f);
        VX_ASSERT(oy >= 0.f);
        VX_ASSERT(oz >= 0.f);

        ix = (ax == 0.0) ? 0 : (ox / ax) * (width - 1);
        iy = (ay == 0.0) ? 0 : (oy / ay) * (height - 1);
        iz = (az == 0.0) ? 0 : (oz / az) * (depth - 1);


        VX_ASSERT(ix >= 0);
        VX_ASSERT(iy >= 0);
        VX_ASSERT(iz >= 0);

        VX_ASSERT(ix + iy * width + iz * (width * height) < width * height * depth);

        color = vx__rgbaf32_to_abgr8888(rgba);
        index = ix + iy * width + iz * (width * height);

        if (data[index] != 0) {
            data[index] = vx__mix(color, data[index]);
        } else {
            data[index] = color;
        }
    }

    VX_FREE(aabb);
    VX_FREE(meshaabb);
    vx_point_cloud_free(pc);

    return data;
}

#undef VOXELIZER_EPSILON
#undef VOXELIZER_INDICES_SIZE
#undef VOXELIZER_HASH_TABLE_SIZE

#endif // VX_VOXELIZER_IMPLEMENTATION
