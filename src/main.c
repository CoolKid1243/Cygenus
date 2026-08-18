#include "platform/platform.h"
#include "renderer/rhi.h"
#include "renderer/render_system.h"
#include "camera/camera.h"
#include "core/project.h"
#include "ecs/ecs.h"
#include "scene/scene.h"
#include "input/engine_input.h"
#include "scripting/script_system.h"
#include "editor/editor.h"
#include <stdio.h>

// Change this to switch projects
#define PROJECT_ROOT "projects/sample_project"

// Moves the camera to wherever the fly-cam input says it should be
static void update_camera(Camera* camera) {
    float vp_x, vp_y, vp_w, vp_h;
    editor_get_viewport_rect(&vp_x, &vp_y, &vp_w, &vp_h);
    if (vp_w > 0 && vp_h > 0) {
        camera->aspect = vp_w / vp_h;
        camera->dirty = 1;
    }

    float x, y, z, fx, fy, fz;
    engine_input_get_position(&x, &y, &z);
    engine_input_get_front(&fx, &fy, &fz);
    camera_set_position(camera, x, y, z);
    camera_set_target(camera, x + fx, y + fy, z + fz);
    camera_update(camera);
}

// Sends the FPS to the editor console once per second
static void update_fps_counter(float dt) {
    static float timer = 0.0f;
    static int frames = 0;
    frames++;
    timer += dt;
    if (timer >= 1.0f) {
        char text[32];
        snprintf(text, sizeof(text), "FPS: %.0f", (float)frames / timer);
        editor_set_console_text(text);
        frames = 0;
        timer = 0.0f;
    }
}

int main() {
    project_set_root(PROJECT_ROOT);

    char scene_path[256];
    project_get_path("scenes/sample.scene", scene_path, sizeof(scene_path));

    PlatformWindow* window = platform_create_window("Cygenus - Game Engine", 1260, 840);
    if (!window) return 1;

    rhi_init(platform_get_gl_context(window));
    engine_input_init(window);

    RHIShader* shader = rhi_shader_create(
        "src/renderer/opengl/opengl_shaders/vertex.glsl",
        "src/renderer/opengl/opengl_shaders/fragment.glsl"
    );
    if (!shader) return 1;

    // Load the scene, or make a default one if there isn't one yet
    EcsWorld world;
    if (!scene_load(&world, scene_path)) {
        scene_new(&world);
        scene_save(&world, scene_path);
    }

    render_system_init(shader);
    script_system_init(window, &world);

    editor_init(window);
    editor_set_world(&world);

    int win_width, win_height;
    platform_get_window_size(window, &win_width, &win_height);

    Camera camera;
    camera_init(&camera, 45.0f, (float)win_width / (float)win_height, 0.1f, 100.0f);

    while (!platform_should_close(window)) {
        platform_poll_events();
        float dt = platform_get_delta_time();
        if (dt > 0.1f) dt = 0.1f;

        update_fps_counter(dt);
        engine_input_update(dt);
        script_system_update(dt);
        update_camera(&camera);

        editor_new_frame();

        render_system_sync(&world);
        ecs_update_transforms(&world);

        RHIFramebuffer* fb = editor_get_framebuffer();
        if (fb) {
            rhi_framebuffer_bind(fb);
            rhi_clear(0.2f, 0.3f, 0.3f, 1.0f);
            rhi_shader_set_mat4(shader, "uView", camera_get_view(&camera));
            rhi_shader_set_mat4(shader, "uProjection", camera_get_projection(&camera));
            render_system_draw(&world);
            rhi_framebuffer_unbind();
        }

        editor_render();
        platform_swap_buffers(window);

        // ESC quits
        if (platform_key_pressed(window, 256)) break;
    }

    render_system_shutdown();
    rhi_shader_destroy(shader);
    engine_input_shutdown();
    script_system_shutdown();
    editor_shutdown();
    rhi_shutdown();
    platform_destroy_window(window);

    return 0;
}
