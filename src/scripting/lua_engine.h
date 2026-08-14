#ifndef LUA_ENGINE_H
#define LUA_ENGINE_H

#include "../ecs/ecs.h"
#include <lua.h>

// Registers the global "engine" table (entity create/find/transform/etc)
void lua_engine_register(lua_State* L);
// All scripts talk to this world
void lua_engine_set_world(EcsWorld* world);

#endif // LUA_ENGINE_H
