#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[512];
    char path[1024];
    char size_text[32]; // Añadido para almacenar el tamaño formateado
    int is_dir;
} RepositoryItem;

typedef struct {
    char name[512];
    char id[1024];
    char download_path[512]; // Nueva ruta de descarga vinculada
    int item_count;
    RepositoryItem *items;
} Repository;

typedef struct {
    int repo_count;
    Repository *repositories;
} RepositoryManager;

// Funciones para gestionar repositorios
RepositoryManager* repository_manager_create(void);
void repository_manager_destroy(RepositoryManager *manager);
void repository_add(RepositoryManager *manager, const char *name, const char *id, const char *download_path);
Repository* repository_get(RepositoryManager *manager, int index);
void repository_add_item(Repository *repo, const char *name, const char *path, int is_dir);
void repository_clear_items(Repository *repo);
void repository_sort_items(Repository *repo);
int repository_populate_by_url(Repository *repo);

#endif
