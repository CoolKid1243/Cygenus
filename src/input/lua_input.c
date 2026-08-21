#include "lua_input.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static PlatformWindow* bound_window = NULL;

void lua_input_set_window(PlatformWindow* window) {
    bound_window = window;
}

static const struct { const char* name; int code; } named_keys[] = {
    {"SPACE",         32},
    {"ESCAPE",        256},
    {"ENTER",         257},
    {"RETURN",        257},
    {"TAB",           258},
    {"BACKSPACE",     259},
    {"INSERT",        260},
    {"DELETE",        261},
    {"RIGHT",         262},
    {"LEFT",          263},
    {"DOWN",          264},
    {"UP",            265},
    {"PAGE_UP",       266},
    {"PAGE_DOWN",     267},
    {"HOME",          268},
    {"END",           269},
    {"CAPS_LOCK",     280},
    {"SCROLL_LOCK",   281},
    {"NUM_LOCK",      282},
    {"PRINT_SCREEN",  283},
    {"PAUSE",         284},

    // Numpad
    {"KP_0",          320},
    {"KP_1",          321},
    {"KP_2",          322},
    {"KP_3",          323},
    {"KP_4",          324},
    {"KP_5",          325},
    {"KP_6",          326},
    {"KP_7",          327},
    {"KP_8",          328},
    {"KP_9",          329},
    {"KP_DECIMAL",    330},
    {"KP_DIVIDE",     331},
    {"KP_MULTIPLY",   332},
    {"KP_SUBTRACT",   333},
    {"KP_ADD",        334},
    {"KP_ENTER",      335},
    {"KP_EQUAL",      336},

    // Modifiers (left/right specific)
    {"LEFT_SHIFT",    340},
    {"LEFT_CONTROL",  341},
    {"LEFT_CTRL",     341}, // alias
    {"LEFT_ALT",      342},
    {"LEFT_SUPER",    343}, // Windows key / Cmd key
    {"RIGHT_SHIFT",   344},
    {"RIGHT_CONTROL", 345},
    {"RIGHT_CTRL",    345}, // alias
    {"RIGHT_ALT",     346},
    {"RIGHT_SUPER",   347}, // Windows key / Cmd key
    {"MENU",          348},
};
#define NAMED_KEY_COUNT (sizeof(named_keys) / sizeof(named_keys[0]))

static int single_char_to_code(char c) {
    if (c >= 'A' && c <= 'Z') return c; // letters
    if (c >= '0' && c <= '9') return c; // digits
    switch (c) {
        case '\'': case ',': case '-': case '.': case '/':
        case ';':  case '=': case '[': case '\\': case ']':
        case '`':
            return (int)(unsigned char)c;
        default:
            return -1;
    }
}

static int key_name_to_code(const char* name) {
    size_t len = strlen(name);

    if (len == 1) {
        char c = (char)toupper((unsigned char)name[0]);
        int code = single_char_to_code(c);
        if (code != -1) return code;
    }

    // F1-F25 handled generically rather than listed one by one.
    if ((name[0] == 'F' || name[0] == 'f') && len >= 2 && len <= 3) {
        char* end = NULL;
        long n = strtol(name + 1, &end, 10);
        if (end != name + 1 && *end == '\0' && n >= 1 && n <= 25) {
            return 289 + (int)n; // GLFW_KEY_F1 == 290
        }
    }

    for (size_t i = 0; i < NAMED_KEY_COUNT; i++) {
        if (strcmp(name, named_keys[i].name) == 0) {
            return named_keys[i].code;
        }
    }

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