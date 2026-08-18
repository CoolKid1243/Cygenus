#ifndef MATERIAL_H
#define MATERIAL_H

#include "rhi.h"

typedef struct {
    RHIShader* shader;
    RHITexture* texture;
    float tint[3];
    int use_texture;
} Material;

void material_init(Material* mat, RHIShader* shader);
void material_set_texture(Material* mat, RHITexture* texture);
void material_bind(Material* mat, const float* tint);

#endif // MATERIAL_H