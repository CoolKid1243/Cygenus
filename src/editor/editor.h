#ifndef EDITOR_H
#define EDITOR_H

#include "../platform/platform.h"
#include "../ecs/ecs.h"
#include "../renderer/rhi.h"

#ifdef __cplusplus
extern "C" {
#endif

void editor_init(PlatformWindow* window);
void editor_shutdown(void);
void editor_new_frame(void);
void editor_render(void);
void editor_set_world(EcsWorld* world);
void editor_get_viewport_rect(float* x, float* y, float* w, float* h);
int  editor_is_mouse_over_viewport(void);
RHIFramebuffer* editor_get_framebuffer(void);

// Game window functions
RHIFramebuffer* editor_get_game_framebuffer(void);
void editor_get_game_framebuffer_size(int* width, int* height);

// Camera matrix functions for gizmos
void editor_set_camera_matrices(const float* view, const float* projection);
void editor_get_camera_matrices(float* view, float* projection);

// Console
void editor_set_console_text(const char* text);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_H