#ifndef ARCHIVE_ORG_H
#define ARCHIVE_ORG_H

#include "ui.h" // For UIState
#include "config.h"
#include "repository.h"
#include <switch.h>

// Estructura para respuestas de Archive.org
typedef struct {
    char identifier[256];
    char title[512];
    char description[1024];
    int total_items;
} ArchiveOrgResponse;

// Funciones para integración con Archive.org
int archive_org_init(void);
void archive_org_exit(void);

// Buscar un repositorio en Archive.org
int archive_org_search(const char *url, ArchiveOrgResponse *response);

// Obtener lista de archivos desde URL y ruta dentro del repositorio
int archive_org_list_files(const char *base_url, const char *path, Repository *repo);

// Descargar archivo desde URL
int archive_org_download_file(const char *base_url, const char *filename, const char *output_path, UIState *ui_state, RepositoryManager *manager, AppConfig *config);

// Funciones HTTP básicas
int http_get(const char *url, char **response, size_t *response_size);

#endif
