#include "script_system.h"
#include "lua_engine.h"
#include "../input/lua_input.h"
#include "../core/project.h"
#include "../ecs/ecs.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include "../platform/win_dirent.h"
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#else
#include <dirent.h>
#endif

#define MAX_SCRIPTS 64

// External ECS functions (defined in ecs.c)
extern void ecs_save_state(EcsWorld* world, EcsWorld* backup);
extern void ecs_restore_state(EcsWorld* world, const EcsWorld* backup);

// Each script gets its own Lua state so their globals don't clash
typedef struct {
    lua_State* L;
    char path[512];
    int has_update;
} LoadedScript;

static LoadedScript scripts[MAX_SCRIPTS];
static int script_count = 0;
static int playing = 0;
static PlatformWindow* bound_window = NULL;
static EcsWorld* world_ref = NULL;
static EcsWorld world_backup;

void script_system_init(PlatformWindow* window, EcsWorld* world) {
    bound_window = window;
    lua_input_set_window(window);
    lua_engine_set_world(world);
    world_ref = world;
    script_count = 0;
    playing = 0;
}

// Loads one .lua file into its own state and calls its start() if present
static void load_script(const char* path) {
    if (script_count >= MAX_SCRIPTS) {
        printf("Too many scripts (max %d), skipping %s\n", MAX_SCRIPTS, path);
        return;
    }
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_input_register(L);
    lua_engine_register(L);

    if (luaL_dofile(L, path) != LUA_OK) {
        printf("Script error (%s): %s\n", path, lua_tostring(L, -1));
        lua_close(L);
        return;
    }

    LoadedScript* s = &scripts[script_count];
    s->L = L;
    snprintf(s->path, sizeof(s->path), "%s", path);

    lua_getglobal(L, "update");
    s->has_update = lua_isfunction(L, -1);
    lua_pop(L, 1);

    lua_getglobal(L, "start");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            printf("Script start() error (%s): %s\n", path, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }

    script_count++;
    // printf("Loaded script: %s\n", path);
}

// Walks a directory tree and loads every .lua file it finds
static void scan_for_scripts(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_for_scripts(full_path);
        } else {
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".lua") == 0) load_script(full_path);
        }
    }
    closedir(dir);
}

static void unload_all_scripts(void) {
    for (int i = 0; i < script_count; i++) {
        if (scripts[i].L) lua_close(scripts[i].L);
        scripts[i].L = NULL;
    }
    script_count = 0;
}

void script_system_set_playing(int should_play) {
    if (should_play == playing) return;
    playing = should_play;
    if (playing) {
        // Save the current world state before entering play mode
        if (world_ref) {
            ecs_save_state(world_ref, &world_backup);
        }
        
        // Fresh load every play so scripts always start from a clean state
        unload_all_scripts();
        char root[256];
        project_get_path("", root, sizeof(root));
        size_t len = strlen(root);
        if (len > 0 && root[len - 1] == '/') root[len - 1] = '\0';
        scan_for_scripts(root);
        // printf("Play: %d script(s) running\n", script_count);
    } else {
        unload_all_scripts();
        // Restore the world state when stopping play mode
        if (world_ref) {
            ecs_restore_state(world_ref, &world_backup);
        }
        // printf("Stopped\n");
    }
}

int script_system_is_playing(void) {
    return playing;
}

void script_system_update(float dt) {
    if (!playing) return;
    for (int i = 0; i < script_count; i++) {
        LoadedScript* s = &scripts[i];
        if (!s->L || !s->has_update) continue;
        lua_getglobal(s->L, "update");
        lua_pushnumber(s->L, dt);
        if (lua_pcall(s->L, 1, 0, 0) != LUA_OK) {
            printf("Script update() error (%s): %s\n", s->path, lua_tostring(s->L, -1));
            lua_pop(s->L, 1);
        }
    }
}

void script_system_shutdown(void) {
    unload_all_scripts();
    playing = 0;
}

void script_system_set_world(EcsWorld* world) {
    world_ref = world;
}