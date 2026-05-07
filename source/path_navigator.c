#include "path_navigator.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void normalize_sdmc_path(const char *path, char *out, size_t size) {
    if (!out || size == 0) return;

    if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
        strncpy(out, "sdmc:/", size - 1);
        out[size - 1] = '\0';
        return;
    }

    // Si ya empieza con sdmc:/, lo mantenemos pero limpiamos posibles dobles barras
    if (strncmp(path, "sdmc:", 5) == 0) {
        const char *p = path + 5;
        while (*p == '/') p++;
        snprintf(out, size, "sdmc:/%s", p[0] == '\0' ? "" : p);
    } else {
        // Si es una ruta absoluta sin prefijo, añadimos sdmc:/
        const char *p = path;
        while (*p == '/') p++;
        snprintf(out, size, "sdmc:/%s", p[0] == '\0' ? "" : p);
    }

    // Eliminar barra final si existe (excepto en raíz)
    size_t len = strlen(out);
    if (len > 6 && out[len - 1] == '/') {
        out[len - 1] = '\0';
    }
}

static DIR *open_sdmc_dir(const char *path) {
    char fs_path[512];
    normalize_sdmc_path(path, fs_path, sizeof(fs_path));

    DIR *dir = opendir(fs_path);
    return dir;
}

static int is_directory(const char *path) {
    char fs_path[512];
    normalize_sdmc_path(path, fs_path, sizeof(fs_path));
    
    struct stat st;
    if (stat(fs_path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return 0;
}

static int is_root_path(const char *path) {
    char normalized[512];
    normalize_sdmc_path(path, normalized, sizeof(normalized));
    return strcmp(normalized, "sdmc:/") == 0;
}

static int path_navigator_load_entries(PathNavigator *nav) {
    // 1. Limpiar entradas anteriores de forma segura
    if (nav->entries) {
        for (int i = 0; i < nav->entry_count; i++) {
            if (nav->entries[i]) free(nav->entries[i]);
        }
        free(nav->entries);
        nav->entries = NULL;
    }
    nav->entry_count = 0;
    
    // 2. Intentar abrir el directorio
    DIR *dir = open_sdmc_dir(nav->current_path);
    if (!dir) {
        ensure_sdmc_mounted();
        dir = open_sdmc_dir(nav->current_path);
    }

    if (!dir) {
        // Si falla, al menos permitimos volver atrás si no es raíz
        if (!is_root_path(nav->current_path)) {
            nav->entries = malloc(sizeof(char*));
            nav->entries[0] = strdup("..");
            nav->entry_count = 1;
        }
        nav->selected_entry = 0;
        return -1;
    }

    // 3. Cargar entradas en una lista dinámica temporal para evitar doble lectura
    char **temp_entries = malloc(sizeof(char*) * 1024); 
    int count = 0;

    if (!is_root_path(nav->current_path)) {
        temp_entries[count++] = strdup("..");
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char full_entry_path[1024];
            
            // Evitar dobles barras sdmc:// al concatenar con la raíz
            if (is_root_path(nav->current_path)) {
                snprintf(full_entry_path, sizeof(full_entry_path), "sdmc:/%s", entry->d_name);
            } else {
                snprintf(full_entry_path, sizeof(full_entry_path), "%s/%s", nav->current_path, entry->d_name);
            }
            
            // Solo añadir a la lista si es un directorio
            if (is_directory(full_entry_path)) {
                temp_entries[count++] = strdup(entry->d_name);
                if (count >= 1024) break; // Límite de seguridad
            }
        }
    }
    closedir(dir);
    
    nav->entries = malloc(sizeof(char*) * count);
    memcpy(nav->entries, temp_entries, sizeof(char*) * count);
    nav->entry_count = count;
    free(temp_entries);

    nav->selected_entry = 0;
    return 0;
}

PathNavigator* path_navigator_create(const char *initial_path) {
    PathNavigator *nav = (PathNavigator*)malloc(sizeof(PathNavigator));
    if (nav) {
        ensure_sdmc_mounted();
        normalize_sdmc_path(initial_path, nav->current_path, sizeof(nav->current_path));

        nav->entries = NULL;
        nav->entry_count = 0;
        nav->selected_entry = 0;
        path_navigator_load_entries(nav);
    }
    return nav;
}

void path_navigator_destroy(PathNavigator *nav) {
    if (nav) {
        if (nav->entries) {
            for (int i = 0; i < nav->entry_count; i++) {
                free(nav->entries[i]);
            }
            free(nav->entries);
        }
        free(nav);
    }
}

int path_navigator_enter_folder(PathNavigator *nav, const char *folder_name) {
    if (!nav || !folder_name) return -1;
    
    if (strcmp(folder_name, "..") == 0) {
        return path_navigator_go_back(nav);
    }
    
    if (strcmp(folder_name, "[Nueva carpeta]") == 0) {
        return -1;  // No entrar, es opción especial
    }
    
    // Construir nueva ruta
    char new_path[512];
    strncpy(new_path, nav->current_path, sizeof(new_path) - 1);
    new_path[sizeof(new_path) - 1] = '\0';
    if (new_path[strlen(new_path) - 1] != '/') {
        strncat(new_path, "/", sizeof(new_path) - strlen(new_path) - 1);
    }
    strncat(new_path, folder_name, sizeof(new_path) - strlen(new_path) - 1);
    normalize_sdmc_path(new_path, new_path, sizeof(new_path));
    
    // Verificar que existe usando la API
    if (is_directory(new_path)) {
        strncpy(nav->current_path, new_path, sizeof(nav->current_path) - 1);
        nav->current_path[sizeof(nav->current_path) - 1] = '\0';
        path_navigator_load_entries(nav);
        return 0;
    }
    
    return -1;
}

int path_navigator_go_back(PathNavigator *nav) {
    if (!nav) return -1;
    
    // No ir más atrás de raíz
    if (is_root_path(nav->current_path)) {
        return 0;
    }
    
    // Remover última carpeta
    int len = strlen(nav->current_path) - 1;
    if (nav->current_path[len] == '/') len--;
    
    while (len > 0 && nav->current_path[len] != '/') {
        len--;
    }
    
    if (len > 0) {
        nav->current_path[len + 1] = '\0';
    } else {
        // Si estamos en raíz, usar la ruta raíz de sdmc
        strcpy(nav->current_path, "sdmc:/");
    }
    
    path_navigator_load_entries(nav);
    return 0;
}

int path_navigator_refresh(PathNavigator *nav) {
    if (!nav) return -1;
    return path_navigator_load_entries(nav);
}

int path_navigator_create_folder(PathNavigator *nav, const char *folder_name) {
    if (!nav || !folder_name) return -1;
    
    char full_path[512];
    strncpy(full_path, nav->current_path, sizeof(full_path) - 1);
    full_path[sizeof(full_path) - 1] = '\0';
    if (full_path[strlen(full_path) - 1] != '/') {
        strncat(full_path, "/", sizeof(full_path) - strlen(full_path) - 1);
    }
    strncat(full_path, folder_name, sizeof(full_path) - strlen(full_path) - 1);

    char fs_path[512];
    normalize_sdmc_path(full_path, fs_path, sizeof(fs_path));
    if (mkdir(fs_path, 0777) != 0) {
        return -1;
    }
    path_navigator_load_entries(nav);
    return 0;
}

const char* path_navigator_get_current(PathNavigator *nav) {
    if (!nav) return NULL;
    return nav->current_path;
}

const char* path_navigator_get_selected_entry(PathNavigator *nav) {
    if (!nav || nav->selected_entry < 0 || nav->selected_entry >= nav->entry_count) {
        return NULL;
    }
    return nav->entries[nav->selected_entry];
}

char** path_navigator_get_entries(PathNavigator *nav) {
    if (!nav) return NULL;
    return nav->entries;
}

int path_navigator_get_entry_count(PathNavigator *nav) {
    if (!nav) return 0;
    return nav->entry_count;
}

void path_navigator_move_selection(PathNavigator *nav, int direction) {
    if (!nav) return;
    
    nav->selected_entry += direction;
    
    if (nav->selected_entry < 0) {
        nav->selected_entry = 0;
    }
    if (nav->selected_entry >= nav->entry_count) {
        nav->selected_entry = nav->entry_count - 1;
    }
}
