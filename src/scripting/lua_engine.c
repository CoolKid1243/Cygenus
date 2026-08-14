#include "lua_engine.h"
#include "../scene/scene.h"
#include <lauxlib.h>
#include <string.h>

static EcsWorld* bound_world = NULL;

void lua_engine_set_world(EcsWorld* world) {
    bound_world = world;
}

// Reads an entity id argument and checks it's valid
static Entity check_entity(lua_State* L, int arg) {
    Entity e = (Entity)luaL_checkinteger(L, arg);
    if (!ecs_is_alive(bound_world, e)) {
        luaL_error(L, "invalid entity id: %d", e);
    }
    return e;
}

// engine.create_entity(name) -> id
static int l_create_entity(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "Entity");
    Entity e = ecs_create_entity(bound_world, name);
    ecs_add_component(bound_world, e, COMPONENT_TRANSFORM);
    bound_world->transforms[e].scale = (Vec3){1, 1, 1};
    bound_world->transforms[e].dirty = 1;
    lua_pushinteger(L, e);
    return 1;
}

// engine.create_cube(name) / create_sphere / create_plane -> id
static int create_primitive(lua_State* L, const char* prim) {
    Entity e = scene_spawn_primitive(bound_world, prim);
    const char* name = luaL_optstring(L, 1, NULL);
    if (name) ecs_rename_entity(bound_world, e, name);
    lua_pushinteger(L, e);
    return 1;
}
static int l_create_cube(lua_State* L)   { return create_primitive(L, "cube"); }
static int l_create_sphere(lua_State* L) { return create_primitive(L, "sphere"); }
static int l_create_plane(lua_State* L)  { return create_primitive(L, "plane"); }

// engine.destroy_entity(id)
static int l_destroy_entity(lua_State* L) {
    ecs_destroy_entity(bound_world, check_entity(L, 1));
    return 0;
}

// engine.find_entity(name) -> id or nil
static int l_find_entity(lua_State* L) {
    Entity e = ecs_find_by_name(bound_world, luaL_checkstring(L, 1));
    if (e == ECS_INVALID_ENTITY) lua_pushnil(L);
    else lua_pushinteger(L, e);
    return 1;
}

// engine.get_name(id) -> name
static int l_get_name(lua_State* L) {
    lua_pushstring(L, bound_world->names[check_entity(L, 1)]);
    return 1;
}

// engine.set_name(id, name) -- gets (1) suffix if taken
static int l_set_name(lua_State* L) {
    Entity e = check_entity(L, 1);
    ecs_rename_entity(bound_world, e, luaL_checkstring(L, 2));
    return 0;
}

// engine.get_position(id) -> x, y, z
static int l_get_position(lua_State* L) {
    TransformComponent* t = &bound_world->transforms[check_entity(L, 1)];
    lua_pushnumber(L, t->position.x);
    lua_pushnumber(L, t->position.y);
    lua_pushnumber(L, t->position.z);
    return 3;
}

// engine.set_position(id, x, y, z)
static int l_set_position(lua_State* L) {
    TransformComponent* t = &bound_world->transforms[check_entity(L, 1)];
    t->position.x = (float)luaL_checknumber(L, 2);
    t->position.y = (float)luaL_checknumber(L, 3);
    t->position.z = (float)luaL_checknumber(L, 4);
    t->dirty = 1;
    return 0;
}

// engine.get_rotation(id) -> x, y, z
static int l_get_rotation(lua_State* L) {
    TransformComponent* t = &bound_world->transforms[check_entity(L, 1)];
    lua_pushnumber(L, t->rotation.x);
    lua_pushnumber(L, t->rotation.y);
    lua_pushnumber(L, t->rotation.z);
    return 3;
}

// engine.set_rotation(id, x, y, z)
static int l_set_rotation(lua_State* L) {
    TransformComponent* t = &bound_world->transforms[check_entity(L, 1)];
    t->rotation.x = (float)luaL_checknumber(L, 2);
    t->rotation.y = (float)luaL_checknumber(L, 3);
    t->rotation.z = (float)luaL_checknumber(L, 4);
    t->dirty = 1;
    return 0;
}

// engine.get_scale(id) -> x, y, z
static int l_get_scale(lua_State* L) {
    TransformComponent* t = &bound_world->transforms[check_entity(L, 1)];
    lua_pushnumber(L, t->scale.x);
    lua_pushnumber(L, t->scale.y);
    lua_pushnumber(L, t->scale.z);
    return 3;
}

// engine.set_scale(id, x, y, z)
static int l_set_scale(lua_State* L) {
    TransformComponent* t = &bound_world->transforms[check_entity(L, 1)];
    t->scale.x = (float)luaL_checknumber(L, 2);
    t->scale.y = (float)luaL_checknumber(L, 3);
    t->scale.z = (float)luaL_checknumber(L, 4);
    t->dirty = 1;
    return 0;
}

// engine.set_tint(id, r, g, b)
static int l_set_tint(lua_State* L) {
    Entity e = check_entity(L, 1);
    MeshComponent* m = &bound_world->meshes[e];
    m->tint[0] = (float)luaL_checknumber(L, 2);
    m->tint[1] = (float)luaL_checknumber(L, 3);
    m->tint[2] = (float)luaL_checknumber(L, 4);
    return 0;
}

// engine.set_parent(child_id, parent_id or nil)
static int l_set_parent(lua_State* L) {
    Entity child = check_entity(L, 1);
    Entity parent = lua_isnoneornil(L, 2) ? ECS_INVALID_ENTITY : check_entity(L, 2);
    ecs_set_parent(bound_world, child, parent);
    return 0;
}

// engine.entity_count() -> count
static int l_entity_count(lua_State* L) {
    lua_pushinteger(L, ecs_entity_count(bound_world));
    return 1;
}

static const luaL_Reg engine_functions[] = {
    {"create_entity",  l_create_entity},
    {"create_cube",    l_create_cube},
    {"create_sphere",  l_create_sphere},
    {"create_plane",   l_create_plane},
    {"destroy_entity", l_destroy_entity},
    {"find_entity",    l_find_entity},
    {"get_name",       l_get_name},
    {"set_name",       l_set_name},
    {"get_position",   l_get_position},
    {"set_position",   l_set_position},
    {"get_rotation",   l_get_rotation},
    {"set_rotation",   l_set_rotation},
    {"get_scale",      l_get_scale},
    {"set_scale",      l_set_scale},
    {"set_tint",       l_set_tint},
    {"set_parent",     l_set_parent},
    {"entity_count",   l_entity_count},
    {NULL, NULL}
};

void lua_engine_register(lua_State* L) {
    luaL_newlib(L, engine_functions);
    lua_setglobal(L, "engine");
}
