#ifndef ECS_H
#define ECS_H

#include "../math/math3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_MAX_ENTITIES 256
#define ECS_MAX_CHILDREN 32
#define ECS_INVALID_ENTITY -1

// An entity is just an id (index into the world arrays)
typedef int Entity;

// Bit flags for which components an entity has
typedef enum {
    COMPONENT_TRANSFORM = 1 << 0,
    COMPONENT_MESH      = 1 << 1,
    COMPONENT_SCRIPT    = 1 << 2,
} ComponentType;

// Position/rotation/scale + parent-child hierarchy
typedef struct {
    Vec3 position;
    Vec3 rotation; // degrees
    Vec3 scale;
    int parent; // ECS_INVALID_ENTITY if root
    int child_count;
    int children[ECS_MAX_CHILDREN];
    float model_matrix[16];
    int dirty;
} TransformComponent;

// What mesh/texture/tint to render with
typedef struct {
    char mesh_path[128];
    char texture_path[256];
    float tint[3];
} MeshComponent;

// Lua script attached to an entity
typedef struct {
    char path[256];
} ScriptComponent;

// The whole world: entities + their components
typedef struct {
    int alive[ECS_MAX_ENTITIES];
    unsigned int masks[ECS_MAX_ENTITIES];
    char names[ECS_MAX_ENTITIES][64];
    TransformComponent transforms[ECS_MAX_ENTITIES];
    MeshComponent meshes[ECS_MAX_ENTITIES];
    ScriptComponent scripts[ECS_MAX_ENTITIES];
} EcsWorld;

void ecs_init(EcsWorld* world);

// Creates an entity, makes the name unique like Unity: "Cube", "Cube (1)", "Cube (2)"...
Entity ecs_create_entity(EcsWorld* world, const char* name);
void ecs_destroy_entity(EcsWorld* world, Entity e);
int ecs_is_alive(const EcsWorld* world, Entity e);
int ecs_entity_count(const EcsWorld* world);

void ecs_add_component(EcsWorld* world, Entity e, ComponentType type);
void ecs_remove_component(EcsWorld* world, Entity e, ComponentType type);
int ecs_has_component(const EcsWorld* world, Entity e, ComponentType type);

Entity ecs_find_by_name(const EcsWorld* world, const char* name);
// Renames an entity, also making the new name unique
void ecs_rename_entity(EcsWorld* world, Entity e, const char* new_name);

void ecs_set_parent(EcsWorld* world, Entity child, Entity parent);
// Recomputes model matrices for dirty entities (parents first)
void ecs_update_transforms(EcsWorld* world);

#ifdef __cplusplus
}
#endif

#endif // ECS_H
