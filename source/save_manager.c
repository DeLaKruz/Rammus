#include "save_manager.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <SDL2/SDL_image.h>

// Crea directorios de forma recursiva (estilo mkdir -p)
static void util_mkdir(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = 0;
    if (len < 7) return; // Ignorar si es menor a "sdmc:/"
    for (char *p = tmp + 7; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

// Asegura que la ruta empiece por sdmc:/ y esté limpia
void consolidate_path(const char *in, char *out, size_t out_size) {
    if (strncmp(in, "sdmc:/", 6) == 0) {
        strncpy(out, in, out_size - 1);
    } else {
        const char *p = (in[0] == '/') ? in + 1 : in;
        snprintf(out, out_size, "sdmc:/%s", p);
    }
    out[out_size - 1] = '\0';
}

SaveManager* save_manager_create(void) {
    SaveManager *mgr = malloc(sizeof(SaveManager));
    if (mgr) {
        mgr->entries = NULL;
        mgr->count = 0;
        mgr->user_count = 0;
        mgr->current_user_index = 0;
        mgr->restore_entry = NULL;
        mgr->restore_folders = NULL;
        mgr->restore_count = 0;
        mgr->restore_selected = 0;
        nsInitialize();
        accountInitialize(AccountServiceType_Administrator);

        // Añadir el usuario "Consola" para datos compartidos/dispositivo
        memset(&mgr->users[0].uid, 0, sizeof(AccountUid));
        strcpy(mgr->users[0].name, "Consola");
        mgr->users[0].icon = NULL;
        mgr->user_count = 1;

        // Cargar usuarios del sistema
        AccountUid uids[9];
        s32 actual_users = 0;
        if (R_SUCCEEDED(accountListAllUsers(uids, 9, &actual_users))) {
            for (int i = 0; i < actual_users && mgr->user_count < 10; i++) {
                int idx = mgr->user_count;
                mgr->users[idx].uid = uids[i];
                mgr->users[idx].icon = NULL;
                
                AccountProfile profile;
                if (R_SUCCEEDED(accountGetProfile(&profile, uids[i]))) {
                    AccountProfileBase profile_base;
                    // Pass NULL for AccountUserData if not needed, get profile_base for nickname
                    if (R_SUCCEEDED(accountProfileGet(&profile, NULL, &profile_base))) {
                        strncpy(mgr->users[idx].name, profile_base.nickname, sizeof(mgr->users[idx].name) - 1);
                        mgr->users[idx].name[sizeof(mgr->users[idx].name) - 1] = '\0';
                        
                        // Limpiar espacios y puntos al final del nombre de usuario
                        size_t ulen = strlen(mgr->users[idx].name);
                        while (ulen > 0 && (mgr->users[idx].name[ulen-1] == ' ' || mgr->users[idx].name[ulen-1] == '.')) {
                            mgr->users[idx].name[--ulen] = '\0';
                        }
                    }
                    
                    // Cargar icono de usuario
                    u32 icon_size = 0;
                    accountProfileGetImageSize(&profile, &icon_size);
                    if (icon_size > 0) {
                        void* icon_buf = malloc(icon_size);
                        u32 decoded_size = 0;
                        if (R_SUCCEEDED(accountProfileLoadImage(&profile, icon_buf, icon_size, &decoded_size))) {
                            // Nota: En una implementación real aquí decodificarías el JPEG del icono
                            // Por ahora usaremos el nombre para identificarlo
                        }
                        free(icon_buf);
                    }
                    accountProfileClose(&profile);
                }
                mgr->user_count++;
            }
        }
    }
    return mgr;
}

void save_manager_destroy(SaveManager *mgr, SDL_Renderer *renderer) {
    if (!mgr) return;
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->entries[i].icon) SDL_DestroyTexture(mgr->entries[i].icon);
    }
    if (mgr->restore_folders) {
        for (int i = 0; i < mgr->restore_count; i++) free(mgr->restore_folders[i]);
        free(mgr->restore_folders);
    }
    if (mgr->entries) free(mgr->entries);
    nsExit();
    accountExit();
    free(mgr);
}

static SDL_Texture* load_icon(SDL_Renderer *renderer, u64 title_id) {
    NsApplicationControlData controlData;
    size_t outSize;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, title_id, &controlData, sizeof(controlData), &outSize);
    
    if (R_SUCCEEDED(rc)) {
        // El icono suele estar después de las estructuras de nombres en el NACP
        SDL_RWops *rw = SDL_RWFromMem(controlData.icon, sizeof(controlData.icon));
        SDL_Texture *tex = IMG_LoadTexture_RW(renderer, rw, 1);
        return tex;
    }
    return NULL;
}

static void get_game_name(u64 title_id, char *out_name) {
    NsApplicationControlData controlData;
    size_t outSize;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, title_id, &controlData, sizeof(controlData), &outSize);
    
    if (R_SUCCEEDED(rc)) {
        // JKSV itera sobre los 16 idiomas posibles para encontrar el nombre
        out_name[0] = '\0';
        for (int i = 0; i < 16; i++) {
            if (controlData.nacp.lang[i].name[0] != '\0') {
                // Copiar y detenerse si encontramos un nombre
                strncpy(out_name, controlData.nacp.lang[i].name, 511); 
                break;
            }
        }
        if (out_name[0] == '\0') snprintf(out_name, 512, "%016lX", title_id);
    } else {
        snprintf(out_name, 512, "%016lX", title_id);
    }
    
    // Eliminar espacios y puntos al final (no permitidos en carpetas FAT32)
    size_t nlen = strlen(out_name);
    while (nlen > 0 && (out_name[nlen-1] == ' ' || out_name[nlen-1] == '.')) {
        out_name[--nlen] = '\0';
    }
    out_name[511] = '\0';
}

int save_manager_rescan(SaveManager *mgr, SDL_Renderer *renderer) {
    // Limpiar entradas previas
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->entries[i].icon) SDL_DestroyTexture(mgr->entries[i].icon);
    }
    if (mgr->entries) free(mgr->entries);
    mgr->entries = NULL;
    mgr->count = 0;

    FsSaveDataInfoReader reader; // Usar FsSaveDataInfoReader
    FsSaveDataInfo info; // Para leer la información
    AccountUid current_uid = mgr->users[mgr->current_user_index].uid;
    int is_console_mode = (mgr->current_user_index == 0);
    
    // JKSV escanea múltiples espacios. Vamos a escanear memoria de usuario y SD.
    FsSaveDataSpaceId spaces[] = {FsSaveDataSpaceId_User, FsSaveDataSpaceId_SdUser};
    
    for (int s = 0; s < 2; s++) {
        Result rc = fsOpenSaveDataInfoReader(&reader, spaces[s]);
        if (R_FAILED(rc)) continue;

        s64 read_count = 0;
        while (R_SUCCEEDED(fsSaveDataInfoReaderRead(&reader, &info, 1, &read_count)) && read_count > 0) {
            if (is_console_mode) {
                // En modo Consola, solo mostramos saves de tipo Device
                if (info.save_data_type != FsSaveDataType_Device) continue;
            } else {
                // En modo Usuario, solo mostramos saves de Cuenta que le pertenezcan
                if (info.save_data_type != FsSaveDataType_Account) continue;
                if (memcmp(&info.uid, &current_uid, sizeof(AccountUid)) != 0) continue;
            }

            mgr->entries = realloc(mgr->entries, sizeof(SaveEntry) * (mgr->count + 1));
            SaveEntry *e = &mgr->entries[mgr->count];
            
            e->title_id = info.application_id;
            e->user_id = info.uid;
            e->info = info; // Guardar la info completa
            get_game_name(e->title_id, e->name);
            e->icon = load_icon(renderer, e->title_id);
            
            mgr->count++;
        }
        fsSaveDataInfoReaderClose(&reader);
    }

    return 0;
}

static void copy_dir_recursive(FsFileSystem *fs, const char *src_path, const char *dst_path) {
    FsDir dir;
    // Intentar abrir el directorio. Si falla, salimos.
    if (R_FAILED(fsFsOpenDirectory(fs, src_path, FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &dir))) {
        // Algunos títulos no aceptan "/" pero sí "" o viceversa
        return;
    }

    mkdir(dst_path, 0777); // Asegurar que el subdirectorio existe

    s64 count = 0;
    FsDirectoryEntry entry;
    while (R_SUCCEEDED(fsDirRead(&dir, &count, 1, &entry)) && count > 0) {
        char new_src[1024], new_dst[1024];
        
        // Evitar doble barra al principio (//) que rompe la lectura en Switch
        if (strcmp(src_path, "/") == 0) {
            snprintf(new_src, 1024, "/%s", entry.name);
        } else {
            snprintf(new_src, 1024, "%s/%s", src_path, entry.name);
        }
        
        snprintf(new_dst, 1024, "%s/%s", dst_path, entry.name);

        if (entry.type == FsDirEntryType_Dir) {
            copy_dir_recursive(fs, new_src, new_dst);
        } else {
            FsFile f_in;
            if (R_SUCCEEDED(fsFsOpenFile(fs, new_src, FsOpenMode_Read, &f_in))) {
                FILE *f_out = fopen(new_dst, "wb");
                if (f_out) {
                    u8 *buf = malloc(0x10000); // Buffer de 64KB
                    u64 offset = 0, bytes_read = 0;
                    while (R_SUCCEEDED(fsFileRead(&f_in, offset, buf, 0x10000, FsReadOption_None, &bytes_read)) && bytes_read > 0) {
                        fwrite(buf, 1, bytes_read, f_out);
                        offset += bytes_read;
                    }
                    free(buf);
                    fclose(f_out);
                }
                fsFileClose(&f_in);
            }
        }
    }
    fsDirClose(&dir);
}

// Borra contenido de la consola de forma recursiva para una restauración limpia
static void delete_dir_recursive(FsFileSystem *fs, const char *path) {
    FsDir dir;
    if (R_FAILED(fsFsOpenDirectory(fs, path, FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &dir))) return;
    s64 count = 0;
    FsDirectoryEntry entry;
    while (R_SUCCEEDED(fsDirRead(&dir, &count, 1, &entry)) && count > 0) {
        char full_path[1024];
        if (strcmp(path, "/") == 0) snprintf(full_path, 1024, "/%s", entry.name);
        else snprintf(full_path, 1024, "%s/%s", path, entry.name);
        if (entry.type == FsDirEntryType_Dir) {
            delete_dir_recursive(fs, full_path);
            fsFsDeleteDirectory(fs, full_path);
        } else {
            fsFsDeleteFile(fs, full_path);
        }
    }
    fsDirClose(&dir);
}

static void util_rmdir_recursive(const char *path) {
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *p;
    while ((p = readdir(d))) {
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/%s", path, p->d_name);
        struct stat st;
        if (!stat(buf, &st)) {
            if (S_ISDIR(st.st_mode)) util_rmdir_recursive(buf);
            else remove(buf);
        }
    }
    closedir(d);
    rmdir(path);
}

int save_manager_delete_backup(const char *path) {
    util_rmdir_recursive(path);
    fsdevCommitDevice("sdmc");
    return 0;
}

static void util_copy_sd_to_sd(const char *src, const char *dst) {
    mkdir(dst, 0777);
    DIR *d = opendir(src);
    if (!d) return;
    struct dirent *p;
    while ((p = readdir(d))) {
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
        char s_buf[1024], d_buf[1024];
        snprintf(s_buf, sizeof(s_buf), "%s/%s", src, p->d_name);
        snprintf(d_buf, sizeof(d_buf), "%s/%s", dst, p->d_name);
        struct stat st;
        if (!stat(s_buf, &st)) {
            if (S_ISDIR(st.st_mode)) util_copy_sd_to_sd(s_buf, d_buf);
            else {
                FILE *fin = fopen(s_buf, "rb");
                FILE *fout = fopen(d_buf, "wb");
                if (fin && fout) {
                    void *buf = malloc(0x10000);
                    size_t read;
                    while ((read = fread(buf, 1, 0x10000, fin)) > 0) fwrite(buf, 1, read, fout);
                    free(buf);
                }
                if (fin) fclose(fin);
                if (fout) fclose(fout);
            }
        }
    }
    closedir(d);
}

int save_manager_duplicate_backup(const char *src_path) {
    char dst_path[1024];
    struct stat st;

    snprintf(dst_path, sizeof(dst_path), "%s - copia", src_path);
    if (stat(dst_path, &st) == 0) {
        int count = 2;
        while (1) {
            snprintf(dst_path, sizeof(dst_path), "%s - copia %d", src_path, count);
            if (stat(dst_path, &st) != 0) break;
            count++;
        }
    }

    util_copy_sd_to_sd(src_path, dst_path);
    fsdevCommitDevice("sdmc");
    return 0;
}

// Copia de SD a Consola
static void copy_sd_to_save_recursive(FsFileSystem *fs, const char *src_path, const char *dst_path) {
    DIR *dir = opendir(src_path);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".nx_save_meta.bin") == 0) continue;
        char new_src[1024], new_dst[1024];
        snprintf(new_src, 1024, "%s/%s", src_path, entry->d_name);
        snprintf(new_dst, 1024, "%s/%s", dst_path, entry->d_name);
        struct stat st;
        if (stat(new_src, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                fsFsCreateDirectory(fs, new_dst);
                copy_sd_to_save_recursive(fs, new_src, new_dst);
            } else {
                FILE *f_in = fopen(new_src, "rb");
                if (f_in) {
                    fsFsDeleteFile(fs, new_dst);
                    fsFsCreateFile(fs, new_dst, st.st_size, 0);
                    FsFile f_out;
                    if (R_SUCCEEDED(fsFsOpenFile(fs, new_dst, FsOpenMode_Write, &f_out))) {
                        u8 *buf = malloc(0x10000);
                        size_t bytes_read; u64 offset = 0;
                        while ((bytes_read = fread(buf, 1, 0x10000, f_in)) > 0) {
                            fsFileWrite(&f_out, offset, buf, bytes_read, FsWriteOption_None);
                            offset += bytes_read;
                        }
                        free(buf); fsFileClose(&f_out);
                    }
                    fclose(f_in);
                }
            }
        }
    }
    closedir(dir);
}

void save_manager_prepare_restore(SaveManager *mgr, int index, const char *root_path) {
    if (index < 0 || index >= mgr->count) return;
    if (mgr->restore_folders) {
        for (int i = 0; i < mgr->restore_count; i++) free(mgr->restore_folders[i]);
        free(mgr->restore_folders);
    }
    mgr->restore_folders = NULL; mgr->restore_count = 0; mgr->restore_selected = 0;
    mgr->restore_entry = &mgr->entries[index];
    char safe_root[PATH_MAX], game_dir[PATH_MAX];
    consolidate_path(root_path, safe_root, sizeof(safe_root));
    snprintf(game_dir, PATH_MAX, "%s/%s", safe_root, mgr->restore_entry->name);
    for(char *p = game_dir + 7; *p; p++) if (strchr(":*?\"<>|", *p)) *p = '_';
    DIR *dir = opendir(game_dir);
    if (dir) {
        struct dirent *entry;
        const char *current_user_name = mgr->users[mgr->current_user_index].name;
        size_t user_name_len = strlen(current_user_name);

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;

            // Solo listar carpetas que empiecen por "NombreUsuario_"
            if (strncmp(entry->d_name, current_user_name, user_name_len) != 0 || entry->d_name[user_name_len] != '_') {
                continue;
            }

            mgr->restore_folders = realloc(mgr->restore_folders, sizeof(char*) * (mgr->restore_count + 1));
            mgr->restore_folders[mgr->restore_count++] = strdup(entry->d_name);
        }
        closedir(dir);
    }
}

int save_manager_restore(SaveEntry *entry, const char *sd_folder_path, struct UIState *ui_state) {
    FsFileSystem save_fs; FsSaveDataAttribute attr; memset(&attr, 0, sizeof(attr));
    attr.application_id = entry->title_id; attr.uid = entry->user_id;
    attr.save_data_type = entry->info.save_data_type; attr.save_data_rank = entry->info.save_data_rank; attr.save_data_index = entry->info.save_data_index;
    if (R_FAILED(fsOpenSaveDataFileSystem(&save_fs, (FsSaveDataSpaceId)entry->info.save_data_space_id, &attr))) return -1;
    ui_state->is_downloading = 1; strcpy(ui_state->download_status, "Restaurando Save Data...");
    delete_dir_recursive(&save_fs, "/"); copy_sd_to_save_recursive(&save_fs, sd_folder_path, "/");
    fsFsCommit(&save_fs); fsFsClose(&save_fs); ui_state->is_downloading = 0; return 0;
}

int save_manager_backup(SaveEntry *entry, const char *root_path, struct UIState *ui_state) {
    FsFileSystem save_fs;
    FsSaveDataAttribute attr;
    memset(&attr, 0, sizeof(attr));
    attr.application_id = entry->title_id;
    attr.uid = entry->user_id;
    attr.save_data_type = entry->info.save_data_type;
    attr.save_data_rank = entry->info.save_data_rank;
    attr.save_data_index = entry->info.save_data_index;

    // Usamos el ID de espacio que el sistema nos devolvió al escanear (lo más seguro)
    FsSaveDataSpaceId space = (FsSaveDataSpaceId)entry->info.save_data_space_id;
    Result rc = fsOpenSaveDataFileSystem(&save_fs, space, &attr);
    if (R_FAILED(rc)) return -1;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d_%02dH%02d", 
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);

    char safe_root[PATH_MAX];
    consolidate_path(root_path, safe_root, PATH_MAX);

    char game_dir[PATH_MAX];
    snprintf(game_dir, PATH_MAX, "%s/%s", safe_root, entry->name);
    // Limpiar caracteres prohibidos en FAT32 (empezando después de sdmc:/)
    for(char *p = game_dir + 7; *p; p++) if (strchr(":*?\"<>|", *p)) *p = '_';

    char final_path[PATH_MAX]; // Usar PATH_MAX
    snprintf(final_path, PATH_MAX, "%s/%s_%s", game_dir, ui_state->save_mgr->users[ui_state->save_mgr->current_user_index].name, timestamp);
    for(char *p = final_path + 7; *p; p++) if (strchr(":*?\"<>|", *p)) *p = '_';

    // Crear jerarquía de carpetas recursivamente para evitar fallos si no existen
    util_mkdir(safe_root);
    util_mkdir(game_dir);
    util_mkdir(final_path);
    fsdevCommitDevice("sdmc"); // Asegurar que los directorios existan para fopen

    ui_state->is_downloading = 1;
    strcpy(ui_state->download_status, "Copiando Save Data...");
    
    // Crear el archivo de metadatos para compatibilidad con JKSV
    char meta_path[PATH_MAX];
    snprintf(meta_path, sizeof(meta_path), "%s/.nx_save_meta.bin", final_path);
    FILE *f_meta = fopen(meta_path, "wb");
    if (f_meta) {
        fwrite(&entry->info, 1, sizeof(entry->info), f_meta); // Guardar la FsSaveDataInfo completa
        fclose(f_meta);
        fsdevCommitDevice("sdmc"); // Forzar la escritura del archivo de metadatos
    }

    // Intentar copia desde "/" y si falla (común en algunos ports oficiales), intentar desde ""
    copy_dir_recursive(&save_fs, "/", final_path);
    // Si la carpeta quedó vacía, podríamos intentar copy_dir_recursive(&save_fs, "", final_path);
    // pero "/" suele ser lo correcto si el sistema de archivos montó bien.
    
    // IMPORTANTE: Confirmar los cambios en la tarjeta SD
    fsdevCommitDevice("sdmc");
    
    fsFsClose(&save_fs);
    
    ui_state->is_downloading = 0;
    return 0;
}