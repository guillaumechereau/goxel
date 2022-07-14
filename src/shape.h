/*
 * Define the 3d shape we can use for the mesh operations.
 *
 * TODO: need to explain how that works.
 */

#ifndef SHAPE_H
#define SHAPE_H

typedef struct shape {
    const char *id;
    float (*func)(const float p[3], const float s[3], float smoothness);
} shape_t;

void shapes_init(void);
extern shape_t shape_sphere;
extern shape_t shape_cube;
extern shape_t shape_cylinder;

#endif // SHAPE_H
