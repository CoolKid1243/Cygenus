#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "rhi.h"
#include "../ecs/ecs.h"

#ifdef __cplusplus
extern "C" {
#endif

void render_system_init(RHIShader* shader);
// Loads/reloads meshes and textures for entities whose paths changed
void render_system_sync(EcsWorld* world);
// Draws every entity that has a mesh component
void render_system_draw(EcsWorld* world);
void render_system_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // RENDER_SYSTEM_H
