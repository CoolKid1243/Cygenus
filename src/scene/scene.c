#include "scene.h"
#include <stdio.h>
#include <string.h>

void scene_new(EcsWorld* world) {
    ecs_init(world);
    scene_spawn_primitive(world, "cube");
}

Entity scene_spawn_primitive(EcsWorld* world, const char* primitive) {
    // Capitalize first letter for the entity name ("cube" -> "Cube")
    char name[64];
    snprintf(name, sizeof(name), "%s", primitive);
    if (name[0] >= 'a' && name[0] <= 'z') name[0] -= 32;

    Entity e = ecs_create_entity(world, name);
    if (e == ECS_INVALID_ENTITY) return e;

    ecs_add_component(world, e, COMPONENT_TRANSFORM);
    TransformComponent* t = &world->transforms[e];
    t->position = (Vec3){0, 0, 0};
    t->rotation = (Vec3){0, 0, 0};
    t->scale = (Vec3){1, 1, 1};
    t->dirty = 1;

    ecs_add_component(world, e, COMPONENT_MESH);
    MeshComponent* m = &world->meshes[e];
    snprintf(m->mesh_path, sizeof(m->mesh_path), "primitive:%s", primitive);
    snprintf(m->texture_path, sizeof(m->texture_path), "none");
    m->tint[0] = 1.0f; m->tint[1] = 1.0f; m->tint[2] = 1.0f;

    return e;
}

int scene_save(const EcsWorld* world, const char* filepath) {
    FILE* file = fopen(filepath, "w");
    if (!file) {
        printf("Failed to open %s for writing\n", filepath);
        return 0;
    }
    // Map entity id -> save order index so parents survive gaps in ids
    int save_index[ECS_MAX_ENTITIES];
    int count = 0;
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        save_index[i] = world->alive[i] ? count++ : -1;
    }
    fprintf(file, "SCENE %d\n", count);
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!world->alive[i]) continue;
        const TransformComponent* t = &world->transforms[i];
        const MeshComponent* m = &world->meshes[i];
        const ScriptComponent* s = &world->scripts[i];
        fprintf(file, "ENTITY\n");
        fprintf(file, "name %s\n", world->names[i]);
        fprintf(file, "position %f %f %f\n", t->position.x, t->position.y, t->position.z);
        fprintf(file, "rotation %f %f %f\n", t->rotation.x, t->rotation.y, t->rotation.z);
        fprintf(file, "scale %f %f %f\n", t->scale.x, t->scale.y, t->scale.z);
        fprintf(file, "mesh %s\n", m->mesh_path[0] ? m->mesh_path : "none");
        fprintf(file, "texture %s\n", m->texture_path[0] ? m->texture_path : "none");
        fprintf(file, "tint %f %f %f\n", m->tint[0], m->tint[1], m->tint[2]);
        fprintf(file, "script %s\n", s->path[0] ? s->path : "none");
        fprintf(file, "parent %d\n", t->parent == ECS_INVALID_ENTITY ? -1 : save_index[t->parent]);
        fprintf(file, "END\n");
    }
    fclose(file);
    printf("Scene saved: %s (%d entities)\n", filepath, count);
    return 1;
}

// Reads one line, strips the trailing newline
static int read_line(FILE* file, char* out, int out_size) {
    if (!fgets(out, out_size, file)) return 0;
    out[strcspn(out, "\r\n")] = '\0';
    return 1;
}

int scene_load(EcsWorld* world, const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        printf("Failed to open %s for reading\n", filepath);
        return 0;
    }
    ecs_init(world);

    Entity loaded[ECS_MAX_ENTITIES];
    int parents[ECS_MAX_ENTITIES];
    int count = 0;

    char line[512];
    Entity current = ECS_INVALID_ENTITY;
    while (read_line(file, line, sizeof(line))) {
        if (strncmp(line, "ENTITY", 6) == 0 || strncmp(line, "OBJECT", 6) == 0) {
            current = ecs_create_entity(world, "Entity");
            if (current == ECS_INVALID_ENTITY) break;
            ecs_add_component(world, current, COMPONENT_TRANSFORM);
            ecs_add_component(world, current, COMPONENT_MESH);
            world->transforms[current].scale = (Vec3){1, 1, 1};
            world->transforms[current].dirty = 1;
            loaded[count] = current;
            parents[count] = -1;
            count++;
        } else if (current != ECS_INVALID_ENTITY) {
            TransformComponent* t = &world->transforms[current];
            MeshComponent* m = &world->meshes[current];
            if (strncmp(line, "name ", 5) == 0) {
                ecs_rename_entity(world, current, line + 5);
            } else if (strncmp(line, "position ", 9) == 0) {
                sscanf(line + 9, "%f %f %f", &t->position.x, &t->position.y, &t->position.z);
            } else if (strncmp(line, "rotation ", 9) == 0) {
                sscanf(line + 9, "%f %f %f", &t->rotation.x, &t->rotation.y, &t->rotation.z);
            } else if (strncmp(line, "scale ", 6) == 0) {
                sscanf(line + 6, "%f %f %f", &t->scale.x, &t->scale.y, &t->scale.z);
            } else if (strncmp(line, "mesh ", 5) == 0) {
                snprintf(m->mesh_path, sizeof(m->mesh_path), "%.127s", line + 5);
            } else if (strncmp(line, "texture ", 8) == 0) {
                snprintf(m->texture_path, sizeof(m->texture_path), "%.255s", line + 8);
            } else if (strncmp(line, "tint ", 5) == 0) {
                sscanf(line + 5, "%f %f %f", &m->tint[0], &m->tint[1], &m->tint[2]);
            } else if (strncmp(line, "script ", 7) == 0) {
                if (line[7] && strcmp(line + 7, "none") != 0) {
                    snprintf(world->scripts[current].path, sizeof(world->scripts[current].path), "%.255s", line + 7);
                    ecs_add_component(world, current, COMPONENT_SCRIPT);
                }
            } else if (strncmp(line, "parent ", 7) == 0) {
                sscanf(line + 7, "%d", &parents[count - 1]);
            } else if (strcmp(line, "END") == 0) {
                current = ECS_INVALID_ENTITY;
            }
        }
    }
    fclose(file);

    // Hook up parents now that every entity exists
    for (int i = 0; i < count; i++) {
        if (parents[i] >= 0 && parents[i] < count) {
            ecs_set_parent(world, loaded[i], loaded[parents[i]]);
        }
    }

    printf("Scene loaded: %s (%d entities)\n", filepath, count);
    return 1;
}
