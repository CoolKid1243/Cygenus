#include "platform/platform.h"
#include "renderer/rhi.h"
#include "renderer/material.h"
#include "renderer/obj_loader.h"
#include "camera/camera.h"
#include "math/math3d.h"
#include "core/project.h"
#include "scene/scene.h"
#include "input/engine_input.h"
#include "input/game_input.h"
#include "renderer/primitives.h"
#include "editor/editor.h"
#include <stdio.h>
#include <string.h>
#include <GLFW/glfw3.h>

#define MAX_RUNTIME_OBJECTS 64

// Change this to switch projects
#define PROJECT_ROOT "projects/sample_project"

static void reload_scene_meshes(Scene* scene, RHIMesh** meshes, RHITexture** textures, Material* materials, RHIShader* shader);

int main() {
    project_set_root(PROJECT_ROOT);

    char scene_path[256];
    project_get_path("scenes/sample.scene", scene_path, sizeof(scene_path));

    PlatformWindow* window = platform_create_window("Cygenus - Game Engine", 1260, 840);
    if (!window) return 1;

    rhi_init(platform_get_gl_context(window));

    engine_input_init(window);
    game_input_init(window, "scripts/game_input.lua");

    RHIShader* shader = rhi_shader_create(
        "src/renderer/opengl/opengl_shaders/vertex.glsl",
        "src/renderer/opengl/opengl_shaders/fragment.glsl"
    );
    if (!shader) return 1;

    // Scene Loading
    Scene scene;
    if (!scene_load(&scene, scene_path)) {
        scene_init(&scene);
        scene_create(&scene);
        scene_save(&scene, scene_path);
    }

    // Editor Setup
    editor_init(window);
    editor_set_scene(&scene);

    // Load Scene Objects
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
            } else {
                printf("Unknown primitive: %s\n", primitive_name);
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

    // Camera Setup
    int win_width, win_height;
    platform_get_window_size(window, &win_width, &win_height);

    Camera camera;
    camera_init(&camera, 45.0f, (float)win_width / (float)win_height, 0.1f, 100.0f);
    camera_set_position(&camera, 0.0f, 0.0f, 3.0f);
    camera_set_target(&camera, 0.0f, 0.0f, 0.0f);
    camera_update(&camera);
    rhi_shader_set_mat4(shader, "uProjection", camera_get_projection(&camera));

    // Main Loop
    while (!platform_should_close(window)) {
        platform_poll_events();
        float dt = platform_get_delta_time();
        if (dt > 0.1f) dt = 0.1f;

        // FPS counter
        static float fps_timer = 0.0f;
        static int frame_count = 0;
        frame_count++;
        fps_timer += dt;
        if (fps_timer >= 1.0f) {
            char fps_text[64];
            snprintf(fps_text, sizeof(fps_text), "FPS: %.0f", (float)frame_count / fps_timer);
            editor_set_console_text(fps_text);
            frame_count = 0;
            fps_timer = 0.0f;
        }

        // Update Input
        engine_input_update(dt);
        game_input_update(dt);

        float vp_x, vp_y, vp_w, vp_h;
        editor_get_viewport_rect(&vp_x, &vp_y, &vp_w, &vp_h);

        // Update camera aspect ratio to match viewport (prevent stretching)
        if (vp_w > 0 && vp_h > 0) {
            camera.aspect = vp_w / vp_h;
            camera.dirty = 1;
        }
        camera_update(&camera);

        // Get Camera Position from Input
        float pos_x, pos_y, pos_z;
        float front_x, front_y, front_z;
        engine_input_get_position(&pos_x, &pos_y, &pos_z);
        engine_input_get_front(&front_x, &front_y, &front_z);

        camera_set_position(&camera, pos_x, pos_y, pos_z);
        camera_set_target(&camera, pos_x + front_x, pos_y + front_y, pos_z + front_z);
        camera_update(&camera);

        // Start ImGui Frame
        editor_new_frame();

        static int prev_object_count = -1;
        if (scene.object_count != prev_object_count) {
            reload_scene_meshes(&scene, meshes, textures, materials, shader);
            prev_object_count = scene.object_count;
        }

        scene_update_transforms(&scene);

        RHIFramebuffer* fb = editor_get_framebuffer();
        if (fb) {
            rhi_framebuffer_bind(fb);
            rhi_clear(0.2f, 0.3f, 0.3f, 1.0f);
            rhi_shader_set_mat4(shader, "uView", camera_get_view(&camera));
            rhi_shader_set_mat4(shader, "uProjection", camera_get_projection(&camera));

            for (int i = 0; i < scene.object_count; i++) {
                if (!meshes[i]) continue;
                SceneObject* obj = &scene.objects[i];
                rhi_shader_set_mat4(shader, "uModel", obj->model_matrix);
                material_bind(&materials[i], obj->tint);
                rhi_mesh_draw(meshes[i]);
            }

            rhi_framebuffer_unbind();
        }

        // Render ImGui (Editor UI) - displays the framebuffer texture
        editor_render();

        // Swap buffers
        platform_swap_buffers(window);

        // Handle ESC to quit
        if (platform_key_pressed(window, 256)) break;
    }

    // Cleanup
    for (int i = 0; i < scene.object_count; i++) {
        if (meshes[i]) rhi_mesh_destroy(meshes[i]);
        if (textures[i]) rhi_texture_destroy(textures[i]);
    }
    rhi_shader_destroy(shader);
    engine_input_shutdown();
    game_input_shutdown();
    editor_shutdown();
    rhi_shutdown();
    platform_destroy_window(window);

    return 0;
}
static void reload_scene_meshes(Scene* scene, RHIMesh** meshes, RHITexture** textures, Material* materials, RHIShader* shader) {
    for (int i = 0; i < scene->object_count; i++) {
        // Destroy old mesh if exists
        if (meshes[i]) {
            rhi_mesh_destroy(meshes[i]);
            meshes[i] = NULL;
        }
        if (textures[i]) {
            rhi_texture_destroy(textures[i]);
            textures[i] = NULL;
        }
        // Load new mesh
        SceneObject* obj = &scene->objects[i];
        if (strncmp(obj->mesh_path, "primitive:", 10) == 0) {
            const char* prim = obj->mesh_path + 10;
            if (strcmp(prim, "cube") == 0) meshes[i] = primitive_create_cube();
            else if (strcmp(prim, "plane") == 0) meshes[i] = primitive_create_plane(3.0f, 3.0f, 1);
            else if (strcmp(prim, "sphere") == 0) meshes[i] = primitive_create_sphere(1.0f, 16, 16);
            else meshes[i] = NULL;
        } else {
            char full_path[256];
            project_get_path(obj->mesh_path, full_path, sizeof(full_path));
            meshes[i] = obj_load_mesh(full_path);
        }
        // Init material
        material_init(&materials[i], shader);
        materials[i].tint[0] = obj->tint[0];
        materials[i].tint[1] = obj->tint[1];
        materials[i].tint[2] = obj->tint[2];
        if (strcmp(obj->texture_path, "none") != 0) {
            char tex_path[256];
            project_get_path(obj->texture_path, tex_path, sizeof(tex_path));
            textures[i] = rhi_texture_create(tex_path);
            if (textures[i]) material_set_texture(&materials[i], textures[i]);
        } else {
            textures[i] = NULL;
        }
    }
}
