#include "ecs.h"
#include <stdio.h>
#include <string.h>

void ecs_init(EcsWorld* world) {
    memset(world, 0, sizeof(EcsWorld));
}

// Checks if any other entity already uses this name
static int name_taken(const EcsWorld* world, const char* name, Entity ignore) {
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!world->alive[i] || i == ignore) continue;
        if (strcmp(world->names[i], name) == 0) return 1;
    }
    return 0;
}

// Turns "Cube" into "Cube (1)", "Cube (2)"... until the name is free
static void make_unique_name(const EcsWorld* world, const char* base, char* out, int out_size, Entity ignore) {
    snprintf(out, out_size, "%s", base);
    int num = 1;
    while (name_taken(world, out, ignore)) {
        snprintf(out, out_size, "%s (%d)", base, num);
        num++;
    }
}

Entity ecs_create_entity(EcsWorld* world, const char* name) {
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (world->alive[i]) continue;
        world->alive[i] = 1;
        world->masks[i] = 0;
        make_unique_name(world, (name && name[0]) ? name : "Entity", world->names[i], sizeof(world->names[i]), i);
        memset(&world->transforms[i], 0, sizeof(TransformComponent));
        memset(&world->meshes[i], 0, sizeof(MeshComponent));
        memset(&world->materials[i], 0, sizeof(MaterialComponent));
        memset(&world->scripts[i], 0, sizeof(ScriptComponent));
        memset(&world->cameras[i], 0, sizeof(CameraComponent));
        world->transforms[i].parent = ECS_INVALID_ENTITY;
        return i;
    }
    printf("ECS: out of entities (max %d)\n", ECS_MAX_ENTITIES);
    return ECS_INVALID_ENTITY;
}

void ecs_destroy_entity(EcsWorld* world, Entity e) {
    if (!ecs_is_alive(world, e)) return;
    // Detach from parent
    ecs_set_parent(world, e, ECS_INVALID_ENTITY);
    // Unparent children so they don't point at a dead entity
    TransformComponent* t = &world->transforms[e];
    while (t->child_count > 0) {
        ecs_set_parent(world, t->children[0], ECS_INVALID_ENTITY);
    }
    world->alive[e] = 0;
    world->masks[e] = 0;
    world->names[e][0] = '\0';
}

int ecs_is_alive(const EcsWorld* world, Entity e) {
    return e >= 0 && e < ECS_MAX_ENTITIES && world->alive[e];
}

int ecs_entity_count(const EcsWorld* world) {
    int count = 0;
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (world->alive[i]) count++;
    }
    return count;
}

void ecs_add_component(EcsWorld* world, Entity e, ComponentType type) {
    if (!ecs_is_alive(world, e)) return;
    if (world->masks[e] & type) return;
    world->masks[e] |= type;
    // Sensible defaults for freshly added components
    if (type == COMPONENT_TRANSFORM) {
        TransformComponent* t = &world->transforms[e];
        if (t->scale.x == 0 && t->scale.y == 0 && t->scale.z == 0) t->scale = (Vec3){1, 1, 1};
        t->dirty = 1;
    } else if (type == COMPONENT_MATERIAL) {
        MaterialComponent* m = &world->materials[e];
        m->color[0] = m->color[1] = m->color[2] = 1.0f;
        snprintf(m->texture_path, sizeof(m->texture_path), "none");
    } else if (type == COMPONENT_CAMERA) {
        CameraComponent* c = &world->cameras[e];
        c->fov = 45.0f;
        c->near_plane = 0.1f;
        c->far_plane = 100.0f;
        c->display_tag = 0;
    }
}

void ecs_remove_component(EcsWorld* world, Entity e, ComponentType type) {
    if (!ecs_is_alive(world, e)) return;
    world->masks[e] &= ~type;
}

int ecs_has_component(const EcsWorld* world, Entity e, ComponentType type) {
    if (!ecs_is_alive(world, e)) return 0;
    return (world->masks[e] & type) != 0;
}

Entity ecs_find_by_name(const EcsWorld* world, const char* name) {
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (world->alive[i] && strcmp(world->names[i], name) == 0) return i;
    }
    return ECS_INVALID_ENTITY;
}

void ecs_rename_entity(EcsWorld* world, Entity e, const char* new_name) {
    if (!ecs_is_alive(world, e)) return;
    if (!new_name || !new_name[0]) return;
    char unique[64];
    make_unique_name(world, new_name, unique, sizeof(unique), e);
    snprintf(world->names[e], sizeof(world->names[e]), "%s", unique);
}

void ecs_set_parent(EcsWorld* world, Entity child, Entity parent) {
    if (!ecs_is_alive(world, child)) return;
    TransformComponent* ct = &world->transforms[child];
    // Remove from old parent's child list
    if (ct->parent != ECS_INVALID_ENTITY && ecs_is_alive(world, ct->parent)) {
        TransformComponent* old_parent = &world->transforms[ct->parent];
        for (int i = 0; i < old_parent->child_count; i++) {
            if (old_parent->children[i] == child) {
                old_parent->children[i] = old_parent->children[old_parent->child_count - 1];
                old_parent->child_count--;
                break;
            }
        }
    }
    // Add to new parent's child list
    ct->parent = ECS_INVALID_ENTITY;
    if (ecs_is_alive(world, parent) && parent != child) {
        TransformComponent* pt = &world->transforms[parent];
        if (pt->child_count < ECS_MAX_CHILDREN) {
            pt->children[pt->child_count++] = child;
            ct->parent = parent;
        }
    }
    ct->dirty = 1;
}

static void update_transform_recursive(EcsWorld* world, Entity e, const float* parent_matrix, int parent_dirty) {
    TransformComponent* t = &world->transforms[e];
    int was_dirty = t->dirty || parent_dirty;
    if (was_dirty) {
        float local[16];
        mat4_compose_trs(local, t->position, t->rotation, t->scale);
        if (parent_matrix) {
            mat4_multiply(t->model_matrix, parent_matrix, local);
        } else {
            memcpy(t->model_matrix, local, sizeof(t->model_matrix));
        }
        t->dirty = 0;
    }
    for (int i = 0; i < t->child_count; i++) {
        update_transform_recursive(world, t->children[i], t->model_matrix, was_dirty);
    }
}

void ecs_update_transforms(EcsWorld* world) {
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!world->alive[i]) continue;
        if (!ecs_has_component(world, i, COMPONENT_TRANSFORM)) continue;
        if (world->transforms[i].parent == ECS_INVALID_ENTITY) {
            update_transform_recursive(world, i, NULL, 0);
        }
    }
}

Entity ecs_get_display_camera(const EcsWorld* world, int tag) {
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!world->alive[i]) continue;
        if (!ecs_has_component(world, i, COMPONENT_CAMERA)) continue;
        if (world->cameras[i].display_tag == tag) return i;
    }
    return ECS_INVALID_ENTITY;
}

void ecs_set_camera_display_tag(EcsWorld* world, Entity e, int tag) {
    if (!ecs_is_alive(world, e) || !ecs_has_component(world, e, COMPONENT_CAMERA)) return;
    
    // If setting to a specific tag (not 0), handle auto-increment
    if (tag > 0) {
        // Find if any camera already has this tag
        Entity existing_camera = ecs_get_display_camera(world, tag);
        if (existing_camera != ECS_INVALID_ENTITY && existing_camera != e) {
            // Auto-increment the existing camera's tag
            int new_tag = tag + 1;
            // Find the next available tag
            while (ecs_get_display_camera(world, new_tag) != ECS_INVALID_ENTITY) {
                new_tag++;
            }
            world->cameras[existing_camera].display_tag = new_tag;
        }
    }
    
    // Set the new tag
    world->cameras[e].display_tag = tag;
}

void ecs_save_state(EcsWorld* world, EcsWorld* backup) {
    memcpy(backup, world, sizeof(EcsWorld));
}

void ecs_restore_state(EcsWorld* world, const EcsWorld* backup) {
    memcpy(world, backup, sizeof(EcsWorld));
}
