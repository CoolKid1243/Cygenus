#ifndef SCENE_H
#define SCENE_H

#include "../ecs/ecs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Makes a fresh scene with a single cube
void scene_new(EcsWorld* world);

// Spawns a primitive entity: "cube", "sphere" or "plane"
Entity scene_spawn_primitive(EcsWorld* world, const char* primitive);

int scene_save(const EcsWorld* world, const char* filepath);
int scene_load(EcsWorld* world, const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // SCENE_H
