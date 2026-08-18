#include "render_system.h"
#include "material.h"
#include "primitives.h"
#include "obj_loader.h"
#include "../core/project.h"
#include <stdio.h>
#include <string.h>

static RHIShader* active_shader = NULL;
static RHIMesh* cached_meshes[ECS_MAX_ENTITIES];
static RHITexture* cached_textures[ECS_MAX_ENTITIES];
static Material cached_materials[ECS_MAX_ENTITIES];
// What paths each slot was loaded with, so we only reload on change
static char loaded_mesh_paths[ECS_MAX_ENTITIES][128];
static char loaded_texture_paths[ECS_MAX_ENTITIES][256];

void render_system_init(RHIShader* shader) {
    active_shader = shader;
    memset(cached_meshes, 0, sizeof(cached_meshes));
    memset(cached_textures, 0, sizeof(cached_textures));
    memset(loaded_mesh_paths, 0, sizeof(loaded_mesh_paths));
    memset(loaded_texture_paths, 0, sizeof(loaded_texture_paths));
}

static RHIMesh* load_mesh(const char* mesh_path) {
    if (strncmp(mesh_path, "primitive:", 10) == 0) {
        const char* prim = mesh_path + 10;
        if (strcmp(prim, "cube") == 0) return primitive_create_cube();
        if (strcmp(prim, "plane") == 0) return primitive_create_plane(3.0f, 3.0f, 1);
        if (strcmp(prim, "sphere") == 0) return primitive_create_sphere(1.0f, 16, 16);
        // printf("Unknown primitive: %s\n", prim);
        return NULL;
    }
    char full_path[256];
    project_get_path(mesh_path, full_path, sizeof(full_path));
    return obj_load_mesh(full_path);
}

static void free_slot(int i) {
    if (cached_meshes[i]) {
        rhi_mesh_destroy(cached_meshes[i]);
        cached_meshes[i] = NULL;
    }
    if (cached_textures[i]) {
        rhi_texture_destroy(cached_textures[i]);
        cached_textures[i] = NULL;
    }
    loaded_mesh_paths[i][0] = '\0';
    loaded_texture_paths[i][0] = '\0';
}

void render_system_sync(EcsWorld* world) {
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs_is_alive(world, i) || !ecs_has_component(world, i, COMPONENT_MESH)) {
            if (cached_meshes[i] || cached_textures[i]) free_slot(i);
            continue;
        }
        MeshComponent* m = &world->meshes[i];

        if (strcmp(loaded_mesh_paths[i], m->mesh_path) != 0) {
            if (cached_meshes[i]) rhi_mesh_destroy(cached_meshes[i]);
            cached_meshes[i] = m->mesh_path[0] ? load_mesh(m->mesh_path) : NULL;
            snprintf(loaded_mesh_paths[i], sizeof(loaded_mesh_paths[i]), "%s", m->mesh_path);
            material_init(&cached_materials[i], active_shader);
        }

        // Texture comes from the material component (no material = no texture)
        const char* tex_path = "";
        if (ecs_has_component(world, i, COMPONENT_MATERIAL)) {
            tex_path = world->materials[i].texture_path;
        }
        if (strcmp(loaded_texture_paths[i], tex_path) != 0) {
            if (cached_textures[i]) rhi_texture_destroy(cached_textures[i]);
            cached_textures[i] = NULL;
            if (tex_path[0] && strcmp(tex_path, "none") != 0) {
                char full_path[256];
                project_get_path(tex_path, full_path, sizeof(full_path));
                cached_textures[i] = rhi_texture_create(full_path);
            }
            snprintf(loaded_texture_paths[i], sizeof(loaded_texture_paths[i]), "%s", tex_path);
            material_init(&cached_materials[i], active_shader);
            if (cached_textures[i]) material_set_texture(&cached_materials[i], cached_textures[i]);
        }
    }
}

void render_system_draw(EcsWorld* world) {
    static const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    static const float white[3] = {1, 1, 1};
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs_is_alive(world, i) || !ecs_has_component(world, i, COMPONENT_MESH)) continue;
        if (!cached_meshes[i]) continue;
        const float* model = ecs_has_component(world, i, COMPONENT_TRANSFORM)
            ? world->transforms[i].model_matrix : identity;
        const float* color = ecs_has_component(world, i, COMPONENT_MATERIAL)
            ? world->materials[i].color : white;
        rhi_shader_set_mat4(active_shader, "uModel", model);
        material_bind(&cached_materials[i], color);
        rhi_mesh_draw(cached_meshes[i]);
    }
}

void render_system_shutdown(void) {
    for (int i = 0; i < ECS_MAX_ENTITIES; i++) {
        free_slot(i);
    }
    active_shader = NULL;
}
