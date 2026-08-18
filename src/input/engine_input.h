#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include "../platform/platform.h"

void engine_input_init(PlatformWindow* window);
void engine_input_update(float dt);
void engine_input_shutdown(void);

void engine_input_get_position(float* x, float* y, float* z);
void engine_input_get_front(float* x, float* y, float* z);

int engine_input_editor_toggle_pressed(void);

#endif // ENGINE_INPUT_H