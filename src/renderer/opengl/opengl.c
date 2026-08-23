#include "opengl.h"
#include "../rhi.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../../third_party/stb_image.h"

static void gl_check(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        printf("GL error [%s]: 0x%04x\n", label, err);
    }
}

struct RHIShader {
    GLuint program_id;
};

typedef struct {
    GLuint id;
    size_t count;
    GLenum target;
} OpenGLBuffer;

typedef struct {
    GLuint vao;
    GLuint vbo_id;
    GLuint ibo_id;
    int vertex_count;
    int index_count;
    int has_indices;
} OpenGLMesh;

typedef struct {
    GLuint id;
    int width, height, channels;
} OpenGLTexture;

typedef struct {
    GLuint fbo;
    GLuint texture;
    GLuint depth_rb;
    int width;
    int height;
} OpenGLFramebuffer;

struct RHITexture {
    GLuint id;
};

static GLFWwindow* gl_window = NULL;

static void opengl_update_viewport(void) {
    if (!gl_window) return;
    int width, height;
    glfwGetFramebufferSize(gl_window, &width, &height);
    glViewport(0, 0, width, height);
}

static void opengl_init(void* win) {
    gl_window = (GLFWwindow*)win;
    glfwMakeContextCurrent(gl_window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        printf("GLEW init failed\n");
        return;
    }
    glGetError(); // GLEW init can leave a spurious error flag set - clear it

    int width, height;
    glfwGetFramebufferSize(gl_window, &width, &height);
    glViewport(0, 0, width, height);

    glEnable(GL_DEPTH_TEST); // needed for correct 3D face ordering

    // Face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CW);

    printf("OpenGL initialized: %s\n", glGetString(GL_VERSION));
    printf("Viewport: %dx%d\n", width, height);
}

static RHIBuffer* opengl_buffer_create(const RHIVertex* vertices, size_t vertex_count) {
    OpenGLBuffer* buffer = malloc(sizeof(OpenGLBuffer));
    if (!buffer) return NULL;

    glGenBuffers(1, &buffer->id);
    glBindBuffer(GL_ARRAY_BUFFER, buffer->id);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(RHIVertex), vertices, GL_STATIC_DRAW);

    buffer->count = vertex_count;
    buffer->target = GL_ARRAY_BUFFER;

    // printf("Vertex buffer created: ID=%u, Count=%zu\n", buffer->id, buffer->count);
    return (RHIBuffer*)buffer;
}

static RHIBuffer* opengl_index_buffer_create(const uint32_t* indices, size_t index_count) {
    OpenGLBuffer* buffer = malloc(sizeof(OpenGLBuffer));
    if (!buffer) return NULL;

    glGenBuffers(1, &buffer->id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32_t), indices, GL_STATIC_DRAW);

    buffer->count = index_count;
    buffer->target = GL_ELEMENT_ARRAY_BUFFER;

    // printf("Index buffer created: ID=%u, Count=%zu\n", buffer->id, buffer->count);
    return (RHIBuffer*)buffer;
}

static void opengl_buffer_destroy(RHIBuffer* buffer) {
    if (!buffer) return;
    OpenGLBuffer* b = (OpenGLBuffer*)buffer;
    glDeleteBuffers(1, &b->id);
    free(b);
}

static void opengl_clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Builds a VAO describing position/normal/UV layout and links it to the given buffers.
static RHIMesh* opengl_mesh_create(RHIBuffer* vertex_buffer, RHIBuffer* index_buffer) {
    if (!vertex_buffer) {
        printf("Cannot create mesh: vertex buffer is NULL\n");
        return NULL;
    }

    OpenGLBuffer* vbo = (OpenGLBuffer*)vertex_buffer;
    OpenGLBuffer* ibo = (OpenGLBuffer*)index_buffer;

    OpenGLMesh* mesh = malloc(sizeof(OpenGLMesh));
    if (!mesh) return NULL;

    glGenVertexArrays(1, &mesh->vao);
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo->id);

    // Attribute 0: position (matches layout(location = 0) in vertex.glsl)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RHIVertex), (void*)offsetof(RHIVertex, x));

    // Attribute 1: normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RHIVertex), (void*)offsetof(RHIVertex, nx));

    // Attribute 2: UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(RHIVertex), (void*)offsetof(RHIVertex, u));

    if (ibo) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo->id);
        mesh->index_count = (int)ibo->count;
        mesh->has_indices = 1;
        mesh->ibo_id = ibo->id;
    } else {
        mesh->index_count = 0;
        mesh->has_indices = 0;
        mesh->ibo_id = 0;
    }

    mesh->vbo_id = vbo->id;
    mesh->vertex_count = (int)vbo->count;

    glBindVertexArray(0);
    gl_check("mesh_create");

    // printf("Mesh created: VAO=%u, Vertices=%d, Indices=%d\n", mesh->vao, mesh->vertex_count, mesh->index_count);
    return (RHIMesh*)mesh;
}

static void opengl_mesh_destroy(RHIMesh* mesh) {
    if (!mesh) return;
    OpenGLMesh* m = (OpenGLMesh*)mesh;
    glDeleteVertexArrays(1, &m->vao);
    free(m);
}

static void opengl_mesh_draw(RHIMesh* mesh) {
    if (!mesh) return;
    OpenGLMesh* m = (OpenGLMesh*)mesh;

    glBindVertexArray(m->vao);
    if (m->has_indices) {
        glDrawElements(GL_TRIANGLES, m->index_count, GL_UNSIGNED_INT, (void*)0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, m->vertex_count);
    }
    glBindVertexArray(0);
}

static char* read_file(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("Failed to open: %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char* content = malloc(size + 1);
    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);
    return content;
}

static GLuint compile_shader(const char* source, GLenum type, const char* type_name) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, NULL, info);
        printf("Error compiling %s shader:\n%s\n", type_name, info);
        return 0;
    }
    return shader;
}

static RHIShader* opengl_shader_create(const char* vertex_path, const char* fragment_path) {
    char* vertex_source = read_file(vertex_path);
    char* fragment_source = read_file(fragment_path);

    if (!vertex_source || !fragment_source) {
        free(vertex_source);
        free(fragment_source);
        return NULL;
    }

    GLuint vertex = compile_shader(vertex_source, GL_VERTEX_SHADER, "vertex");
    GLuint fragment = compile_shader(fragment_source, GL_FRAGMENT_SHADER, "fragment");
    free(vertex_source);
    free(fragment_source);

    if (vertex == 0 || fragment == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return NULL;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, 512, NULL, info);
        printf("Error linking shader program:\n%s\n", info);
        glDeleteProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return NULL;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    RHIShader* shader = malloc(sizeof(RHIShader));
    shader->program_id = program;

    // printf("Shader created: Program ID=%u\n", program);
    return shader;
}

static void opengl_shader_destroy(RHIShader* shader) {
    if (shader) {
        glDeleteProgram(shader->program_id);
        free(shader);
    }
}

static void opengl_shader_use(RHIShader* shader) {
    if (shader) glUseProgram(shader->program_id);
}

static void opengl_shader_set_mat4(RHIShader* shader, const char* name, const float* matrix) {
    if (!shader || !name || !matrix) return;
    GLint loc = glGetUniformLocation(shader->program_id, name);
    if (loc == -1) return;
    glProgramUniformMatrix4fv(shader->program_id, loc, 1, GL_FALSE, matrix);
}

static void opengl_shader_set_float(RHIShader* shader, const char* name, float value) {
    if (!shader || !name) return;
    GLint loc = glGetUniformLocation(shader->program_id, name);
    if (loc == -1) return;
    glProgramUniform1f(shader->program_id, loc, value);
}

static void opengl_shader_set_int(RHIShader* shader, const char* name, int value) {
    if (!shader || !name) return;
    GLint loc = glGetUniformLocation(shader->program_id, name);
    if (loc == -1) return;
    glProgramUniform1i(shader->program_id, loc, value);
}

static void opengl_shader_set_vec3(RHIShader* shader, const char* name, float x, float y, float z) {
    if (!shader || !name) return;
    GLint loc = glGetUniformLocation(shader->program_id, name);
    if (loc == -1) return;
    glProgramUniform3f(shader->program_id, loc, x, y, z);
}

// Loads an image file from disk and uploads it as a 2D texture.
static RHITexture* opengl_texture_create(const char* filepath) {
    stbi_set_flip_vertically_on_load(1); // image files are top-down, GL UVs are bottom-up

    int width, height, channels;
    unsigned char* data = stbi_load(filepath, &width, &height, &channels, 0);
    if (!data) {
        printf("Failed to load texture: %s\n", filepath);
        return NULL;
    }

    GLenum format = GL_RGB;
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    OpenGLTexture* tex = malloc(sizeof(OpenGLTexture));
    tex->width = width;
    tex->height = height;
    tex->channels = channels;

    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    // printf("Texture loaded: %s (%dx%d, %d channels)\n", filepath, width, height, channels);
    return (RHITexture*)tex;
}

static void opengl_texture_destroy(RHITexture* texture) {
    if (!texture) return;
    OpenGLTexture* t = (OpenGLTexture*)texture;
    glDeleteTextures(1, &t->id);
    free(t);
}

static void opengl_texture_bind(RHITexture* texture, int slot) {
    if (!texture) return;
    OpenGLTexture* t = (OpenGLTexture*)texture;
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, t->id);
}

static void opengl_begin_frame(void) {
    opengl_update_viewport();
}
static void opengl_end_frame(void) {}

static RHIFramebuffer* opengl_framebuffer_create(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    
    OpenGLFramebuffer* fb = malloc(sizeof(OpenGLFramebuffer));
    fb->width = width;
    fb->height = height;
    
    // Create FBO
    glGenFramebuffers(1, &fb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);
    
    // Create color texture
    glGenTextures(1, &fb->texture);
    glBindTexture(GL_TEXTURE_2D, fb->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->texture, 0);
    
    // Create depth renderbuffer
    glGenRenderbuffers(1, &fb->depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, fb->depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depth_rb);
    
    // Check FBO
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &fb->fbo);
        glDeleteTextures(1, &fb->texture);
        glDeleteRenderbuffers(1, &fb->depth_rb);
        free(fb);
        return NULL;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return (RHIFramebuffer*)fb;
}

static void opengl_framebuffer_destroy(RHIFramebuffer* fb) {
    if (!fb) return;
    OpenGLFramebuffer* f = (OpenGLFramebuffer*)fb;
    glDeleteFramebuffers(1, &f->fbo);
    glDeleteTextures(1, &f->texture);
    glDeleteRenderbuffers(1, &f->depth_rb);
    free(f);
}

static void opengl_framebuffer_bind(RHIFramebuffer* fb) {
    if (!fb) return;
    OpenGLFramebuffer* f = (OpenGLFramebuffer*)fb;
    glBindFramebuffer(GL_FRAMEBUFFER, f->fbo);
    glViewport(0, 0, f->width, f->height);
}

static void opengl_framebuffer_unbind(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static unsigned int opengl_framebuffer_get_texture_id(RHIFramebuffer* fb) {
    if (!fb) return 0;
    OpenGLFramebuffer* f = (OpenGLFramebuffer*)fb;
    return f->texture;
}

static int opengl_framebuffer_get_width(RHIFramebuffer* fb) {
    if (!fb) return 0;
    OpenGLFramebuffer* f = (OpenGLFramebuffer*)fb;
    return f->width;
}

static int opengl_framebuffer_get_height(RHIFramebuffer* fb) {
    if (!fb) return 0;
    OpenGLFramebuffer* f = (OpenGLFramebuffer*)fb;
    return f->height;
}

void opengl_register(RHIFunctionTable* table) {
    table->init = opengl_init;
    table->shutdown = NULL;
    table->begin_frame = opengl_begin_frame;
    table->end_frame = opengl_end_frame;
    table->clear = opengl_clear;

    table->shader_create = opengl_shader_create;
    table->shader_destroy = opengl_shader_destroy;
    table->shader_use = opengl_shader_use;

    table->shader_set_mat4 = opengl_shader_set_mat4;
    table->shader_set_float = opengl_shader_set_float;
    table->shader_set_int = opengl_shader_set_int;
    table->shader_set_vec3 = opengl_shader_set_vec3;

    table->buffer_create = opengl_buffer_create;
    table->index_buffer_create = opengl_index_buffer_create;
    table->buffer_destroy = opengl_buffer_destroy;

    table->mesh_create = opengl_mesh_create;
    table->mesh_destroy = opengl_mesh_destroy;
    table->mesh_draw = opengl_mesh_draw;

    table->texture_create = opengl_texture_create;
    table->texture_destroy = opengl_texture_destroy;
    table->texture_bind = opengl_texture_bind;

    table->framebuffer_create = opengl_framebuffer_create;
    table->framebuffer_destroy = opengl_framebuffer_destroy;
    table->framebuffer_bind = opengl_framebuffer_bind;
    table->framebuffer_unbind = opengl_framebuffer_unbind;
    table->framebuffer_get_texture_id = opengl_framebuffer_get_texture_id;
    table->framebuffer_get_width = opengl_framebuffer_get_width;
    table->framebuffer_get_height = opengl_framebuffer_get_height;

    printf("OpenGL backend registered\n");
}