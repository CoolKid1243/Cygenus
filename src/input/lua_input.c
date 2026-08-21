#include "lua_input.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

static PlatformWindow* bound_window = NULL;

void lua_input_set_window(PlatformWindow* window) {
    bound_window = window;
}

// Converts a key name string into a GLFW key code, so Lua scripts use
// readable names instead of numbers.
static int key_name_to_code(const char* name) {
    if (strlen(name) == 1 && name[0] >= 'A' && name[0] <= 'Z') {
        return name[0]; // GLFW letter codes match ASCII uppercase
    }
    if (strcmp(name, "SPACE") == 0) return 32;
    if (strcmp(name, "COMMA") == 0) return 44;
    if (strcmp(name, "ESCAPE") == 0) return 256;
    if (strcmp(name, "LEFT_SHIFT") == 0) return 340;
    if (strcmp(name, "F1") == 0) return 290;
    return -1; // unknown key name
}

static int l_key_held(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int code = key_name_to_code(name);
    lua_pushboolean(L, (code != -1) && platform_key_held(bound_window, code));
    return 1;
}

static int l_key_pressed(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int code = key_name_to_code(name);
    lua_pushboolean(L, (code != -1) && platform_key_pressed(bound_window, code));
    return 1;
}

static int l_key_released(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int code = key_name_to_code(name);
    lua_pushboolean(L, (code != -1) && platform_key_released(bound_window, code));
    return 1;
}

// Returns TWO Lua values: local x, y = input.mouse_position()
static int l_mouse_position(lua_State* L) {
    double x, y;
    platform_get_mouse_position(bound_window, &x, &y);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

static int l_set_mouse_locked(lua_State* L) {
    int locked = lua_toboolean(L, 1);
    platform_set_mouse_locked(bound_window, locked);
    return 0;
}

static int mouse_button_name_to_code(const char* name) {
    if (strcmp(name, "LEFT") == 0) return 0;
    if (strcmp(name, "RIGHT") == 0) return 1;
    if (strcmp(name, "MIDDLE") == 0) return 2;
    return -1; // unknown button name
}

static int l_mouse_button_pressed(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int code = mouse_button_name_to_code(name);
    lua_pushboolean(L, (code != -1) && platform_mouse_button_pressed(bound_window, code));
    return 1;
}

static int l_mouse_button_released(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int code = mouse_button_name_to_code(name);
    lua_pushboolean(L, (code != -1) && platform_mouse_button_released(bound_window, code));
    return 1;
}

static const luaL_Reg input_functions[] = {
    {"key_held",            l_key_held},
    {"key_pressed",         l_key_pressed},
    {"key_released",        l_key_released},
    {"mouse_position",      l_mouse_position},
    {"set_mouse_locked",    l_set_mouse_locked},
    {"mouse_button_pressed", l_mouse_button_pressed},
    {"mouse_button_released", l_mouse_button_released},
    {NULL, NULL}
};

void lua_input_register(lua_State* L) {
    luaL_newlib(L, input_functions);
    lua_setglobal(L, "input");
}