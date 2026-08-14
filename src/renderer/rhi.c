#include "rhi.h"
#include "opengl/opengl.h"
#include <string.h>
#include <stdio.h>

static RHIFunctionTable api = {0};

void rhi_init(void* window) {
    opengl_register(&api);
    if (api.init) {
        api.init(window);
        printf("RHI: Initialized\n");
    }
}

void rhi_shutdown(void) {
    if (api.shutdown) api.shutdown();
}

void rhi_begin_frame(void) {
    if (api.begin_frame) api.begin_frame();
}

void rhi_end_frame(void) {
    if (api.end_frame) api.end_frame();
}

void rhi_clear(float r, float g, float b, float a) {
    if (api.clear) api.clear(r, g, b, a);
}

RHIShader* rhi_shader_create(const char* vertex_path, const char* fragment_path) {
    return api.shader_create ? api.shader_create(vertex_path, fragment_path) : NULL;
}

void rhi_shader_destroy(RHIShader* shader) {
    if (api.shader_destroy) api.shader_destroy(shader);
}

void rhi_shader_use(RHIShader* shader) {
    if (api.shader_use) api.shader_use(shader);
}

RHIBuffer* rhi_buffer_create(const RHIVertex* vertices, size_t vertex_count) {
    return api.buffer_create ? api.buffer_create(vertices, vertex_count) : NULL;
}

RHIBuffer* rhi_index_buffer_create(const uint32_t* indices, size_t index_count) {
    return api.index_buffer_create ? api.index_buffer_create(indices, index_count) : NULL;
}

void rhi_buffer_destroy(RHIBuffer* buffer) {
    if (api.buffer_destroy) api.buffer_destroy(buffer);
}

RHIMesh* rhi_mesh_create(RHIBuffer* vertex_buffer, RHIBuffer* index_buffer) {
    return api.mesh_create ? api.mesh_create(vertex_buffer, index_buffer) : NULL;
}

void rhi_mesh_destroy(RHIMesh* mesh) {
    if (api.mesh_destroy) api.mesh_destroy(mesh);
}

void rhi_mesh_draw(RHIMesh* mesh) {
    if (api.mesh_draw) api.mesh_draw(mesh);
}

RHITexture* rhi_texture_create(const char* filepath) {
    return api.texture_create ? api.texture_create(filepath) : NULL;
}

void rhi_texture_destroy(RHITexture* texture) {
    if (api.texture_destroy) api.texture_destroy(texture);
}

void rhi_texture_bind(RHITexture* texture, int slot) {
    if (api.texture_bind) api.texture_bind(texture, slot);
}

void rhi_shader_set_mat4(RHIShader* shader, const char* name, const float* matrix) {
    if (api.shader_set_mat4) api.shader_set_mat4(shader, name, matrix);
}

void rhi_shader_set_float(RHIShader* shader, const char* name, float value) {
    if (api.shader_set_float) api.shader_set_float(shader, name, value);
}

void rhi_shader_set_int(RHIShader* shader, const char* name, int value) {
    if (api.shader_set_int) api.shader_set_int(shader, name, value);
}

void rhi_shader_set_vec3(RHIShader* shader, const char* name, float x, float y, float z) {
    if (api.shader_set_vec3) api.shader_set_vec3(shader, name, x, y, z);
}

RHIFramebuffer* rhi_framebuffer_create(int width, int height) {
    if (api.framebuffer_create) return api.framebuffer_create(width, height);
    return NULL;
}

void rhi_framebuffer_destroy(RHIFramebuffer* fb) {
    if (api.framebuffer_destroy) api.framebuffer_destroy(fb);
}

void rhi_framebuffer_bind(RHIFramebuffer* fb) {
    if (api.framebuffer_bind) api.framebuffer_bind(fb);
}

void rhi_framebuffer_unbind(void) {
    if (api.framebuffer_unbind) api.framebuffer_unbind();
}

RHITexture* rhi_framebuffer_get_texture(RHIFramebuffer* fb) {
    if (api.framebuffer_get_texture) return api.framebuffer_get_texture(fb);
    return NULL;
}

int rhi_framebuffer_get_width(RHIFramebuffer* fb) {
    if (api.framebuffer_get_width) return api.framebuffer_get_width(fb);
    return 0;
}

int rhi_framebuffer_get_height(RHIFramebuffer* fb) {
    if (api.framebuffer_get_height) return api.framebuffer_get_height(fb);
    return 0;
}

unsigned int rhi_framebuffer_get_texture_id(RHIFramebuffer* fb) {
    if (api.framebuffer_get_texture_id) return api.framebuffer_get_texture_id(fb);
    return 0;
}