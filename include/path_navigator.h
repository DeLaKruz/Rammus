#ifndef PATH_NAVIGATOR_H
#define PATH_NAVIGATOR_H

#include <switch.h>

typedef struct {
    char current_path[512];
    char **entries;
    int entry_count;
    int selected_entry;
} PathNavigator;

// Función de utilidad definida en main.h
// int ensure_sdmc_mounted(void); // Ya no es necesaria aquí, se incluye en main.h

// Crear/destruir navegador
PathNavigator* path_navigator_create(const char *initial_path);
void path_navigator_destroy(PathNavigator *nav);

// Navegar por carpetas
int path_navigator_enter_folder(PathNavigator *nav, const char *folder_name);
int path_navigator_go_back(PathNavigator *nav);
int path_navigator_refresh(PathNavigator *nav);

// Crear carpeta nueva
int path_navigator_create_folder(PathNavigator *nav, const char *folder_name);

// Getters
const char* path_navigator_get_current(PathNavigator *nav);
const char* path_navigator_get_selected_entry(PathNavigator *nav);
char** path_navigator_get_entries(PathNavigator *nav);
int path_navigator_get_entry_count(PathNavigator *nav);

// Controles
void path_navigator_move_selection(PathNavigator *nav, int direction);

#endif
