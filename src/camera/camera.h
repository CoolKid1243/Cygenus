#ifndef CAMERA_H
#define CAMERA_H

#include "../math/math3d.h"

typedef struct {
    Vec3 position;
    Vec3 target;
    Vec3 up;
    float fov;
    float aspect;
    float near_plane;
    float far_plane;
    float view_matrix[16];
    float projection_matrix[16];
    int dirty; // set when position/target/etc change, cleared after camera_update
} Camera;

void camera_init(Camera* cam, float fov_degrees, float aspect, float near_plane, float far_plane);
void camera_set_position(Camera* cam, float x, float y, float z);
void camera_set_target(Camera* cam, float x, float y, float z);
void camera_set_up(Camera* cam, float x, float y, float z);
void camera_update(Camera* cam); // recalculates matrices if dirty
const float* camera_get_view(Camera* cam);
const float* camera_get_projection(Camera* cam);

#endif