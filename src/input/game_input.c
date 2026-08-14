#include "game_input.h"
#include "lua_input.h"
#include "../core/project.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>

static lua_State* L = NULL;
static int has_update_function = 0;

void game_input_init(PlatformWindow* window, const char* project_relative_script_path) {
    L = luaL_newstate();
    luaL_openlibs(L);

    lua_input_set_window(window);
    lua_input_register(L);

    char full_path[256];
    project_get_path(project_relative_script_path, full_path, sizeof(full_path));

    // Game input is optional per project - if the script is missing or
    // broken, print a warning and just skip it instead of crashing.
    if (luaL_dofile(L, full_path) != LUA_OK) {
        printf("Game input script not loaded (%s): %s\n", full_path, lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_close(L);
        L = NULL;
        return;
    }

    lua_getglobal(L, "update");
    has_update_function = lua_isfunction(L, -1);
    lua_pop(L, 1);
}

void game_input_update(float dt) {
    if (!L || !has_update_function) return;

    lua_getglobal(L, "update");
    lua_pushnumber(L, dt);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        printf("Game input runtime error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void game_input_shutdown(void) {
    if (L) {
        lua_close(L);
        L = NULL;
    }
}