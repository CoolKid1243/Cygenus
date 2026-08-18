#ifndef WIN_DIRENT_H
#define WIN_DIRENT_H

#if defined(_WIN32)

#include <windows.h>
#include <string.h>
#include <stdlib.h>

struct dirent {
    char d_name[MAX_PATH];
};

typedef struct DIR {
    HANDLE handle;
    WIN32_FIND_DATAA find_data;
    int first_read;
    struct dirent entry;
} DIR;

static DIR* opendir(const char* path) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    DIR* dir = (DIR*)malloc(sizeof(DIR));
    if (!dir) return NULL;

    dir->handle = FindFirstFileA(search_path, &dir->find_data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }

    dir->first_read = 1;
    return dir;
}

static struct dirent* readdir(DIR* dir) {
    if (!dir) return NULL;

    if (dir->first_read) {
        dir->first_read = 0;
    } else {
        if (!FindNextFileA(dir->handle, &dir->find_data)) {
            return NULL;
        }
    }

    snprintf(dir->entry.d_name, sizeof(dir->entry.d_name), "%s", dir->find_data.cFileName);
    return &dir->entry;
}

static void closedir(DIR* dir) {
    if (!dir) return;
    if (dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
    }
    free(dir);
}

#endif // defined(_WIN32)

#endif // WIN_DIRENT_H