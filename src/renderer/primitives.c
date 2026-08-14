#include "primitives.h"
#include "renderer/rhi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
    // Not implemented yet – returns a cube placeholder
    // (you can implement later)
    return primitive_create_cube();
}