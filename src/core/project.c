#include "project.h"
#include <stdio.h>
#include <string.h>

static char project_root[256] = "projects/sample_project";

void project_set_root(const char* root_path) {
    strncpy(project_root, root_path, sizeof(project_root) - 1); // strncopy sets a limmit on how long the input can be.
}

void project_get_path(const char* relative_path, char* out_buffer, int buffer_size) {
    snprintf(out_buffer, buffer_size, "%s/%s", project_root, relative_path);
}