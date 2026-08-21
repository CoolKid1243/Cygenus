#include "platform.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLFW-based platform layer - Windows
#if defined(_WIN32)

struct PlatformWindow {
    GLFWwindow* glfw_window;
    int width;
    int height;
    double last_time;
    float delta_time;
};

// Key/button state is tracked globally since GLFW callbacks don't carry user context here.
static int previous_key_state[512] = {0};
static int current_key_state[512] = {0};
static int previous_mouse_state[8] = {0};
static int current_mouse_state[8] = {0};

static int current_width = 0;
static int current_height = 0;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error (%d): %s\n", error, description);
}

static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key >= 0 && key < 512) {
        if (action == GLFW_PRESS) current_key_state[key] = 1;
        else if (action == GLFW_RELEASE) current_key_state[key] = 0;
    }
}

static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button >= 0 && button < 8) {
        if (action == GLFW_PRESS) current_mouse_state[button] = 1;
        else if (action == GLFW_RELEASE) current_mouse_state[button] = 0;
    }
}

static void glfw_framebuffer_size_callback(GLFWwindow* win, int width, int height) {
    current_width = width;
    current_height = height;
}

PlatformWindow* platform_create_window(const char* title, int width, int height) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return NULL;
    }

    // Request GL 4.1 core profile - matches the macOS layer's target for parity.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* glfw_window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!glfw_window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return NULL;
    }

    glfwMakeContextCurrent(glfw_window);

    glewExperimental = GL_TRUE; // required for core profile on some drivers
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        glfwDestroyWindow(glfw_window);
        glfwTerminate();
        return NULL;
    }

    glfwSetKeyCallback(glfw_window, glfw_key_callback);
    glfwSetMouseButtonCallback(glfw_window, glfw_mouse_button_callback);
    glfwSetFramebufferSizeCallback(glfw_window, glfw_framebuffer_size_callback);

    // Initialize tracked size immediately so it's correct before any resize happens.
    glfwGetFramebufferSize(glfw_window, &current_width, &current_height);

    PlatformWindow* window = malloc(sizeof(PlatformWindow));
    window->glfw_window = glfw_window;
    window->width = width;
    window->height = height;
    window->last_time = glfwGetTime();
    window->delta_time = 0.0f;

    printf("Platform initialized: OpenGL %s, GLFW %s\n",
           glGetString(GL_VERSION),
           glfwGetVersionString());

    return window;
}

void platform_destroy_window(PlatformWindow* window) {
    if (window) {
        glfwDestroyWindow(window->glfw_window);
        glfwTerminate();
        free(window);
    }
}

void platform_set_window_title(PlatformWindow* window, const char* title) {
    if (window && window->glfw_window) {
        glfwSetWindowTitle(window->glfw_window, title);
    }
}

void platform_get_window_size(PlatformWindow* window, int* width, int* height) {
    *width = current_width;
    *height = current_height;
}

int platform_should_close(PlatformWindow* window) {
    if (window && window->glfw_window) {
        return glfwWindowShouldClose(window->glfw_window);
    }
    return 1;
}

void platform_poll_events(void) {
    memcpy(previous_key_state, current_key_state, sizeof(current_key_state));
    memcpy(previous_mouse_state, current_mouse_state, sizeof(current_mouse_state));

    glfwPollEvents();
}

void platform_swap_buffers(PlatformWindow* window) {
    if (window && window->glfw_window) {
        glfwSwapBuffers(window->glfw_window);
    }
}

int platform_key_pressed(PlatformWindow* window, int key) {
    if (key >= 0 && key < 512) {
        return (current_key_state[key] == 1 && previous_key_state[key] == 0);
    }
    return 0;
}

int platform_key_held(PlatformWindow* window, int key) {
    if (key >= 0 && key < 512) {
        return current_key_state[key] == 1;
    }
    return 0;
}

int platform_key_released(PlatformWindow* window, int key) {
    if (key >= 0 && key < 512) {
        return (current_key_state[key] == 0 && previous_key_state[key] == 1);
    }
    return 0;
}

void platform_get_mouse_position(PlatformWindow* window, double* x, double* y) {
    if (window && window->glfw_window) {
        glfwGetCursorPos(window->glfw_window, x, y);
    } else {
        *x = 0;
        *y = 0;
    }
}

int platform_mouse_button_pressed(PlatformWindow* window, int button) {
    if (button >= 0 && button < 8) {
        return (current_mouse_state[button] == 1 && previous_mouse_state[button] == 0);
    }
    return 0;
}

int platform_mouse_button_released(PlatformWindow* window, int button) {
    if (button >= 0 && button < 8) {
        return (current_mouse_state[button] == 0 && previous_mouse_state[button] == 1);
    }
    return 0;
}

void platform_set_mouse_locked(PlatformWindow* window, int locked) {
    if (window && window->glfw_window) {
        glfwSetInputMode(window->glfw_window, GLFW_CURSOR,
            locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}

double platform_get_time(void) {
    return glfwGetTime();
}

float platform_get_delta_time(void) {
    static double last_time = 0.0;
    double current_time = glfwGetTime();
    float delta = (float)(current_time - last_time);
    last_time = current_time;

    if (delta > 0.1f) delta = 0.1f;
    if (delta < 0.0f) delta = 0.0f;

    return delta;
}

void* platform_get_gl_context(PlatformWindow* window) {
    if (window && window->glfw_window) {
        return window->glfw_window;
    }
    return NULL;
}

void platform_set_resize_callback(PlatformWindow* window, void (*callback)(int, int)) {
    glfwSetFramebufferSizeCallback(window->glfw_window,
        (void(*)(GLFWwindow*, int, int))callback);
}

#endif // defined(_WIN32)