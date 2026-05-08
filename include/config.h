#ifndef CONFIG_H
#define CONFIG_H

#include "repository.h"

#define CONFIG_PATH "sdmc:/switch/archive_navigator/config.json"
#define CONFIG_DIR  "sdmc:/switch/archive_navigator/"

typedef struct {
    char download_path[512];
    char save_backup_path[512]; // Nueva ruta para copias de guardado
    int repo_count;
    Repository *custom_repos;
} AppConfig;

// Funciones de configuración
AppConfig* config_create(void);
void config_destroy(AppConfig *config);

// Cargar/guardar configuración persistente
int config_load(AppConfig *config);
int config_save(AppConfig *config);

// Getters/Setters
void config_set_download_path(AppConfig *config, const char *path);
const char* config_get_download_path(AppConfig *config);
void config_set_save_backup_path(AppConfig *config, const char *path);
const char* config_get_save_backup_path(AppConfig *config);

// Gestión de repositorios personalizados
void config_add_custom_repo(AppConfig *config, const char *name, const char *archive_url, const char *download_path);
Repository* config_get_custom_repo(AppConfig *config, int index);
int config_get_repo_count(AppConfig *config);
void config_remove_custom_repo(AppConfig *config, int index);

#endif
