#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PlatformWindow PlatformWindow;

// Window
PlatformWindow* platform_create_window(const char* title, int width, int height);
void platform_destroy_window(PlatformWindow* window);
void platform_set_window_title(PlatformWindow* window, const char* title);
void platform_get_window_size(PlatformWindow* window, int* width, int* height);

// Main loop
int platform_should_close(PlatformWindow* window);
void platform_poll_events(void);
void platform_swap_buffers(PlatformWindow* window);

// Keyboard
int platform_key_pressed(PlatformWindow* window, int key);
int platform_key_held(PlatformWindow* window, int key);
int platform_key_released(PlatformWindow* window, int key);

// Mouse
void platform_get_mouse_position(PlatformWindow* window, double* x, double* y);
int platform_mouse_button_pressed(PlatformWindow* window, int button);
int platform_mouse_button_released(PlatformWindow* window, int button);
void platform_set_mouse_locked(PlatformWindow* window, int locked);

// Time
double platform_get_time(void);
float platform_get_delta_time(void);

void* platform_get_gl_context(PlatformWindow* window);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_H