#include "scene.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>   // for toupper

// Forward declaration
// static int scene_find_object_by_name(const Scene* scene, const char* name);

void scene_init(Scene* scene) {
    scene->object_count = 0;
    memset(scene->objects, 0, sizeof(scene->objects));
}

// Helper to generate unique name
static void generate_unique_name(Scene* scene, SceneObject* obj) {
    if (strlen(obj->name) == 0) {
        const char* base = obj->mesh_path;
        if (strncmp(base, "primitive:", 10) == 0) base += 10;
        char base_name[64];
        strncpy(base_name, base, sizeof(base_name));
        base_name[0] = toupper(base_name[0]);
        int num = 0;
        char temp[64];
        do {
            if (num == 0) snprintf(temp, sizeof(temp), "%s", base_name);
            else snprintf(temp, sizeof(temp), "%s (%d)", base_name, num);
            num++;
        } while (scene_find_object_by_name(scene, temp) != -1);
        strncpy(obj->name, temp, sizeof(obj->name)-1);
    } else {
        // User-provided name – ensure uniqueness
        char temp[64];
        strncpy(temp, obj->name, sizeof(temp)-1);
        int num = 1;
        while (scene_find_object_by_name(scene, temp) != -1) {
            snprintf(temp, sizeof(temp), "%s (%d)", obj->name, num);
            num++;
        }
        strncpy(obj->name, temp, sizeof(obj->name)-1);
    }
}

int scene_add_object(Scene* scene, SceneObject obj) {
    if (scene->object_count >= SCENE_MAX_OBJECTS) return -1;
    obj.parent_index = -1;
    obj.child_count = 0;
    obj.dirty = 1;
    // Ensure name is unique
    generate_unique_name(scene, &obj);
    scene->objects[scene->object_count] = obj;
    scene->object_count++;
    return scene->object_count - 1;
}

int scene_find_object_by_name(const Scene* scene, const char* name) {
    for (int i = 0; i < scene->object_count; i++) {
        if (strcmp(scene->objects[i].name, name) == 0) return i;
    }
    return -1;
}

void scene_rename_object(Scene* scene, int index, const char* new_name) {
    if (index < 0 || index >= scene->object_count) return;
    SceneObject* obj = &scene->objects[index];
    char temp[64];
    strncpy(temp, new_name, sizeof(temp)-1);
    int num = 1;
    while (1) {
        int found = scene_find_object_by_name(scene, temp);
        if (found == -1 || found == index) break;
        snprintf(temp, sizeof(temp), "%s (%d)", new_name, num);
        num++;
    }
    strncpy(obj->name, temp, sizeof(obj->name)-1);
}

void scene_set_parent(Scene* scene, int child_idx, int parent_idx) {
    if (child_idx < 0 || child_idx >= scene->object_count) return;
    SceneObject* child = &scene->objects[child_idx];
    // Remove from old parent
    if (child->parent_index >= 0) {
        SceneObject* old_parent = &scene->objects[child->parent_index];
        for (int i = 0; i < old_parent->child_count; i++) {
            if (old_parent->child_indices[i] == child_idx) {
                old_parent->child_indices[i] = old_parent->child_indices[old_parent->child_count-1];
                old_parent->child_count--;
                break;
            }
        }
    }
    // Add to new parent
    child->parent_index = parent_idx;
    if (parent_idx >= 0) {
        SceneObject* new_parent = &scene->objects[parent_idx];
        if (new_parent->child_count < MAX_CHILDREN) {
            new_parent->child_indices[new_parent->child_count++] = child_idx;
        }
    }
    // Mark dirty for child and descendants
    child->dirty = 1;
    for (int i = 0; i < child->child_count; i++) {
        scene->objects[child->child_indices[i]].dirty = 1;
    }
}

static void update_transform_recursive(Scene* scene, int idx, const float* parent_matrix) {
    SceneObject* obj = &scene->objects[idx];
    if (obj->dirty) {
        float local[16];
        mat4_compose_trs(local, obj->position, obj->rotation, obj->scale);
        if (parent_matrix) {
            mat4_multiply(obj->model_matrix, parent_matrix, local);
        } else {
            memcpy(obj->model_matrix, local, sizeof(obj->model_matrix));
        }
        obj->dirty = 0;
    }
    for (int i = 0; i < obj->child_count; i++) {
        int child_idx = obj->child_indices[i];
        update_transform_recursive(scene, child_idx, obj->model_matrix);
    }
}

void scene_update_transforms(Scene* scene) {
    for (int i = 0; i < scene->object_count; i++) {
        if (scene->objects[i].parent_index == -1) {
            update_transform_recursive(scene, i, NULL);
        }
    }
}

// Save/Load functions (existing ones, add the new fields to serialization)
int scene_save(const Scene* scene, const char* filepath) {
    FILE* file = fopen(filepath, "w");
    if (!file) {
        printf("Failed to open %s for writing\n", filepath);
        return 0;
    }
    fprintf(file, "SCENE %d\n", scene->object_count);
    for (int i = 0; i < scene->object_count; i++) {
        const SceneObject* obj = &scene->objects[i];
        fprintf(file, "OBJECT\n");
        fprintf(file, "name %s\n", obj->name);
        fprintf(file, "position %f %f %f\n", obj->position.x, obj->position.y, obj->position.z);
        fprintf(file, "rotation %f %f %f\n", obj->rotation.x, obj->rotation.y, obj->rotation.z);
        fprintf(file, "scale %f %f %f\n", obj->scale.x, obj->scale.y, obj->scale.z);
        fprintf(file, "mesh %s\n", obj->mesh_path);
        fprintf(file, "texture %s\n", obj->texture_path);
        fprintf(file, "tint %f %f %f\n", obj->tint[0], obj->tint[1], obj->tint[2]);
        fprintf(file, "script %s\n", obj->script_path);
        fprintf(file, "parent %d\n", obj->parent_index);
        fprintf(file, "END\n");
    }
    fclose(file);
    printf("Scene saved: %s (%d objects)\n", filepath, scene->object_count);
    return 1;
}

int scene_load(Scene* scene, const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        printf("Failed to open %s for reading\n", filepath);
        return 0;
    }
    scene_init(scene);
    int declared_count = 0;
    fscanf(file, "SCENE %d\n", &declared_count);
    char keyword[64];
    while (fscanf(file, "%63s", keyword) == 1) {
        if (strcmp(keyword, "OBJECT") != 0) continue;
        SceneObject obj;
        memset(&obj, 0, sizeof(obj));
        char field[64];
        while (fscanf(file, "%63s", field) == 1) {
            if (strcmp(field, "name") == 0) fscanf(file, "%63s", obj.name);
            else if (strcmp(field, "position") == 0) fscanf(file, "%f %f %f", &obj.position.x, &obj.position.y, &obj.position.z);
            else if (strcmp(field, "rotation") == 0) fscanf(file, "%f %f %f", &obj.rotation.x, &obj.rotation.y, &obj.rotation.z);
            else if (strcmp(field, "scale") == 0) fscanf(file, "%f %f %f", &obj.scale.x, &obj.scale.y, &obj.scale.z);
            else if (strcmp(field, "mesh") == 0) fscanf(file, "%127s", obj.mesh_path);
            else if (strcmp(field, "texture") == 0) fscanf(file, "%255s", obj.texture_path);
            else if (strcmp(field, "tint") == 0) fscanf(file, "%f %f %f", &obj.tint[0], &obj.tint[1], &obj.tint[2]);
            else if (strcmp(field, "script") == 0) fscanf(file, "%255s", obj.script_path);
            else if (strcmp(field, "parent") == 0) fscanf(file, "%d", &obj.parent_index);
            else if (strcmp(field, "END") == 0) break;
        }
        obj.child_count = 0;
        obj.dirty = 1;
        scene_add_object(scene, obj);
    }
    // After loading all, rebuild children lists
    for (int i = 0; i < scene->object_count; i++) {
        scene->objects[i].child_count = 0;
    }
    for (int i = 0; i < scene->object_count; i++) {
        int p = scene->objects[i].parent_index;
        if (p >= 0 && p < scene->object_count) {
            SceneObject* parent = &scene->objects[p];
            if (parent->child_count < MAX_CHILDREN) {
                parent->child_indices[parent->child_count++] = i;
            }
        }
    }
    fclose(file);
    printf("Scene loaded: %s (%d objects)\n", filepath, scene->object_count);
    return 1;
}

void scene_create(Scene* scene) {
    scene_init(scene);
    SceneObject cube;
    memset(&cube, 0, sizeof(cube));
    cube.position = (Vec3){0,0,0};
    cube.rotation = (Vec3){0,0,0};
    cube.scale = (Vec3){1,1,1};
    strcpy(cube.mesh_path, "primitive:cube");
    strcpy(cube.texture_path, "none");
    cube.tint[0] = 1.0f; cube.tint[1] = 1.0f; cube.tint[2] = 1.0f;
    scene_add_object(scene, cube);
    // SceneObject plane;
    // memset(&plane, 0, sizeof(plane));
    // plane.position = (Vec3){0, -0.5f, 0};
    // plane.rotation = (Vec3){0,0,0};
    // plane.scale = (Vec3){1,1,1};
    // strcpy(plane.mesh_path, "primitive:plane");
    // strcpy(plane.texture_path, "none");
    // plane.tint[0] = 0.5f; plane.tint[1] = 0.5f; plane.tint[2] = 0.5f;
    // scene_add_object(scene, plane);
}
