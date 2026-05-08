#include "archive_org.h"
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <curl/curl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h> // For strtod
#include "ui.h"   // For UIState and progress updates

#define HTTP_BUFFER_SIZE 8192
#define HTTP_MAX_REDIRECTS 3

static int socket_initialized = 0;

int archive_org_init(void)
{
    if (!socket_initialized)
    {
        socketInitializeDefault();
        curl_global_init(CURL_GLOBAL_DEFAULT);
        socket_initialized = 1;
    }
    return 0;
}

void archive_org_exit(void)
{
    if (socket_initialized)
    {
        curl_global_cleanup();
        socketExit();
        socket_initialized = 0;
    }
}

typedef struct
{
    char *data;
    size_t size;
} MemoryStruct;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr)
        return 0;
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}

typedef struct
{
    FILE *stream;
    UIState *ui_state;
    RepositoryManager *repo_manager;
    AppConfig *config;
    double total_to_download;
    double downloaded_now;
} ProgressData;

static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    ProgressData *progress_data = (ProgressData *)clientp;
    if (progress_data->ui_state)
    {
        progress_data->ui_state->download_progress = 0.0;
        if (dltotal > 0)
        {
            progress_data->ui_state->download_progress = (double)dlnow / (double)dltotal;
        }
        progress_data->downloaded_now = (double)dlnow;
        progress_data->total_to_download = (double)dltotal;
        snprintf(progress_data->ui_state->download_status, sizeof(progress_data->ui_state->download_status), "%.2f / %.2f MB", progress_data->downloaded_now / (1024.0 * 1024.0), progress_data->total_to_download / (1024.0 * 1024.0));

        // Redibujar el fondo para evitar parpadeos (epilepsia)
        SDL_SetRenderDrawColor(progress_data->ui_state->renderer, 45, 45, 45, 255);
        SDL_RenderClear(progress_data->ui_state->renderer);
        
        if (progress_data->ui_state->mode == UI_MODE_COPIAS) { // Redibujar la UI de saves si estamos en ese modo
            ui_draw_saves_menu(progress_data->ui_state);
        } else if (progress_data->ui_state->mode == UI_MODE_SAVE_PATH_PICKER) {
            ui_draw_path_picker(progress_data->ui_state, progress_data->config);
        } else if (progress_data->ui_state->mode == UI_MODE_BROWSER) {
            Repository *repo = repository_get(progress_data->repo_manager, progress_data->ui_state->selected_repo);
            ui_draw_repository_browser(repo, progress_data->ui_state, progress_data->config);
        } else {
            ui_draw_main_menu(progress_data->repo_manager, progress_data->ui_state, progress_data->config);
        }

        ui_draw_progress(progress_data->ui_state);
        SDL_RenderPresent(progress_data->ui_state->renderer);
    }
    return 0; // Devolver 0 para continuar la transferencia
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

int http_get(const char *url, char **response, size_t *response_size)
{
    CURL *curl_handle = curl_easy_init();
    if (!curl_handle)
        return -1;

    MemoryStruct chunk = {malloc(1), 0};

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ArchiveNavigator/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl_handle);
    long http_code = 0;
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK || http_code != 200)
    {
        free(chunk.data);
        curl_easy_cleanup(curl_handle);
        return -1;
    }

    *response = chunk.data;
    *response_size = chunk.size;
    curl_easy_cleanup(curl_handle);
    return 0;
}

static void strip_query_string(char *path)
{
    char *q = strchr(path, '?');
    if (q)
        *q = '\0';
}

static int extract_archive_org_id(const char *url, char *id, size_t size)
{
    const char *patterns[] = {"/download/", "/details/", "/metadata/"};
    for (int i = 0; i < 3; i++)
    {
        const char *pos = strstr(url, patterns[i]);
        if (pos)
        {
            pos += strlen(patterns[i]);
            size_t len = 0;
            while (pos[len] && pos[len] != '/' && pos[len] != '?' && pos[len] != '&' && pos[len] != '#')
            {
                len++;
            }
            if (len == 0 || len >= size)
                return -1;
            strncpy(id, pos, len);
            id[len] = '\0';
            return 0;
        }
    }

    if (strlen(url) < size && strchr(url, '/') == NULL && strchr(url, ':') == NULL)
    {
        strcpy(id, url);
        return 0;
    }

    return -1;
}

static int already_has_item(Repository *repo, const char *name)
{
    if (!repo || !name)
        return 0;
    for (int i = 0; i < repo->item_count; i++)
    {
        if (strcmp(repo->items[i].name, name) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int get_filename_from_href(const char *href, char *filename, size_t size)
{
    if (!href || !filename || size == 0)
        return -1;
    const char *p = href + strlen(href);
    while (p > href && p[-1] != '/' && p[-1] != '\\')
    {
        p--;
    }
    if (*p == '\0')
        return -1;
    size_t len = strlen(p);
    if (len == 0 || len >= size)
        return -1;
    strncpy(filename, p, size - 1);
    filename[size - 1] = '\0';
    strip_query_string(filename);
    if (strchr(filename, '.') == NULL)
        return -1;
    return 0;
}

static int normalize_archive_org_download_base(const char *url, char *download_base, size_t size)
{
    const char *download_pos = strstr(url, "/download/");
    if (download_pos)
    {
        const char *id_start = download_pos + strlen("/download/");
        const char *slash_pos = strchr(id_start, '/');
        size_t id_len = slash_pos ? (size_t)(slash_pos - id_start) : strlen(id_start);
        if (id_len == 0 || id_len + 25 >= size)
            return -1;
        snprintf(download_base, size, "https://archive.org/download/%.*s", (int)id_len, id_start);
        return 0;
    }

    const char *details_pos = strstr(url, "/details/");
    if (details_pos)
    {
        details_pos += strlen("/details/");
        const char *slash_pos = strchr(details_pos, '/');
        size_t id_len = slash_pos ? (size_t)(slash_pos - details_pos) : strlen(details_pos);
        if (id_len == 0 || id_len + 25 >= size)
            return -1;
        snprintf(download_base, size, "https://archive.org/download/%.*s", (int)id_len, details_pos);
        return 0;
    }

    const char *metadata_pos = strstr(url, "/metadata/");
    if (metadata_pos)
    {
        metadata_pos += strlen("/metadata/");
        const char *slash_pos = strchr(metadata_pos, '/');
        size_t id_len = slash_pos ? (size_t)(slash_pos - metadata_pos) : strlen(metadata_pos);
        if (id_len == 0 || id_len + 25 >= size)
            return -1;
        snprintf(download_base, size, "https://archive.org/download/%.*s", (int)id_len, metadata_pos);
        return 0;
    }

    return -1;
}

static int is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static int url_encode_path(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0)
        return -1;

    size_t dst_idx = 0;
    for (size_t i = 0; src[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == '/')
        {
            if (dst_idx + 1 >= dst_size)
                return -1;
            dst[dst_idx++] = c;
        }
        else if (c == '%' && is_hex_digit(src[i + 1]) && is_hex_digit(src[i + 2]))
        {
            if (dst_idx + 3 >= dst_size)
                return -1;
            dst[dst_idx++] = src[i++];
            dst[dst_idx++] = src[i++];
            dst[dst_idx++] = src[i];
        }
        else
        {
            if (dst_idx + 3 >= dst_size)
                return -1;
            static const char hex[] = "0123456789ABCDEF";
            dst[dst_idx++] = '%';
            dst[dst_idx++] = hex[c >> 4];
            dst[dst_idx++] = hex[c & 0xF];
        }
    }
    if (dst_idx >= dst_size)
        return -1;
    dst[dst_idx] = '\0';
    return 0;
}

// Buscar repositorio (placeholder)
int archive_org_search(const char *url, ArchiveOrgResponse *response)
{
    char identifier[256];
    if (extract_archive_org_id(url, identifier, sizeof(identifier)) != 0)
    {
        return -1;
    }

    if (strlen(identifier) >= sizeof(response->identifier))
    {
        return -1;
    }

    strcpy(response->identifier, identifier);
    strcpy(response->title, response->identifier);
    response->total_items = 0;
    return 0;
}

static void clean_json_path(char *path)
{
    if (!path)
        return;
    char *src = path, *dst = path;
    while (*src)
    {
        if (*src == '\\' && (*(src + 1) == '/' || *(src + 1) == '\\' || *(src + 1) == '\"'))
        {
            src++; // Saltar la barra invertida de escape
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}

static void sanitize_switch_filename(char *filename)
{
    if (!filename)
        return;
    const char *forbidden = ":*?\"<>|";
    for (char *p = filename; *p; p++)
    {
        if (strchr(forbidden, *p))
        {
            *p = '_';
        }
    }
}

static void format_bytes(char *dest, size_t size, double bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    while (bytes >= 1024 && i < 4)
    {
        bytes /= 1024;
        i++;
    }
    snprintf(dest, size, "%.2f %s", bytes, units[i]);
}

static void add_metadata_item(Repository *repo, const char *name, const char *current_path, const char *size_raw)
{
    char relative[1024];
    const char *target_name = name;
    char formatted_size[32] = {0};
    size_t name_len = strlen(name);

    if (strcmp(current_path, "/") == 0)
    {
        strncpy(relative, target_name, sizeof(relative) - 1);
        relative[sizeof(relative) - 1] = '\0';
    }
    else
    {
        size_t prefix_len = strlen(current_path) - 1;
        if (name_len <= prefix_len || strncmp(target_name, current_path + 1, prefix_len) != 0 || target_name[prefix_len] != '/')
        {
            return;
        }
        strncpy(relative, target_name + prefix_len + 1, sizeof(relative) - 1);
        relative[sizeof(relative) - 1] = '\0';
    }

    if (relative[0] == '\0')
        return;

    char *slash = strchr(relative, '/');
    if (slash)
    {
        size_t dir_len = slash - relative;
        char dir_name[512];
        if (dir_len >= sizeof(dir_name))
            return;
        strncpy(dir_name, relative, dir_len);
        dir_name[dir_len] = '\0';

        char dir_path[1024];
        if (strcmp(current_path, "/") == 0)
        {
            snprintf(dir_path, sizeof(dir_path), "%s", dir_name);
        }
        else
        {
            snprintf(dir_path, sizeof(dir_path), "%s/%s", current_path + 1, dir_name);
        }

        if (!already_has_item(repo, dir_name))
        {
            repository_add_item(repo, dir_name, dir_path, 1);
        }
    }
    else
    {
        if (!already_has_item(repo, relative))
        {
            char file_path[1024];
            if (strcmp(current_path, "/") == 0)
            {
                snprintf(file_path, sizeof(file_path), "%s", relative);
            }
            else
            {
                size_t prefix_len = strlen(current_path + 1);
                size_t relative_len = strlen(relative);
                if (prefix_len + 1 + relative_len >= sizeof(file_path))
                {
                    return;
                }
                memcpy(file_path, current_path + 1, prefix_len);
                file_path[prefix_len] = '/';
                memcpy(file_path + prefix_len + 1, relative, relative_len);
                file_path[prefix_len + 1 + relative_len] = '\0';
            }

            repository_add_item(repo, relative, file_path, 0);
            if (size_raw && size_raw[0] != '\0')
            {
                format_bytes(formatted_size, sizeof(formatted_size), strtod(size_raw, NULL));
                snprintf(repo->items[repo->item_count - 1].size_text,
                         sizeof(repo->items[repo->item_count - 1].size_text),
                         "%s", formatted_size);
            }
        }
    }
}

static int parse_archive_org_metadata_files(const char *json, const char *current_path, Repository *repo)
{
    const char *files_array_start = strstr(json, "\"files\":");
    if (!files_array_start)
        return -1;
    files_array_start = strchr(files_array_start, '[');
    if (!files_array_start)
        return -1;
    files_array_start++; // Moverse más allá de '['

    int found = 0;
    const char *current_obj_start = files_array_start;

    while ((current_obj_start = strchr(current_obj_start, '{')) != NULL)
    {
        current_obj_start++; // Moverse más allá de '{'
        const char *current_obj_end = strchr(current_obj_start, '}');
        if (!current_obj_end)
            break; // Fin del array de archivos o JSON mal formado

        // Extraer filename
        const char *name_key_start = strstr(current_obj_start, "\"name\":");
        if (!name_key_start || name_key_start > current_obj_end)
        {
            current_obj_start = current_obj_end + 1;
            continue;
        }
        const char *name_val_start = strchr(name_key_start + 7, '"');
        if (!name_val_start || name_val_start > current_obj_end)
        {
            current_obj_start = current_obj_end + 1;
            continue;
        }
        name_val_start++; // Moverse más allá de la comilla de apertura
        const char *name_val_end = strchr(name_val_start, '"');
        if (!name_val_end || name_val_end > current_obj_end)
        {
            current_obj_start = current_obj_end + 1;
            continue;
        }

        char filename_buffer[512];
        size_t name_len = name_val_end - name_val_start;
        if (name_len >= sizeof(filename_buffer))
        {
            current_obj_start = current_obj_end + 1;
            continue;
        }
        strncpy(filename_buffer, name_val_start, name_len);
        filename_buffer[name_len] = '\0';
        clean_json_path(filename_buffer);
        sanitize_switch_filename(filename_buffer);
        strip_query_string(filename_buffer);

        // Extraer size
        const char *size_key_start = strstr(current_obj_start, "\"size\":");
        char size_val_buffer[64] = {0};
        if (size_key_start && size_key_start < current_obj_end)
        {
            const char *size_num_start = size_key_start + strlen("\"size\":");
            // Saltar espacios y comillas (muy importante para Archive.org)
            while (*size_num_start == ' ' || *size_num_start == '\t' || *size_num_start == '\"')
                size_num_start++;
            const char *size_num_end = size_num_start;
            // Asegurarse de que el tamaño sea un número válido
            while (isdigit((unsigned char)*size_num_end) || *size_num_end == '.')
                size_num_end++;

            size_t size_len = size_num_end - size_num_start;
            if (size_len > 0 && size_len < sizeof(size_val_buffer))
            {
                strncpy(size_val_buffer, size_num_start, size_len);
                size_val_buffer[size_len] = '\0';
            }
        }

        add_metadata_item(repo, filename_buffer, current_path, size_val_buffer);
        found++;

        current_obj_start = current_obj_end + 1; // Moverse al siguiente carácter después de '}'
    }

    return repo->item_count > 0 ? 0 : -1;
}

// Listar archivos usando metadata de Archive.org o HTML como respaldo
static void normalize_device_path(char *out, size_t size, const char *path);
static int is_http_url(const char *url);
static int is_local_fs_path(const char *path);
static int copy_file(const char *src_path, const char *dst_path);
static int build_local_source_path(char *out, size_t size, const char *base_url, const char *filename);

static int archive_org_list_local_files(const char *base_url, const char *path, Repository *repo)
{
    char source_dir[1024];
    if (path && path[0] != '\0' && strcmp(path, "/") != 0)
    {
        if (build_local_source_path(source_dir, sizeof(source_dir), base_url, path) != 0)
        {
            return -1;
        }
    }
    else
    {
        if (snprintf(source_dir, sizeof(source_dir), "%s", base_url) >= sizeof(source_dir))
        {
            return -1;
        }
    }

    char normalized_source_dir[1024];
    normalize_device_path(normalized_source_dir, sizeof(normalized_source_dir), source_dir);

    DIR *dir = opendir(normalized_source_dir);
    if (!dir)
    {
        return -1;
    }

    repository_clear_items(repo);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char item_path[512];
        if (path && strcmp(path, "/") != 0)
        {
            snprintf(item_path, sizeof(item_path), "%s/%s", path, entry->d_name);
        }
        else
        {
            snprintf(item_path, sizeof(item_path), "/%s", entry->d_name);
        }

        int is_dir = 0;
#if defined(DT_UNKNOWN)
        if (entry->d_type != DT_UNKNOWN)
        {
            is_dir = (entry->d_type == DT_DIR);
        }
        else
        {
#endif
            char test_path[1024];
            if (build_local_source_path(test_path, sizeof(test_path), normalized_source_dir, entry->d_name) == 0)
            {
                struct stat st;
                if (stat(test_path, &st) == 0 && S_ISDIR(st.st_mode))
                {
                    is_dir = 1;
                }
            }
#if defined(DT_UNKNOWN)
        }
#endif

        repository_add_item(repo, entry->d_name, item_path, is_dir);
    }

    closedir(dir);
    if (repo->item_count > 0)
    {
        repository_sort_items(repo);
    }
    return repo->item_count > 0 ? 0 : -1;
}

int archive_org_list_files(const char *base_url, const char *path, Repository *repo)
{
    if (is_local_fs_path(base_url))
    {
        return archive_org_list_local_files(base_url, path, repo);
    }

    char identifier[256];
    if (extract_archive_org_id(base_url, identifier, sizeof(identifier)) != 0)
    {
        printf("Error: No se pudo extraer ID de %s\n", base_url);
        return -1;
    }

    char metadata_url[512];
    snprintf(metadata_url, sizeof(metadata_url), "https://archive.org/metadata/%s", identifier);

    printf("Cargando metadata de: %s\n", identifier);

    repository_clear_items(repo);

    char *json;
    size_t json_size;
    if (http_get(metadata_url, &json, &json_size) == 0)
    {
        if (parse_archive_org_metadata_files(json, path, repo) == 0)
        {
            free(json);
            repository_sort_items(repo);
            return 0;
        }
        free(json);
    }

    char url_buffer[512];
    const char *url = NULL;
    if (strncmp(base_url, "http://", 7) == 0)
    {
        url = base_url;
    }
    else if (strncmp(base_url, "https://", 8) == 0)
    {
        snprintf(url_buffer, sizeof(url_buffer), "http://%s", base_url + 8);
        url = url_buffer;
    }
    else if (strchr(base_url, '/') != NULL)
    {
        if (strlen(base_url) + 8 < sizeof(url_buffer))
        {
            snprintf(url_buffer, sizeof(url_buffer), "http://%s", base_url);
            url = url_buffer;
        }
    }

    char *html;
    size_t html_size;
    if (!url || http_get(url, &html, &html_size) != 0)
    {
        return -1;
    }

    char *pos = html;
    while ((pos = strstr(pos, "href=")) != NULL)
    {
        pos += 5;
        char quote = *pos;
        if (quote != '"' && quote != '\'')
        {
            pos++;
            continue;
        }
        pos++;

        char *end = strchr(pos, quote);
        if (!end)
            break;

        char href[512];
        size_t href_len = end - pos;
        if (href_len >= sizeof(href))
        {
            pos = end + 1;
            continue;
        }

        strncpy(href, pos, href_len);
        href[href_len] = '\0';
        pos = end + 1;

        if (strcmp(href, "../") == 0 || href[0] == '#')
        {
            continue;
        }

        char filename[256];
        int valid = 0;
        if (strncmp(href, "/download/", 10) == 0 || strncmp(href, "download/", 9) == 0)
        {
            if (get_filename_from_href(href, filename, sizeof(filename)) == 0)
            {
                valid = 1;
            }
        }
        else if (strncmp(href, "http://archive.org/download/", 28) == 0 || strncmp(href, "https://archive.org/download/", 29) == 0)
        {
            if (get_filename_from_href(href, filename, sizeof(filename)) == 0)
            {
                valid = 1;
            }
        }

        if (valid && !already_has_item(repo, filename))
        {
            // Construir el path relativo desde la carpeta actual
            char full_path[512];
            if (path && strcmp(path, "/") != 0)
            {
                snprintf(full_path, sizeof(full_path), "%s/%s", path, filename);
            }
            else
            {
                snprintf(full_path, sizeof(full_path), "%s", filename);
            }
            repository_add_item(repo, filename, full_path, 0);
        }
    }

    free(html);
    if (repo->item_count > 0)
    {
        repository_sort_items(repo);
    }
    return repo->item_count > 0 ? 0 : -1;
}

static void normalize_device_path(char *out, size_t size, const char *path)
{
    if (!out || size == 0 || !path)
        return;

    const char *p = path;
    size_t idx = 0;

    // Preserve prefix until ':' if present
    while (*p && *p != ':')
    {
        if (idx + 1 < size)
            out[idx++] = *p++;
        else
            break;
    }
    if (*p == ':')
    {
        if (idx + 1 < size)
            out[idx++] = *p++;
    }

    // Asegurar sdmc:/
    if (idx + 1 < size)
        out[idx++] = '/';
    while (*p == '/')
        p++;

    int slash_written = 1;
    while (*p && idx + 1 < size)
    {
        if (*p == '/')
        {
            if (!slash_written || out[idx - 1] != '/')
            {
                out[idx++] = '/';
            }
            slash_written = 1;
            p++;
            while (*p == '/')
                p++;
            continue;
        }
        slash_written = 0;
        out[idx++] = *p++;
    }

    if (idx >= size)
        idx = size - 1;
    out[idx] = '\0';
}

static int is_http_url(const char *url)
{
    return url && (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0);
}

static int is_local_fs_path(const char *path)
{
    if (!path)
        return 0;
    if (is_http_url(path))
        return 0;
    if (path[0] == '/')
        return 1;
    return strchr(path, ':') != NULL;
}

static int copy_file(const char *src_path, const char *dst_path)
{
    FILE *src = fopen(src_path, "rb");
    if (!src)
        return -1;

    FILE *dst = fopen(dst_path, "wb");
    if (!dst)
    {
        fclose(src);
        return -1;
    }

    char buffer[8192];
    size_t read_bytes;
    int result = 0;

    while ((read_bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        if (fwrite(buffer, 1, read_bytes, dst) != read_bytes)
        {
            result = -1;
            break;
        }
    }

    fclose(src);
    fclose(dst);

    if (result != 0)
    {
        unlink(dst_path);
    }
    return result;
}

static int build_local_source_path(char *out, size_t size, const char *base_url, const char *filename)
{
    if (!out || size == 0 || !base_url || !filename)
        return -1;

    const char *rel = filename;
    while (*rel == '/')
    {
        rel++;
    }
    if (*rel == '\0')
        return -1;

    size_t base_len = strlen(base_url);
    if (base_len > 0 && base_url[base_len - 1] == '/')
    {
        if (snprintf(out, size, "%s%s", base_url, rel) >= size)
            return -1;
    }
    else
    {
        if (snprintf(out, size, "%s/%s", base_url, rel) >= size)
            return -1;
    }
    return 0;
}

static int create_parent_directories(const char *path)
{
    if (!path)
        return -1;

    char tmp[1024];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    // Buscamos el último slash para no crear el archivo como carpeta
    char *last_slash = strrchr(tmp, '/');
    if (last_slash)
        *last_slash = '\0';
    else
        return 0;

    // Empezamos después de "sdmc:/" (6 caracteres)
    for (p = tmp + 6; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
    return 0;
}

// Descargar archivo
int archive_org_download_file(const char *base_url, const char *filename, const char *output_path, UIState *ui_state, RepositoryManager *manager, AppConfig *config)
{
    if (is_local_fs_path(base_url))
    {
        char normalized_filename[1024];
        if (filename && filename[0] == '/')
        {
            strncpy(normalized_filename, filename + 1, sizeof(normalized_filename) - 1);
        }
        else
        {
            strncpy(normalized_filename, filename ? filename : "", sizeof(normalized_filename) - 1);
        }
        normalized_filename[sizeof(normalized_filename) - 1] = '\0';

        if (create_parent_directories(output_path) != 0)
        {
            return -1;
        }

        char source_path[1024];
        if (build_local_source_path(source_path, sizeof(source_path), base_url, normalized_filename) != 0)
        {
            return -1;
        }
        return copy_file(source_path, output_path);
    }

    // Lógica de descarga HTTP
    char download_base[2048] = {0};
    if (normalize_archive_org_download_base(base_url, download_base, sizeof(download_base)) != 0)
    {
        if (base_url && strchr(base_url, '/') == NULL)
        {
            snprintf(download_base, sizeof(download_base), "http://archive.org/download/%s", base_url);
        }
        else
        {
            return -1;
        }
    }

    char normalized_filename[1024];
    if (filename && filename[0] == '/')
    {
        strncpy(normalized_filename, filename + 1, sizeof(normalized_filename) - 1);
    }
    else
    {
        strncpy(normalized_filename, filename ? filename : "", sizeof(normalized_filename) - 1);
    }
    normalized_filename[sizeof(normalized_filename) - 1] = '\0';

    char encoded_filename[2048];
    if (url_encode_path(normalized_filename, encoded_filename, sizeof(encoded_filename)) != 0)
    {
        return -1;
    }

    char full_url[4096];
    if (snprintf(full_url, sizeof(full_url), "%s/%s", download_base, encoded_filename) >= sizeof(full_url))
    {
        return -1;
    }

    create_parent_directories(output_path);

    // Configurar CURL y callback de progreso
    CURL *curl_handle = curl_easy_init();
    if (!curl_handle)
        return -1;

    FILE *fp = fopen(output_path, "wb");
    if (!fp)
    {
        curl_easy_cleanup(curl_handle);
        return -1;
    }

    ProgressData progress_data = {fp, ui_state, manager, config, 0.0, 0.0};

    curl_easy_setopt(curl_handle, CURLOPT_URL, full_url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ArchiveNavigator/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);                      // Habilitar la función de progreso
    curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback); // Usar XFERINFOFUNCTION para progreso
    curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &progress_data);

    ui_state->is_downloading = 1; // Establecer flag de descarga en el UIState
    CURLcode res = curl_easy_perform(curl_handle);
    long http_code = 0;
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);

    fclose(fp);
    curl_easy_cleanup(curl_handle);

    ui_state->is_downloading = 0; // Quitar la barra de progreso al terminar
    if (res != CURLE_OK || http_code != 200)
    {
        unlink(output_path);
        return -1;
    }
    return 0; // Descarga exitosa
}
