#include "obj_loader.h"
#include "../math/math3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "ufbx.h"

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

// Case-insensitive check for whether `path` ends with `suffix` (e.g. ".fbx"),
// since some exporters/OSes produce uppercase extensions and strcasecmp
// isn't portable to MSVC.
static int ends_with_ci(const char* path, const char* suffix) {
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);
    if (path_len < suffix_len) return 0;
    const char* tail = path + (path_len - suffix_len);
    for (size_t i = 0; i < suffix_len; i++) {
        if (tolower((unsigned char)tail[i]) != tolower((unsigned char)suffix[i])) return 0;
    }
    return 1;
}

static RHIMesh* load_obj_mesh(const char* filepath) {
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
            // Read however many v/vt/vn groups are on this line - a
            // triangle has 3, but Blender's default OBJ export writes
            // quads (4), and OBJ technically allows arbitrary n-gons.
            int vi[32], ti[32], ni[32];
            int n = 0;

            char buf[256];
            snprintf(buf, sizeof(buf), "%s", line + 2); // skip "f "

            char* token = strtok(buf, " \t\r\n");
            while (token && n < 32) {
                int a, b, c;
                if (sscanf(token, "%d/%d/%d", &a, &b, &c) == 3) {
                    vi[n] = a; ti[n] = b; ni[n] = c;
                    n++;
                }
                token = strtok(NULL, " \t\r\n");
            }

            if (n < 3) {
                printf("Skipping unsupported face line (need at least a triangle with v/vt/vn): %s", line);
                continue;
            }

            for (int i = 1; i < n - 1; i++) {
                int idx[3] = { 0, i, i + 1 };
                for (int k = 0; k < 3; k++) {
                    int j = idx[k];
                    Vec3 pos = positions.data[vi[j] - 1];
                    Vec3 nrm = normals.data[ni[j] - 1];
                    UV uv = uvs.data[ti[j] - 1];

                    RHIVertex vert;
                    vert.x = pos.x; vert.y = pos.y; vert.z = pos.z;
                    vert.nx = nrm.x; vert.ny = nrm.y; vert.nz = nrm.z;
                    vert.u = uv.u; vert.v = uv.v;
                    vertex_array_push(&vertices, vert);
                }
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

static RHIMesh* load_fbx_mesh(const char* filepath) {
    ufbx_load_opts opts = {0};
    // Normalize every file to the same axes/scale our engine expects,
    // regardless of which axis convention the source DCC tool (Blender,
    // Maya, 3ds Max, etc.) exported with - otherwise an FBX authored in a
    // Z-up tool would import lying on its side.
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;
    opts.generate_missing_normals = true;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(filepath, &opts, &error);
    if (!scene) {
        printf("Failed to load FBX file: %s (%s)\n", filepath, error.description.data);
        return NULL;
    }

    if (scene->meshes.count == 0) {
        printf("FBX file has no meshes: %s\n", filepath);
        ufbx_free_scene(scene);
        return NULL;
    }

    // NOTE: only supports a single mesh per file, same as load_obj_mesh - if
    // the FBX has multiple meshes/objects, only the first is loaded, and no
    // materials, skeletons, or animation are read yet either.
    ufbx_mesh* mesh = scene->meshes.data[0];

    ufbx_node* node = mesh->instances.count > 0 ? mesh->instances.data[0] : scene->root_node;
    ufbx_matrix to_world = node->geometry_to_world;
    ufbx_matrix normal_matrix = ufbx_matrix_for_normals(&to_world);

    int flip_winding = ufbx_matrix_determinant(&to_world) < 0.0;
    static const int vertex_order[3]  = {0, 1, 2};
    static const int flipped_order[3] = {0, 2, 1};
    const int* order = flip_winding ? flipped_order : vertex_order;

    VertexArray vertices = {0};
    uint32_t* tri_indices = malloc(mesh->max_face_triangles * 3 * sizeof(uint32_t));

    for (size_t f = 0; f < mesh->faces.count; f++) {
        ufbx_face face = mesh->faces.data[f];
        uint32_t num_triangles = ufbx_triangulate_face(tri_indices, mesh->max_face_triangles * 3, mesh, face);
        uint32_t num_tri_indices = num_triangles * 3;

        for (uint32_t t = 0; t < num_tri_indices; t += 3) {
            for (int k = 0; k < 3; k++) {
                uint32_t index = tri_indices[t + order[k]];

                ufbx_vec3 local_pos = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
                ufbx_vec3 local_nrm = mesh->vertex_normal.exists
                    ? ufbx_get_vertex_vec3(&mesh->vertex_normal, index)
                    : (ufbx_vec3){0, 1, 0};
                ufbx_vec2 uv = mesh->vertex_uv.exists
                    ? ufbx_get_vertex_vec2(&mesh->vertex_uv, index)
                    : (ufbx_vec2){0, 0};

                ufbx_vec3 pos = ufbx_transform_position(&to_world, local_pos);
                ufbx_vec3 nrm = ufbx_transform_direction(&normal_matrix, local_nrm);
                double nrm_len = sqrt(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
                if (nrm_len > 1e-8) {
                    nrm.x /= nrm_len; nrm.y /= nrm_len; nrm.z /= nrm_len;
                }

                RHIVertex vert;
                vert.x = (float)pos.x;  vert.y = (float)pos.y;  vert.z = (float)pos.z;
                vert.nx = (float)nrm.x; vert.ny = (float)nrm.y; vert.nz = (float)nrm.z;
                vert.u = (float)uv.x;   vert.v = (float)uv.y;
                vertex_array_push(&vertices, vert);
            }
        }
    }

    free(tri_indices);
    ufbx_free_scene(scene);

    RHIBuffer* vbo = rhi_buffer_create(vertices.data, vertices.count);
    RHIMesh* result = rhi_mesh_create(vbo, NULL); // NULL: no index buffer, draw as plain triangle list (matches load_obj_mesh)

    free(vertices.data);
    return result;
}

RHIMesh* obj_load_mesh(const char* filepath) {
    if (ends_with_ci(filepath, ".fbx")) {
        return load_fbx_mesh(filepath);
    }
    return load_obj_mesh(filepath);
}