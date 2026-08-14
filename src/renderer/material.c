#include "material.h"

void material_init(Material* mat, RHIShader* shader) {
    mat->shader = shader;
    mat->texture = NULL;
    mat->tint[0] = 1.0f;
    mat->tint[1] = 1.0f;
    mat->tint[2] = 1.0f;
    mat->use_texture = 0;
}

void material_set_texture(Material* mat, RHITexture* texture) {
    mat->texture = texture;
    mat->use_texture = 1;
}

void material_bind(Material* mat, const float* tint) {
    if (!mat || !mat->shader) return;

    rhi_shader_use(mat->shader);
    rhi_shader_set_int(mat->shader, "uUseTexture", mat->use_texture);
    // Use the passed tint, fallback to mat->tint if NULL
    if (tint) {
        rhi_shader_set_vec3(mat->shader, "uTint", tint[0], tint[1], tint[2]);
    } else {
        rhi_shader_set_vec3(mat->shader, "uTint", mat->tint[0], mat->tint[1], mat->tint[2]);
    }

    if (mat->use_texture && mat->texture) {
        rhi_texture_bind(mat->texture, 0);
        rhi_shader_set_int(mat->shader, "uTexture", 0);
    }
}