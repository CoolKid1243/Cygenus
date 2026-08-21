#include "engine_input.h"
#include "lua_input.h"
#include "../editor/editor.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>

// Engine input owns its own Lua state, fully separate from game_input's.
static lua_State* L = NULL;

static int l_is_mouse_over_viewport(lua_State* L) {
    lua_pushboolean(L, engine_input_is_mouse_over_viewport());
    return 1;
}

void engine_input_init(PlatformWindow* window) {
    L = luaL_newstate();
    luaL_openlibs(L);

    lua_input_set_window(window);
    lua_input_register(L);

    // Register custom engine functions
    lua_pushcfunction(L, l_is_mouse_over_viewport);
    lua_setglobal(L, "is_mouse_over_viewport");

    if (luaL_dofile(L, "src/input/engine_input.lua") != LUA_OK) {
        printf("Engine input script error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void engine_input_update(float dt) {
    if (!L) return;

    lua_getglobal(L, "update");
    lua_pushnumber(L, dt);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        printf("Engine input runtime error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

static float get_number_global(const char* name) {
    lua_getglobal(L, name);
    float value = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return value;
}

void engine_input_get_position(float* x, float* y, float* z) {
    *x = get_number_global("pos_x");
    *y = get_number_global("pos_y");
    *z = get_number_global("pos_z");
}

void engine_input_get_front(float* x, float* y, float* z) {
    *x = get_number_global("front_x");
    *y = get_number_global("front_y");
    *z = get_number_global("front_z");
}

int engine_input_editor_toggle_pressed(void) {
    lua_getglobal(L, "editor_toggle");
    int pressed = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return pressed;
}

void engine_input_shutdown(void) {
    if (L) {
        lua_close(L);
        L = NULL;
    }
}

int engine_input_is_mouse_over_viewport(void) {
    return editor_is_mouse_over_viewport();
}