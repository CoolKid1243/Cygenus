#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "rhi.h"
#include "../math/math3d.h"

#ifdef __cplusplus
extern "C" {
#endif

RHIMesh* obj_load_mesh(const char* filepath);

// One mesh from a multi-mesh FBX import (see obj_load_fbx_multi below).
// position/rotation/scale are meant to be dropped straight into a
// TransformComponent - rotation is in degrees, matching mat4_compose_trs's
// convention.
typedef struct {
    RHIMesh* mesh;
    char name[64];
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    int parent_index;
} FbxMeshNode;

typedef struct {
    FbxMeshNode* nodes;
    int count; // 0 if the file failed to load or had no meshes
} FbxImportResult;

FbxImportResult obj_load_fbx_multi(const char* filepath);

void obj_free_fbx_multi(FbxImportResult* result);

#ifdef __cplusplus
}
#endif

#endif