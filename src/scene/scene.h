#ifndef SCENE_H
#define SCENE_H

#include "../math/math3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCENE_MAX_OBJECTS 64
#define MAX_CHILDREN 32

typedef struct {
    char name[64];
    Vec3 position;
    Vec3 rotation;  // degrees
    Vec3 scale;
    char mesh_path[128];
    char texture_path[256];
    float tint[3];
    char script_path[256];
    int parent_index;   // -1 if root
    int child_count;
    int child_indices[MAX_CHILDREN];
    float model_matrix[16];
    int dirty;
} SceneObject;

typedef struct {
    SceneObject objects[SCENE_MAX_OBJECTS];
    int object_count;
} Scene;

void scene_init(Scene* scene);
int scene_add_object(Scene* scene, SceneObject obj);
int scene_find_object_by_name(const Scene* scene, const char* name);
void scene_rename_object(Scene* scene, int index, const char* new_name);
void scene_set_parent(Scene* scene, int child_idx, int parent_idx);
void scene_update_transforms(Scene* scene);   // recomputes all model matrices
void scene_create(Scene* scene);
int scene_save(const Scene* scene, const char* filepath);
int scene_load(Scene* scene, const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // SCENE_H