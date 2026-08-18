#include "camera.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void camera_init(Camera* cam, float fov_degrees, float aspect, float near_plane, float far_plane) {
    cam->position = (Vec3){ 0.0f, 0.0f, 3.0f };
    cam->target   = (Vec3){ 0.0f, 0.0f, 0.0f };
    cam->up       = (Vec3){ 0.0f, 1.0f, 0.0f };
    cam->fov = fov_degrees;
    cam->aspect = aspect;
    cam->near_plane = near_plane;
    cam->far_plane = far_plane;
    cam->dirty = 1;
}

void camera_set_position(Camera* cam, float x, float y, float z) {
    cam->position = (Vec3){ x, y, z };
    cam->dirty = 1;
}

void camera_set_target(Camera* cam, float x, float y, float z) {
    cam->target = (Vec3){ x, y, z };
    cam->dirty = 1;
}

void camera_set_up(Camera* cam, float x, float y, float z) {
    cam->up = (Vec3){ x, y, z };
    cam->dirty = 1;
}

void camera_update(Camera* cam) {
    if (!cam->dirty) return;
    mat4_perspective(cam->projection_matrix, cam->fov * (float)M_PI / 180.0f,
                      cam->aspect, cam->near_plane, cam->far_plane);
    mat4_look_at(cam->view_matrix, cam->position, cam->target, cam->up);
    cam->dirty = 0;
}

const float* camera_get_view(Camera* cam) {
    return cam->view_matrix;
}

const float* camera_get_projection(Camera* cam) {
    return cam->projection_matrix;
}