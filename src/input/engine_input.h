#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include "../platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

void engine_input_init(PlatformWindow* window);
void engine_input_update(float dt);
void engine_input_shutdown(void);

void engine_input_get_position(float* x, float* y, float* z);
void engine_input_get_front(float* x, float* y, float* z);

void engine_input_set_position(float x, float y, float z);
void engine_input_set_yaw_pitch(float yaw_degrees, float pitch_degrees);

int engine_input_editor_toggle_pressed(void);
int engine_input_is_mouse_over_viewport(void);

#ifdef __cplusplus
}
#endif

#endif // ENGINE_INPUT_H