#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "../platform/platform.h"

void game_input_init(PlatformWindow* window, const char* project_relative_script_path);
void game_input_update(float dt);
void game_input_shutdown(void);

#endif // GAME_INPUT_H