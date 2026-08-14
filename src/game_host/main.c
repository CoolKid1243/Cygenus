// src/game_host/main.c
#include "../platform/platform.h"
#include "../renderer/rhi.h"
#include "../renderer/material.h"
#include "../renderer/obj_loader.h"
#include "../camera/camera.h"
#include "../math/math3d.h"
#include "../core/project.h"
#include "../scene/scene.h"
#include "../renderer/primitives.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAX_RUNTIME_OBJECTS 64

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: game_host <scene_file>\n");
        return 1;
    }

    const char* scene_path = argv[1];
    printf("Loading scene: %s\n", scene_path);

    // Set project root based on scene path
    char project_root[256];
    strncpy(project_root, scene_path, sizeof(project_root));
    char* last_slash = strrchr(project_root, '/');
    if (last_slash) {
        *last_slash = '\0';
        char* second_last_slash = strrchr(project_root, '/');
        if (second_last_slash) {
            *second_last_slash = '\0';
        }
    } else {
        strcpy(project_root, ".");
    }
    project_set_root(project_root);
    printf("Project root: %s\n", project_root);

    PlatformWindow* window = platform_create_window("Cygenus Game", 800, 600);
    if (!window) return 1;
    platform_set_mouse_locked(window, 1);

    rhi_init(platform_get_gl_context(window));

    Scene scene;
    scene_init(&scene);
    if (!scene_load(&scene, scene_path)) {
        printf("Failed to load scene: %s\n", scene_path);
        return 1;
    }

    // Update transforms (parent-child hierarchy)
    scene_update_transforms(&scene);

    RHIShader* shader = rhi_shader_create(
        "src/renderer/opengl/opengl_shaders/vertex.glsl",
        "src/renderer/opengl/opengl_shaders/fragment.glsl"
    );
    if (!shader) return 1;

    RHIMesh* meshes[MAX_RUNTIME_OBJECTS] = {0};
    RHITexture* textures[MAX_RUNTIME_OBJECTS] = {0};
    Material materials[MAX_RUNTIME_OBJECTS];

    for (int i = 0; i < scene.object_count; i++) {
        SceneObject* obj = &scene.objects[i];
        if (strncmp(obj->mesh_path, "primitive:", 10) == 0) {
            const char* primitive_name = obj->mesh_path + 10;
            if (strcmp(primitive_name, "cube") == 0) {
                meshes[i] = primitive_create_cube();
            } else if (strcmp(primitive_name, "plane") == 0) {
                meshes[i] = primitive_create_plane(3.0f, 3.0f, 1);
            } else if (strcmp(primitive_name, "sphere") == 0) {
                meshes[i] = primitive_create_sphere(1.0f, 16, 16);
            } else {
                meshes[i] = NULL;
            }
        } else {
            char mesh_full_path[256];
            project_get_path(obj->mesh_path, mesh_full_path, sizeof(mesh_full_path));
            meshes[i] = obj_load_mesh(mesh_full_path);
        }

        material_init(&materials[i], shader);
        materials[i].tint[0] = obj->tint[0];
        materials[i].tint[1] = obj->tint[1];
        materials[i].tint[2] = obj->tint[2];

        if (strcmp(obj->texture_path, "none") != 0) {
            char texture_full_path[256];
            project_get_path(obj->texture_path, texture_full_path, sizeof(texture_full_path));
            textures[i] = rhi_texture_create(texture_full_path);
            if (textures[i]) material_set_texture(&materials[i], textures[i]);
        }
    }

    Camera camera;
    camera_init(&camera, 45.0f, 800.0f/600.0f, 0.1f, 100.0f);
    camera_set_position(&camera, 0.0f, 0.0f, 3.0f);
    camera_set_target(&camera, 0.0f, 0.0f, 0.0f);
    camera_update(&camera);
    rhi_shader_set_mat4(shader, "uProjection", camera_get_projection(&camera));

    float angle = 0.0f;
    float radius = 3.0f;

    while (!platform_should_close(window)) {
        platform_poll_events();
        float dt = platform_get_delta_time();

        angle += dt * 0.5f;
        float eyeX = radius * cosf(angle);
        float eyeZ = radius * sinf(angle);
        camera_set_position(&camera, eyeX, 0.5f, eyeZ);
        camera_set_target(&camera, 0.0f, 0.0f, 0.0f);
        camera_update(&camera);
        rhi_shader_set_mat4(shader, "uView", camera_get_view(&camera));

        rhi_clear(0.2f, 0.3f, 0.3f, 1.0f);

        for (int i = 0; i < scene.object_count; i++) {
            if (!meshes[i]) continue;
            SceneObject* obj = &scene.objects[i];
            float model[16];
            mat4_compose_trs(model, obj->position, obj->rotation, obj->scale);
            rhi_shader_set_mat4(shader, "uModel", model);
            material_bind(&materials[i], scene.objects[i].tint);
            rhi_mesh_draw(meshes[i]);
        }

        platform_swap_buffers(window);

        if (platform_key_pressed(window, 256)) break;
    }

    for (int i = 0; i < scene.object_count; i++) {
        if (meshes[i]) rhi_mesh_destroy(meshes[i]);
        if (textures[i]) rhi_texture_destroy(textures[i]);
    }
    rhi_shader_destroy(shader);
    rhi_shutdown();
    platform_destroy_window(window);

    return 0;
}