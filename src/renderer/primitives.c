#include "primitives.h"
#include "renderer/rhi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

RHIMesh* primitive_create_cube(void) {
    static RHIVertex vertices[] = {
        // Front face (z = 0.5)
        {-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f},
        { 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f},
        { 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f},
        {-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f},
        // Back face (z = -0.5)
        {-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f},
        {-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f},
        { 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f},
        { 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f},
        // Left face (x = -0.5)
        {-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f},
        {-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f},
        {-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f},
        {-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f},
        // Right face (x = 0.5)
        { 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f},
        { 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f},
        { 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f},
        { 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f},
        // Top face (y = 0.5)
        {-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f},
        {-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f},
        { 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f},
        { 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f},
        // Bottom face (y = -0.5)
        {-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f},
        { 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f},
        { 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f},
        {-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f},
    };

    static uint32_t indices[] = {
        0,1,2, 0,2,3,  4,5,6, 4,6,7,  8,9,10, 8,10,11,
        12,13,14, 12,14,15,  16,17,18, 16,18,19,  20,21,22, 20,22,23
    };

    RHIBuffer* vbo = rhi_buffer_create(vertices, sizeof(vertices)/sizeof(vertices[0]));
    RHIBuffer* ibo = rhi_index_buffer_create(indices, sizeof(indices)/sizeof(indices[0]));
    return rhi_mesh_create(vbo, ibo);
}

RHIMesh* primitive_create_plane(float width, float depth, int resolution) {
    if (resolution < 1) resolution = 1;
    int verts_per_row = resolution + 1;
    int vertex_count = verts_per_row * verts_per_row;
    int quad_count = resolution * resolution;
    int index_count = quad_count * 6;

    RHIVertex* vertices = malloc(vertex_count * sizeof(RHIVertex));
    uint32_t* indices = malloc(index_count * sizeof(uint32_t));

    int v_idx = 0;
    for (int row = 0; row <= resolution; row++) {
        for (int col = 0; col <= resolution; col++) {
            float u = (float)col / resolution;
            float v = (float)row / resolution;
            float x = (u - 0.5f) * width;
            float z = (v - 0.5f) * depth;
            RHIVertex vert;
            vert.x = x;
            vert.y = 0.0f;
            vert.z = z;
            vert.nx = 0.0f;
            vert.ny = 1.0f;
            vert.nz = 0.0f;
            vert.u = u;
            vert.v = v;
            vertices[v_idx++] = vert;
        }
    }

    int i_idx = 0;
    for (int row = 0; row < resolution; row++) {
        for (int col = 0; col < resolution; col++) {
            int top_left = row * verts_per_row + col;
            int top_right = row * verts_per_row + (col + 1);
            int bottom_left = (row + 1) * verts_per_row + col;
            int bottom_right = (row + 1) * verts_per_row + (col + 1);
            // Triangle 1: bottom_left, bottom_right, top_right
            indices[i_idx++] = bottom_left;
            indices[i_idx++] = bottom_right;
            indices[i_idx++] = top_right;
            // Triangle 2: bottom_left, top_right, top_left
            indices[i_idx++] = bottom_left;
            indices[i_idx++] = top_right;
            indices[i_idx++] = top_left;
        }
    }

    RHIBuffer* vbo = rhi_buffer_create(vertices, vertex_count);
    RHIBuffer* ibo = rhi_index_buffer_create(indices, index_count);
    RHIMesh* mesh = rhi_mesh_create(vbo, ibo);

    free(vertices);
    free(indices);
    return mesh;
}

RHIMesh* primitive_create_sphere(float radius, int segments, int rings) {
    if (segments < 3) segments = 3; // longitude slices (around the equator)
    if (rings < 2) rings = 2;       // latitude bands (pole to pole)

    int vertex_count = (rings + 1) * (segments + 1);
    int quad_count = rings * segments;
    int index_count = quad_count * 6;

    RHIVertex* vertices = malloc(vertex_count * sizeof(RHIVertex));
    uint32_t* indices = malloc(index_count * sizeof(uint32_t));

    // Build vertices row by row, from the top pole (ring 0) to the bottom pole
    int v_idx = 0;
    for (int ring = 0; ring <= rings; ring++) {
        float v = (float)ring / (float)rings;
        float phi = v * (float)M_PI;
        float sin_phi = sinf(phi);
        float cos_phi = cosf(phi);

        for (int seg = 0; seg <= segments; seg++) {
            // theta: sweeps a full circle around the Y axis
            float u = (float)seg / (float)segments;
            float theta = u * 2.0f * (float)M_PI;
            float sin_theta = sinf(theta);
            float cos_theta = cosf(theta);

            float nx = sin_phi * cos_theta;
            float ny = cos_phi;
            float nz = sin_phi * sin_theta;

            RHIVertex vert;
            vert.x = radius * nx;
            vert.y = radius * ny;
            vert.z = radius * nz;
            vert.nx = nx;
            vert.ny = ny;
            vert.nz = nz;
            vert.u = u;
            vert.v = v;
            vertices[v_idx++] = vert;
        }
    }

    int verts_per_row = segments + 1;
    int i_idx = 0;
    for (int ring = 0; ring < rings; ring++) {
        for (int seg = 0; seg < segments; seg++) {
            int top_left = ring * verts_per_row + seg;
            int top_right = ring * verts_per_row + (seg + 1);
            int bottom_left = (ring + 1) * verts_per_row + seg;
            int bottom_right = (ring + 1) * verts_per_row + (seg + 1);

            indices[i_idx++] = top_left;
            indices[i_idx++] = top_right;
            indices[i_idx++] = bottom_right;

            indices[i_idx++] = top_left;
            indices[i_idx++] = bottom_right;
            indices[i_idx++] = bottom_left;
        }
    }

    RHIBuffer* vbo = rhi_buffer_create(vertices, vertex_count);
    RHIBuffer* ibo = rhi_index_buffer_create(indices, index_count);
    RHIMesh* mesh = rhi_mesh_create(vbo, ibo);

    free(vertices);
    free(indices);
    return mesh;
}
