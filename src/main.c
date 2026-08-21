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
#include <math.h>

// External ECS function
extern Entity ecs_get_display_camera(const EcsWorld* world, int tag);

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

// Builds a camera from the main camera entity in the scene
static void build_game_camera(Camera* camera, EcsWorld* world, Entity main_camera_entity) {
    if (!ecs_is_alive(world, main_camera_entity)) return;
    
    TransformComponent* transform = &world->transforms[main_camera_entity];
    CameraComponent* cam_comp = &world->cameras[main_camera_entity];
    
    // Set camera position from transform
    camera_set_position(camera, transform->position.x, transform->position.y, transform->position.z);
    
    // Calculate forward direction from rotation (Euler angles)
    float yaw_rad = transform->rotation.y * 3.14159265358979323846f / 180.0f;
    float pitch_rad = transform->rotation.x * 3.14159265358979323846f / 180.0f;
    
    Vec3 front;
    front.x = cosf(yaw_rad) * cosf(pitch_rad);
    front.y = sinf(pitch_rad);
    front.z = sinf(yaw_rad) * cosf(pitch_rad);
    
    // Normalize front vector
    float len = sqrtf(front.x * front.x + front.y * front.y + front.z * front.z);
    if (len > 0.0001f) {
        front.x /= len;
        front.y /= len;
        front.z /= len;
    }
    
    // Set target to be in front of the camera
    camera_set_target(camera, 
        transform->position.x + front.x,
        transform->position.y + front.y,
        transform->position.z + front.z);
    
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

        // Update camera matrices for gizmos
        editor_set_camera_matrices(camera_get_view(&camera), camera_get_projection(&camera));

        render_system_sync(&world);
        ecs_update_transforms(&world);

        // Render to editor viewport (always active)
        RHIFramebuffer* fb = editor_get_framebuffer();
        if (fb) {
            rhi_framebuffer_bind(fb);
            rhi_clear(0.2f, 0.3f, 0.3f, 1.0f);
            rhi_shader_set_mat4(shader, "uView", camera_get_view(&camera));
            rhi_shader_set_mat4(shader, "uProjection", camera_get_projection(&camera));
            render_system_draw(&world);
            rhi_framebuffer_unbind();
        }

        // Render to game window 
        RHIFramebuffer* game_fb = editor_get_game_framebuffer();
        if (game_fb) {
            int game_fb_width, game_fb_height;
            editor_get_game_framebuffer_size(&game_fb_width, &game_fb_height);
            
            Entity main_camera = ecs_get_display_camera(&world, 1);
            
            if (ecs_is_alive(&world, main_camera)) {
                Camera game_camera;
                CameraComponent* cam_comp = &world.cameras[main_camera];
                camera_init(&game_camera, cam_comp->fov, 
                    (float)game_fb_width / (float)game_fb_height,
                    cam_comp->near_plane, cam_comp->far_plane);
                build_game_camera(&game_camera, &world, main_camera);
                
                rhi_framebuffer_bind(game_fb);
                rhi_clear(0.1f, 0.1f, 0.15f, 1.0f); // Darker background for game view
                rhi_shader_set_mat4(shader, "uView", camera_get_view(&game_camera));
                rhi_shader_set_mat4(shader, "uProjection", camera_get_projection(&game_camera));
                render_system_draw(&world);
                rhi_framebuffer_unbind();
            }
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
