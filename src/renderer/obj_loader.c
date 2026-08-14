#include "obj_loader.h"
#include "../math/math3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float u, v; } UV;

typedef struct {
    Vec3* data;
    int count;
    int capacity;
} Vec3Array;

typedef struct {
    UV* data;
    int count;
    int capacity;
} UVArray;

typedef struct {
    RHIVertex* data;
    int count;
    int capacity;
} VertexArray;

static void vec3_array_push(Vec3Array* arr, Vec3 v) {
    if (arr->count == arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->data = realloc(arr->data, arr->capacity * sizeof(Vec3));
    }
    arr->data[arr->count++] = v;
}

static void uv_array_push(UVArray* arr, UV v) {
    if (arr->count == arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->data = realloc(arr->data, arr->capacity * sizeof(UV));
    }
    arr->data[arr->count++] = v;
}

static void vertex_array_push(VertexArray* arr, RHIVertex v) {
    if (arr->count == arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->data = realloc(arr->data, arr->capacity * sizeof(RHIVertex));
    }
    arr->data[arr->count++] = v;
}

RHIMesh* obj_load_mesh(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        printf("Failed to open OBJ file: %s\n", filepath);
        return NULL;
    }

    Vec3Array positions = {0};
    Vec3Array normals = {0};
    UVArray uvs = {0};

    VertexArray vertices = {0};

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            Vec3 p;
            sscanf(line, "v %f %f %f", &p.x, &p.y, &p.z);
            vec3_array_push(&positions, p);
        } else if (line[0] == 'v' && line[1] == 't') {
            UV uv;
            sscanf(line, "vt %f %f", &uv.u, &uv.v);
            uv_array_push(&uvs, uv);
        } else if (line[0] == 'v' && line[1] == 'n') {
            Vec3 n;
            sscanf(line, "vn %f %f %f", &n.x, &n.y, &n.z);
            vec3_array_push(&normals, n);
        } else if (line[0] == 'f' && line[1] == ' ') {
            int vi[3], ti[3], ni[3];
            int matched = sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
                &vi[0], &ti[0], &ni[0],
                &vi[1], &ti[1], &ni[1],
                &vi[2], &ti[2], &ni[2]);

            if (matched != 9) {
                printf("Skipping unsupported face line (need triangulated v/vt/vn): %s", line);
                continue;
            }

            // OBJ indices are 1-based, so subtract 1 for our 0-based arrays.
            for (int i = 0; i < 3; i++) {
                Vec3 pos = positions.data[vi[i] - 1];
                Vec3 nrm = normals.data[ni[i] - 1];
                UV uv = uvs.data[ti[i] - 1];

                RHIVertex vert;
                vert.x = pos.x; vert.y = pos.y; vert.z = pos.z;
                vert.nx = nrm.x; vert.ny = nrm.y; vert.nz = nrm.z;
                vert.u = uv.u; vert.v = uv.v;
                vertex_array_push(&vertices, vert);
            }
        }
    }

    fclose(file);

    RHIBuffer* vbo = rhi_buffer_create(vertices.data, vertices.count);
    RHIMesh* mesh = rhi_mesh_create(vbo, NULL); // NULL: no index buffer, draw as plain triangle list

    // printf("OBJ loaded: %s (%d vertices)\n", filepath, vertices.count);

    // Free our temporary CPU-side arrays - the GPU has its own copy now via rhi_buffer_create.
    free(positions.data);
    free(normals.data);
    free(uvs.data);
    free(vertices.data);

    return mesh;
}