#ifndef DOWNLOADER_H
#define DOWNLOADER_H

typedef struct {
    int total_bytes;
    int downloaded_bytes;
    int percentage;
    char status[256];
} DownloadProgress;

// Callbacks para progreso
typedef void (*download_progress_callback)(DownloadProgress *progress, void *userdata);

// Descargar archivo de Archive.org
int downloader_fetch_file(const char *url, const char *output_path, 
                         download_progress_callback callback, void *userdata);

// Descargar metadata JSON
char* downloader_fetch_json(const char *url);

// Cancelar descarga actual
void downloader_cancel(void);

#endif
