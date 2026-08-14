#ifndef PROJECT_H
#define PROJECT_H

#ifdef __cplusplus
extern "C" {
#endif

void project_set_root(const char* root_path);
void project_get_path(const char* relative_path, char* out_buffer, int buffer_size);

#ifdef __cplusplus
}
#endif

#endif // PROJECT_H