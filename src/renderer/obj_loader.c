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

// Triangulates every face of a ufbx mesh and bakes `to_local` (whatever
// space the caller wants the vertices in - either straight to world space
// for the single-mesh loader, or just the mesh's own node-local pivot for
// the multi-mesh importer, which leaves scene placement to the ECS Transform
// instead) into position/normal data. Winding is reversed if `to_local`
// mirrors the mesh (negative determinant), since transforming positions
// through a mirroring matrix alone flips the mesh inside-out relative to its
// normals otherwise.
static RHIMesh* build_rhi_mesh_from_ufbx_mesh(ufbx_mesh* mesh, const ufbx_matrix* to_local) {
    ufbx_matrix normal_matrix = ufbx_matrix_for_normals(to_local);
    int flip_winding = ufbx_matrix_determinant(to_local) < 0.0;
    static const int vertex_order[3]  = {0, 1, 2};
    static const int flipped_order[3] = {0, 2, 1};
    const int* order = flip_winding ? flipped_order : vertex_order;

    VertexArray vertices = {0};
    uint32_t* tri_indices = malloc(mesh->max_face_triangles * 3 * sizeof(uint32_t));

    for (size_t f = 0; f < mesh->faces.count; f++) {
        ufbx_face face = mesh->faces.data[f];
        // ufbx_triangulate_face() returns the number of TRIANGLES written,
        // not the number of indices - each triangle is 3 indices in
        // tri_indices[]. (A quad face returns 2, not 6.)
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

                ufbx_vec3 pos = ufbx_transform_position(to_local, local_pos);
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

    RHIBuffer* vbo = rhi_buffer_create(vertices.data, vertices.count);
    RHIMesh* result = rhi_mesh_create(vbo, NULL); // NULL: no index buffer, draw as plain triangle list (matches load_obj_mesh)

    free(vertices.data);
    return result;
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

    // NOTE: only supports a single mesh per file - if the FBX has multiple
    // meshes/objects, only the first is loaded. Use obj_load_fbx_multi() to
    // bring in every mesh in the file as a parented hierarchy of entities.
    ufbx_mesh* mesh = scene->meshes.data[0];

    // ufbx_get_vertex_vec3(&mesh->vertex_position, ...) returns vertex data in
    // *mesh-local* space, not the target world space we asked for via
    // opts.target_axes/target_unit_meters - that conversion (plus whatever
    // position/rotation/scale the node itself has) lives in the owning
    // node's transform and has to be applied explicitly. Since this loader
    // only ever needs one static, non-animated copy of the mesh, we bake
    // that transform into the vertices once here, rather than storing a
    // transform to apply every frame.
    ufbx_node* node = mesh->instances.count > 0 ? mesh->instances.data[0] : scene->root_node;
    RHIMesh* result = build_rhi_mesh_from_ufbx_mesh(mesh, &node->geometry_to_world);

    ufbx_free_scene(scene);
    return result;
}

FbxImportResult obj_load_fbx_multi(const char* filepath) {
    FbxImportResult result = {0};

    ufbx_load_opts opts = {0};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;
    opts.generate_missing_normals = true;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(filepath, &opts, &error);
    if (!scene) {
        printf("Failed to load FBX file: %s (%s)\n", filepath, error.description.data);
        return result;
    }

    // Every node in the file that has a mesh attached, in ufbx's own stable
    // node order. The first one found becomes the entity everything else in
    // the file gets parented under - mirrors how Unity flattens a multi-
    // object FBX import under one root when you drag it into a scene.
    int mesh_node_count = 0;
    for (size_t i = 0; i < scene->nodes.count; i++) {
        if (scene->nodes.data[i]->mesh) mesh_node_count++;
    }

    if (mesh_node_count == 0) {
        printf("FBX file has no meshes: %s\n", filepath);
        ufbx_free_scene(scene);
        return result;
    }

    result.nodes = malloc(sizeof(FbxMeshNode) * mesh_node_count);
    result.count = mesh_node_count;

    ufbx_node* root_node = NULL;
    ufbx_matrix root_world_inv = {0};
    int out_i = 0;

    for (size_t i = 0; i < scene->nodes.count; i++) {
        ufbx_node* node = scene->nodes.data[i];
        if (!node->mesh) continue;

        FbxMeshNode* out = &result.nodes[out_i];
        ufbx_transform relative_transform;

        if (!root_node) {
            // First mesh in the file: this becomes the top-level entity, so
            // its own Transform should reflect its full world placement.
            root_node = node;
            root_world_inv = ufbx_matrix_invert(&node->node_to_world);
            relative_transform = ufbx_matrix_to_transform(&node->node_to_world);
            out->parent_index = -1;
        } else {
            // Every other mesh's transform relative to the root node, so
            // parenting it under the root entity in the ECS reproduces the
            // file's original layout without us needing a matrix-valued
            // transform component.
            ufbx_matrix relative = ufbx_matrix_mul(&root_world_inv, &node->node_to_world);
            relative_transform = ufbx_matrix_to_transform(&relative);
            out->parent_index = 0;
        }

        ufbx_vec3 euler = ufbx_quat_to_euler(relative_transform.rotation, UFBX_ROTATION_ORDER_XYZ);
        out->position = (Vec3){ (float)relative_transform.translation.x, (float)relative_transform.translation.y, (float)relative_transform.translation.z };
        out->rotation = (Vec3){ (float)euler.x, (float)euler.y, (float)euler.z };
        out->scale    = (Vec3){ (float)relative_transform.scale.x, (float)relative_transform.scale.y, (float)relative_transform.scale.z };

        snprintf(out->name, sizeof(out->name), "%s", node->name.data[0] ? node->name.data : "Mesh");

        // Bake in only this mesh's own local pivot/offset within its node
        // (geometry_to_node) - the mesh's placement in the wider scene is
        // handled by position/rotation/scale above once the caller turns
        // these into parented entities, not by the vertex data itself.
        out->mesh = build_rhi_mesh_from_ufbx_mesh(node->mesh, &node->geometry_to_node);

        out_i++;
    }

    ufbx_free_scene(scene);
    return result;
}

void obj_free_fbx_multi(FbxImportResult* result) {
    if (!result) return;
    free(result->nodes);
    result->nodes = NULL;
    result->count = 0;
}

RHIMesh* obj_load_mesh(const char* filepath) {
    if (ends_with_ci(filepath, ".fbx")) {
        return load_fbx_mesh(filepath);
    }
    return load_obj_mesh(filepath);
}