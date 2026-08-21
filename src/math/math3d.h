#ifndef MATH3D_H
#define MATH3D_H

#ifdef __cplusplus
extern "C" {
#endif

// All matrices are column-major float[16]
typedef struct { float x, y, z; } Vec3;

// Vector opperations
Vec3 vec3_sub(Vec3 a, Vec3 b);
Vec3 vec3_cross(Vec3 a, Vec3 b);
Vec3 vec3_normalize(Vec3 v);
float vec3_length(Vec3 v);

// Matrix builders
void mat4_identity(float* out);
void mat4_perspective(float* out, float fov_radians, float aspect, float near_plane, float far_plane);
void mat4_look_at(float* out, Vec3 eye, Vec3 target, Vec3 up);
void mat4_translate(float* out, float x, float y, float z);
void mat4_multiply(float* out, const float* a, const float* b);

void mat4_scale(float* out, float sx, float sy, float sz);
void mat4_rotate_x(float* out, float radians);
void mat4_rotate_y(float* out, float radians);
void mat4_rotate_z(float* out, float radians);

void mat4_compose_trs(float* out, Vec3 position, Vec3 rotation_degrees, Vec3 scale);

#ifdef __cplusplus
}
#endif

#endif // MATH3D_H