#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void ensure_config_dir_exists(void) {
    mkdir(CONFIG_DIR, 0777);
}

AppConfig* config_create(void) {
    AppConfig *config = (AppConfig*)malloc(sizeof(AppConfig));
    if (config) {
        strcpy(config->download_path, "sdmc:/");
        config->repo_count = 0;
        config->custom_repos = NULL;
        ensure_config_dir_exists();
    }
    return config;
}

void config_destroy(AppConfig *config) {
    if (config) {
        for (int i = 0; i < config->repo_count; i++) {
            if (config->custom_repos[i].items) {
                free(config->custom_repos[i].items);
            }
        }
        if (config->custom_repos) {
            free(config->custom_repos);
        }
        free(config);
    }
}

int config_load(AppConfig *config) {
    if (!config) return -1;
    
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) {
        // Archivo no existe, usar defaults
        strcpy(config->download_path, "sdmc:/");
        config->repo_count = 0;
        return 0;  // No es error, es primer uso
    }
    
    char line[1024];
    
    // Leer ruta de descarga
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, "download_path=%511s", config->download_path);
    }
    
    // Leer cantidad de repositorios
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, "repo_count=%d", &config->repo_count);
    }
    
    if (config->repo_count > 0) {
        config->custom_repos = (Repository*)malloc(sizeof(Repository) * config->repo_count);
        
        for (int i = 0; i < config->repo_count; i++) {
            // Leer nombre
            if (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    strncpy(config->custom_repos[i].name, eq + 1, sizeof(config->custom_repos[i].name) - 1);
                    config->custom_repos[i].name[strcspn(config->custom_repos[i].name, "\n")] = 0;
                }
            }
            
            // Leer URL/ID
            if (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    strncpy(config->custom_repos[i].id, eq + 1, sizeof(config->custom_repos[i].id) - 1);
                    config->custom_repos[i].id[strcspn(config->custom_repos[i].id, "\n")] = 0;
                }
            }

            // Leer Ruta de descarga del repo
            if (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    strncpy(config->custom_repos[i].download_path, eq + 1, sizeof(config->custom_repos[i].download_path) - 1);
                    config->custom_repos[i].download_path[strcspn(config->custom_repos[i].download_path, "\n")] = 0;
                }
            }
            
            config->custom_repos[i].item_count = 0;
            config->custom_repos[i].items = NULL;
        }
    }
    
    fclose(f);
    return 0;
}

int config_save(AppConfig *config) {
    if (!config) return -1;
    
    ensure_config_dir_exists();
    
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) {
        printf("Error: No se pudo crear archivo de configuración\n");
        return -1;
    }
    
    fprintf(f, "download_path=%s\n", config->download_path);
    fprintf(f, "repo_count=%d\n", config->repo_count);
    
    for (int i = 0; i < config->repo_count; i++) {
        fprintf(f, "repo_name=%s\n", config->custom_repos[i].name);
        fprintf(f, "repo_url=%s\n", config->custom_repos[i].id);
        fprintf(f, "repo_path=%s\n", config->custom_repos[i].download_path);
    }
    
    fclose(f);
    return 0;
}

void config_set_download_path(AppConfig *config, const char *path) {
    if (!config || !path) return;
    strncpy(config->download_path, path, sizeof(config->download_path) - 1);
    config->download_path[sizeof(config->download_path) - 1] = '\0';
}

const char* config_get_download_path(AppConfig *config) {
    if (!config) return NULL;
    return config->download_path;
}

void config_add_custom_repo(AppConfig *config, const char *name, const char *archive_url, const char *download_path) {
    if (!config || !name || !archive_url) return;
    
    Repository *new_repos = (Repository*)realloc(config->custom_repos,
                                                 sizeof(Repository) * (config->repo_count + 1));
    if (new_repos) {
        config->custom_repos = new_repos;
        Repository *repo = &config->custom_repos[config->repo_count];
        
        strncpy(repo->name, name, sizeof(repo->name) - 1);
        repo->name[sizeof(repo->name) - 1] = '\0';
        
        strncpy(repo->id, archive_url, sizeof(repo->id) - 1);
        repo->id[sizeof(repo->id) - 1] = '\0';
        
        strncpy(repo->download_path, download_path, sizeof(repo->download_path) - 1);
        repo->download_path[sizeof(repo->download_path) - 1] = '\0';

        repo->item_count = 0;
        repo->items = NULL;
        
        config->repo_count++;
    }
}

Repository* config_get_custom_repo(AppConfig *config, int index) {
    if (!config || index < 0 || index >= config->repo_count) {
        return NULL;
    }
    return &config->custom_repos[index];
}

int config_get_repo_count(AppConfig *config) {
    if (!config) return 0;
    return config->repo_count;
}

void config_remove_custom_repo(AppConfig *config, int index) {
    if (!config || index < 0 || index >= config->repo_count) return;
    
    // Liberar el repo a eliminar
    if (config->custom_repos[index].items) {
        free(config->custom_repos[index].items);
    }
    
    // Desplazar repos posteriores
    for (int i = index; i < config->repo_count - 1; i++) {
        config->custom_repos[i] = config->custom_repos[i + 1];
    }
    
    config->repo_count--;
    
    // Redimensionar array
    if (config->repo_count > 0) {
        Repository *new_repos = (Repository*)realloc(config->custom_repos,
                                                     sizeof(Repository) * config->repo_count);
        if (new_repos) {
            config->custom_repos = new_repos;
        }
    } else {
        free(config->custom_repos);
        config->custom_repos = NULL;
    }
}
