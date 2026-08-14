#ifndef EDITOR_H
#define EDITOR_H

#include "../platform/platform.h"
#include "../scene/scene.h"
#include "../renderer/rhi.h"

#ifdef __cplusplus
extern "C" {
#endif

void editor_init(PlatformWindow* window);
void editor_shutdown(void);
void editor_new_frame(void);
void editor_update(float dt);
void editor_render(void);
void editor_set_scene(Scene* scene);
void editor_get_viewport_rect(float* x, float* y, float* w, float* h);
int  editor_is_mouse_over_viewport(void);
void editor_run_game(void);
RHIFramebuffer* editor_get_framebuffer(void);

// Console
void editor_set_console_text(const char* text);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_H