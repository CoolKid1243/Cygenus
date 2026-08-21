#include "math3d.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __cplusplus
extern "C" {
#endif

Vec3 vec3_sub(Vec3 a, Vec3 b) {
    Vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    Vec3 r = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return r;
}

float vec3_length(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 vec3_normalize(Vec3 v) {
    float len = vec3_length(v);
    if (len < 0.00001f) return v; // avoid divide by zero on degenerate input
    Vec3 r = { v.x / len, v.y / len, v.z / len };
    return r;
}

void mat4_identity(float* out) {
    for (int i = 0; i < 16; i++) out[i] = 0.0f;
    out[0] = out[5] = out[10] = out[15] = 1.0f;
}

// Standard perspective projection matrix
void mat4_perspective(float* out, float fov_radians, float aspect, float near_plane, float far_plane) {
    float tan_half_fov = tanf(fov_radians / 2.0f);
    mat4_identity(out);
    out[0]  = 1.0f / (aspect * tan_half_fov);
    out[5]  = 1.0f / tan_half_fov;
    out[10] = -(far_plane + near_plane) / (far_plane - near_plane);
    out[11] = -1.0f;
    out[14] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);
    out[15] = 0.0f;
}

// Builds a view matrix that transforms world space into camera space.
void mat4_look_at(float* out, Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 forward = vec3_normalize(vec3_sub(target, eye));
    Vec3 right = vec3_normalize(vec3_cross(forward, up));
    Vec3 real_up = vec3_cross(right, forward);

    out[0] = right.x;      out[4] = right.y;      out[8]  = right.z;      out[12] = 0.0f;
    out[1] = real_up.x;    out[5] = real_up.y;    out[9]  = real_up.z;    out[13] = 0.0f;
    out[2] = -forward.x;   out[6] = -forward.y;   out[10] = -forward.z;   out[14] = 0.0f;
    out[3] = 0.0f;         out[7] = 0.0f;         out[11] = 0.0f;         out[15] = 1.0f;

    out[12] = -(right.x * eye.x   + right.y * eye.y   + right.z * eye.z);
    out[13] = -(real_up.x * eye.x + real_up.y * eye.y + real_up.z * eye.z);
    out[14] =  (forward.x * eye.x + forward.y * eye.y + forward.z * eye.z);
}

void mat4_translate(float* out, float x, float y, float z) {
    mat4_identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

// Column-major matrix multiply
void mat4_multiply(float* out, const float* a, const float* b) {
    float result[16];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a[k * 4 + row] * b[col * 4 + k];
            }
            result[col * 4 + row] = sum;
        }
    }
    memcpy(out, result, sizeof(result));
}

void mat4_scale(float* out, float sx, float sy, float sz) {
    mat4_identity(out);
    out[0] = sx;
    out[5] = sy;
    out[10] = sz;
}

void mat4_rotate_x(float* out, float radians) {
    mat4_identity(out);
    float c = cosf(radians), s = sinf(radians);
    out[5] = c;  out[6] = s;
    out[9] = -s; out[10] = c;
}

void mat4_rotate_y(float* out, float radians) {
    mat4_identity(out);
    float c = cosf(radians), s = sinf(radians);
    out[0] = c; out[2] = -s;
    out[8] = s; out[10] = c;
}

void mat4_rotate_z(float* out, float radians) {
    mat4_identity(out);
    float c = cosf(radians), s = sinf(radians);
    out[0] = c;  out[1] = s;
    out[4] = -s; out[5] = c;
}

void mat4_compose_trs(float* out, Vec3 position, Vec3 rotation_degrees, Vec3 scale) {
    float rx[16], ry[16], rz[16], s[16], t[16];
    float temp[16], rot[16];

    mat4_rotate_x(rx, rotation_degrees.x * (float)M_PI / 180.0f);
    mat4_rotate_y(ry, rotation_degrees.y * (float)M_PI / 180.0f);
    mat4_rotate_z(rz, rotation_degrees.z * (float)M_PI / 180.0f);
    mat4_scale(s, scale.x, scale.y, scale.z);
    mat4_translate(t, position.x, position.y, position.z);

    // Combine rotations: apply X first, then Y, then Z (Z * Y * X, read right to left).
    mat4_multiply(temp, ry, rx);  // temp = Ry * Rx
    mat4_multiply(rot, rz, temp); // rot  = Rz * Ry * Rx

    // Full model matrix: scale happens first, then rotate, then move into place.
    mat4_multiply(temp, rot, s);  // temp = R * S
    mat4_multiply(out, t, temp);  // out  = T * R * S
}

#ifdef __cplusplus
}
#endif