#ifndef RHI_H
#define RHI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RHIContext RHIContext;
typedef struct RHIBuffer RHIBuffer;
typedef struct RHITexture RHITexture;
typedef struct RHIShader RHIShader;
typedef struct RHIMesh RHIMesh;
typedef struct RHIFramebuffer RHIFramebuffer;

typedef struct {
    float x, y, z;    // position
    float nx, ny, nz; // normal
    float u, v;       // texture coordinate
} RHIVertex;

typedef struct {
    void (*init)(void* window);
    void (*shutdown)(void);

    void (*begin_frame)(void);
    void (*end_frame)(void);
    void (*clear)(float r, float g, float b, float a);

    RHIShader* (*shader_create)(const char* v, const char* f);
    void (*shader_destroy)(RHIShader* s);
    void (*shader_use)(RHIShader* s);

    void (*shader_set_mat4)(RHIShader* shader, const char* name, const float* matrix);
    void (*shader_set_float)(RHIShader* shader, const char* name, float value);
    void (*shader_set_int)(RHIShader* shader, const char* name, int value);
    void (*shader_set_vec3)(RHIShader* shader, const char* name, float x, float y, float z);

    RHIBuffer* (*buffer_create)(const RHIVertex* v, size_t c);
    RHIBuffer* (*index_buffer_create)(const uint32_t* i, size_t c);
    void (*buffer_destroy)(RHIBuffer* b);

    RHIMesh* (*mesh_create)(RHIBuffer* vb, RHIBuffer* ib);
    void (*mesh_destroy)(RHIMesh* m);
    void (*mesh_draw)(RHIMesh* m);

    RHITexture* (*texture_create)(const char* path);
    void (*texture_destroy)(RHITexture* t);
    void (*texture_bind)(RHITexture* t, int slot);

    RHIFramebuffer* (*framebuffer_create)(int width, int height);
    void (*framebuffer_destroy)(RHIFramebuffer* fb);
    void (*framebuffer_bind)(RHIFramebuffer* fb);
    void (*framebuffer_unbind)(void);
    RHITexture* (*framebuffer_get_texture)(RHIFramebuffer* fb);
    unsigned int (*framebuffer_get_texture_id)(RHIFramebuffer* fb);
    int (*framebuffer_get_width)(RHIFramebuffer* fb);
    int (*framebuffer_get_height)(RHIFramebuffer* fb);
} RHIFunctionTable;

void rhi_init(void* window);
void rhi_shutdown(void);

void rhi_begin_frame(void);
void rhi_end_frame(void);
void rhi_clear(float r, float g, float b, float a);

RHIShader* rhi_shader_create(const char* vertex_path, const char* fragment_path);
void rhi_shader_destroy(RHIShader* shader);
void rhi_shader_use(RHIShader* shader);

void rhi_shader_set_mat4(RHIShader* shader, const char* name, const float* matrix);
void rhi_shader_set_float(RHIShader* shader, const char* name, float value);
void rhi_shader_set_int(RHIShader* shader, const char* name, int value);
void rhi_shader_set_vec3(RHIShader* shader, const char* name, float x, float y, float z);

RHIBuffer* rhi_buffer_create(const RHIVertex* vertices, size_t vertex_count);
RHIBuffer* rhi_index_buffer_create(const uint32_t* indices, size_t index_count);
void rhi_buffer_destroy(RHIBuffer* buffer);

RHIMesh* rhi_mesh_create(RHIBuffer* vertex_buffer, RHIBuffer* index_buffer);
void rhi_mesh_destroy(RHIMesh* mesh);
void rhi_mesh_draw(RHIMesh* mesh);

RHITexture* rhi_texture_create(const char* filepath);
void rhi_texture_destroy(RHITexture* texture);
void rhi_texture_bind(RHITexture* texture, int slot);

unsigned int rhi_framebuffer_get_texture_id(RHIFramebuffer* fb);
RHIFramebuffer* rhi_framebuffer_create(int width, int height);
void rhi_framebuffer_destroy(RHIFramebuffer* fb);
void rhi_framebuffer_bind(RHIFramebuffer* fb);
void rhi_framebuffer_unbind(void);
RHITexture* rhi_framebuffer_get_texture(RHIFramebuffer* fb);
int rhi_framebuffer_get_width(RHIFramebuffer* fb);
int rhi_framebuffer_get_height(RHIFramebuffer* fb);

#ifdef __cplusplus
}
#endif

#endif // RHI_H