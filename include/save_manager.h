#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#include <switch.h>
#include <SDL2/SDL.h>

struct UIState;

// Asegura que la ruta empiece por sdmc:/ y esté limpia
void consolidate_path(const char *in, char *out, size_t out_size);

typedef struct {
    AccountUid uid;
    char name[33];
    SDL_Texture *icon;
} UserEntry;

typedef struct {
    u64 title_id;
    AccountUid user_id;
    char name[512];
    SDL_Texture *icon;
    FsSaveDataInfo info; // Revertido a FsSaveDataInfo
} SaveEntry;

typedef struct {
    SaveEntry *entries;
    int count;
    
    UserEntry users[10]; // Aumentado para Consola + Usuarios del sistema
    int user_count;
    int current_user_index;

    // Para el menú de restauración
    SaveEntry *restore_entry;
    char **restore_folders;
    int restore_count;
    int restore_selected;
} SaveManager;

SaveManager* save_manager_create(void);
void save_manager_destroy(SaveManager *mgr, SDL_Renderer *renderer);

// Escanea la consola buscando saves e iconos
int save_manager_rescan(SaveManager *mgr, SDL_Renderer *renderer);

// Realiza el backup al estilo JKSV
int save_manager_backup(SaveEntry *entry, const char *root_path, struct UIState *ui_state);

// Prepara la lista de carpetas para restaurar
void save_manager_prepare_restore(SaveManager *mgr, int index, const char *root_path);
// Realiza la restauración desde una carpeta de la SD
int save_manager_restore(SaveEntry *entry, const char *sd_folder_path, struct UIState *ui_state);
// Elimina un backup de la SD
int save_manager_delete_backup(const char *path);
// Duplica un backup en la SD
int save_manager_duplicate_backup(const char *src_path);

#endif