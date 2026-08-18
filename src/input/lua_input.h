#ifndef LUA_INPUT_H
#define LUA_INPUT_H

#include "../platform/platform.h"
#include <lua.h>

void lua_input_register(lua_State* L);

void lua_input_set_window(PlatformWindow* window);

#endif // LUA_INPUT_H