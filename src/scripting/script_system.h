#ifndef SCRIPT_SYSTEM_H
#define SCRIPT_SYSTEM_H

#include "../ecs/ecs.h"
#include "../platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

void script_system_init(PlatformWindow* window, EcsWorld* world);
// Play mode: loads every .lua file found anywhere in the project folder,
// calls start() once, then update(dt) every frame until stopped
void script_system_set_playing(int playing);
int script_system_is_playing(void);
void script_system_update(float dt);
void script_system_shutdown(void);

// Set the world reference for state management
void script_system_set_world(EcsWorld* world);

#ifdef __cplusplus
}
#endif

#endif // SCRIPT_SYSTEM_H
