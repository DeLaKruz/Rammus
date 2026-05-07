#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include <switch.h>

#include "repository.h"
#include "ui.h"
#include "config.h"
#include "archive_org.h"

int ensure_sdmc_mounted(void) {
    DIR *dir = opendir("sdmc:/");
    if (dir) {
        closedir(dir);
        return 0;
    }

    Result rc = fsdevMountSdmc();
    if (R_FAILED(rc)) {
        printf("Advertencia: no se pudo montar sdmc:/ (0x%08x)\n", (unsigned int)rc);
        return -1;
    }

    return 0;
}

static void add_persistent_repos(RepositoryManager *manager, AppConfig *config) {
    if (!manager || !config) return;
    for (int i = 0; i < config_get_repo_count(config); i++) {
        Repository *saved = config_get_custom_repo(config, i);
        if (saved) {
            repository_add(manager, saved->name, saved->id);
        }
    }
}

static void append_to_message(char *dest, size_t dest_size, const char *src) {
    size_t len = strlen(dest);
    if (len + 1 >= dest_size) return;
    size_t avail = dest_size - len - 1;
    size_t copy_len = strlen(src);
    if (copy_len > avail) copy_len = avail;
    memcpy(dest + len, src, copy_len);
    dest[len + copy_len] = '\0';
}

static void join_sdmc_path(char *out, size_t out_size, const char *base, const char *relative) {
    if (!out || out_size == 0) return;
    if (!base) base = "";
    if (!relative) relative = "";

    // Asegurar que la base tenga el prefijo correcto y no termine en slash
    char clean_base[1024] = {0};
    if (strncmp(base, "sdmc:/", 6) != 0) {
        const char *p = (base[0] == '/') ? base + 1 : base;
        snprintf(clean_base, sizeof(clean_base), "sdmc:/%s", p);
    } else {
        strncpy(clean_base, base, sizeof(clean_base) - 1);
        clean_base[sizeof(clean_base) - 1] = '\0';
    }

    size_t blen = strlen(clean_base);
    if (blen > 6 && clean_base[blen-1] == '/') clean_base[blen-1] = '\0';

    const char *rel = relative;
    while (*rel == '/') rel++;

    // Evitar doble barra si la base es exactamente "sdmc:/"
    if (strcmp(clean_base, "sdmc:/") == 0) {
        snprintf(out, out_size, "sdmc:/%s", rel);
    } else {
        snprintf(out, out_size, "%s/%s", clean_base, rel);
    }
}

int main(int argc, char **argv)
{
    consoleInit(NULL);

    // Intentar montar sdmc:/ si no está ya accesible
    ensure_sdmc_mounted();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    // Inicializar Archive.org
    if (archive_org_init() != 0) {
        printf("Error inicializando Archive.org\n");
        consoleUpdate(NULL);
        return -1;
    }

    AppConfig *config = config_create();
    config_load(config);

    RepositoryManager *manager = repository_manager_create();
    add_persistent_repos(manager, config);

    UIState *ui_state = ui_state_create();
    ui_state->current_path[0] = '/';
    ui_state->current_path[1] = '\0';

    int running = 1;

    while (appletMainLoop() && running)
    {
        padUpdate(&pad);
        UIInput input = ui_handle_input(&pad);

        if (ui_state->show_message) {
            ui_draw_message(ui_state);
            if (input.action == UI_ACTION_BACK) {
                ui_state->show_message = 0;
                // Permanecer en el modo actual en lugar de volver al menú principal
                if (ui_state->mode == UI_MODE_MAIN) ui_state->mode = UI_MODE_MAIN; 
            }
            consoleUpdate(NULL);
            continue;
        }

        switch (ui_state->mode) {
            case UI_MODE_MAIN: {
                if (input.action == UI_ACTION_EXIT) {
                    running = 0;
                } else if (input.action == UI_ACTION_SETTINGS) {
                    ui_state->mode = UI_MODE_SETTINGS;
                } else if (input.action == UI_ACTION_SELECT) {
                    if (input.param == 1) {
                        ui_state->selected_repo++;
                        if (ui_state->selected_repo >= manager->repo_count) {
                            ui_state->selected_repo = manager->repo_count - 1;
                        }
                    } else if (input.param == -1) {
                        ui_state->selected_repo--;
                        if (ui_state->selected_repo < 0) {
                            ui_state->selected_repo = 0;
                        }
                    } else {
                        if (manager->repo_count > 0) {
                            ui_state->mode = UI_MODE_BROWSER;
                            ui_state->selected_item = 0;
                            ui_state->browser_scroll_offset = 0;
                            ui_state->current_path[0] = '/';
                            ui_state->current_path[1] = '\0';
                            printf("Conectando con Archive.org...\n");
                            Repository *repo = repository_get(manager, ui_state->selected_repo);
                            if (repo) {
                                // Listar la ruta raíz del repositorio
                                archive_org_list_files(repo->id, ui_state->current_path, repo);
                            }
                        }
                    }
                }
                ui_draw_main_menu(manager, ui_state, config);
                break;
            }
            case UI_MODE_BROWSER: {
                Repository *current_repo = repository_get(manager, ui_state->selected_repo);
                if (!current_repo) {
                    ui_state->mode = UI_MODE_MAIN;
                    break;
                }

                if (input.action == UI_ACTION_SETTINGS) {
                    ui_state->mode = UI_MODE_SETTINGS;
                } else if (input.action == UI_ACTION_BACK) {
                    if (strcmp(ui_state->current_path, "/") != 0) {
                        // Volver un nivel en la ruta dentro del repositorio
                        int len = strlen(ui_state->current_path);
                        if (ui_state->current_path[len - 1] == '/') {
                            ui_state->current_path[len - 1] = '\0';
                            len--;
                        }
                        while (len > 0 && ui_state->current_path[len - 1] != '/') {
                            ui_state->current_path[--len] = '\0';
                        }
                        if (len == 0) {
                            ui_state->current_path[0] = '/';
                            ui_state->current_path[1] = '\0';
                        }
                        Repository *current_repo = repository_get(manager, ui_state->selected_repo);
                        if (current_repo) {
                            archive_org_list_files(current_repo->id, ui_state->current_path, current_repo);
                            ui_state->selected_item = 0;
                            ui_state->browser_scroll_offset = 0;
                        }
                    } else {
                        ui_state->mode = UI_MODE_MAIN;
                    }
                } else if (input.action == UI_ACTION_SELECT) {
                    if (input.param == 1) {
                        ui_state->selected_item++;
                        if (ui_state->selected_item >= current_repo->item_count) {
                            ui_state->selected_item = current_repo->item_count - 1;
                        }
                    } else if (input.param == -1) {
                        ui_state->selected_item--;
                        if (ui_state->selected_item < 0) {
                            ui_state->selected_item = 0;
                        }
                    } else {
                        if (current_repo->item_count > 0) {
                            RepositoryItem *selected = &current_repo->items[ui_state->selected_item];
                            if (selected->is_dir) {
                                char old_path[1024];
                                strncpy(old_path, ui_state->current_path, sizeof(old_path) - 1);
                                old_path[sizeof(old_path) - 1] = '\0';

                                if (strcmp(ui_state->current_path, "/") == 0) {
                                    snprintf(ui_state->current_path, sizeof(ui_state->current_path), "/%s", selected->name);
                                } else {
                                    snprintf(ui_state->current_path, sizeof(ui_state->current_path), "%s/%s", old_path, selected->name);
                                }
                                
                                printf("Cargando carpeta: %s\n", selected->name);
                                if (archive_org_list_files(current_repo->id, ui_state->current_path, current_repo) != 0) {
                                    strncpy(ui_state->current_path, old_path, sizeof(ui_state->current_path));
                                    snprintf(ui_state->message, sizeof(ui_state->message), "Error al abrir la carpeta");
                                    ui_state->show_message = 1;
                                }
                                ui_state->selected_item = 0;
                                ui_state->browser_scroll_offset = 0;
                            } else {
                                // Descargar archivo desde Archive.org
                                char output_path[1024] = {0};
                                // Usamos selected->name para guardar el archivo directamente en la carpeta elegida
                                join_sdmc_path(output_path, sizeof(output_path), config->download_path, selected->name);

                                int result = archive_org_download_file(current_repo->id, selected->path, output_path);
                                if (result == 0) {
                                    ui_state->message[0] = '\0';
                                    printf("Descarga completada: %s\n", selected->name);
                                    append_to_message(ui_state->message, sizeof(ui_state->message), "Archivo '");
                                    append_to_message(ui_state->message, sizeof(ui_state->message), selected->name);
                                    append_to_message(ui_state->message, sizeof(ui_state->message), "' copiado en ");
                                    append_to_message(ui_state->message, sizeof(ui_state->message), config->download_path);
                                } else {
                                    ui_state->message[0] = '\0';
                                    append_to_message(ui_state->message, sizeof(ui_state->message), "Error descargando '");
                                    append_to_message(ui_state->message, sizeof(ui_state->message), selected->name);
                                    append_to_message(ui_state->message, sizeof(ui_state->message), "'");
                                }
                                ui_state->show_message = 1;
                            }
                        }
                    }
                }
                ui_draw_repository_browser(current_repo, ui_state, config);
                break;
            }
            case UI_MODE_SETTINGS: {
                if (input.action == UI_ACTION_BACK) {
                    ui_state->mode = UI_MODE_MAIN;
                } else if (input.action == UI_ACTION_SELECT) {
                    if (input.param == 1) {
                        ui_state->selected_setting++;
                        if (ui_state->selected_setting > 3) ui_state->selected_setting = 3;
                    } else if (input.param == -1) {
                        ui_state->selected_setting--;
                        if (ui_state->selected_setting < 0) ui_state->selected_setting = 0;
                    } else {
                        if (ui_state->selected_setting == 0) {
                            if (ui_state->path_nav) {
                                path_navigator_destroy(ui_state->path_nav);
                            }
                            ui_state->path_nav = path_navigator_create("sdmc:/");
                            ui_state->path_scroll_offset = 0;
                            ui_state->mode = UI_MODE_PATH_PICKER;
                        } else if (ui_state->selected_setting == 1) {
                            if (ui_show_text_input(ui_state->new_repo_url, sizeof(ui_state->new_repo_url), "URL completa de Archive.org") &&
                                ui_show_text_input(ui_state->new_repo_name, sizeof(ui_state->new_repo_name), "Nombre del repositorio")) {
                                // Guardar la URL completa como ID
                                config_add_custom_repo(config, ui_state->new_repo_name, ui_state->new_repo_url);
                                repository_add(manager, ui_state->new_repo_name, ui_state->new_repo_url);
                                config_save(config);
                                append_to_message(ui_state->message, sizeof(ui_state->message), "Repo añadido: ");
                                append_to_message(ui_state->message, sizeof(ui_state->message), ui_state->new_repo_name);
                                ui_state->show_message = 1;
                            } else {
                                snprintf(ui_state->message, sizeof(ui_state->message), "Cancelado");
                                ui_state->show_message = 1;
                            }
                        } else if (ui_state->selected_setting == 2) {
                            if (manager->repo_count > 0) {
                                config_remove_custom_repo(config, ui_state->selected_repo);
                                repository_manager_destroy(manager);
                                manager = repository_manager_create();
                                add_persistent_repos(manager, config);
                                config_save(config);
                                if (ui_state->selected_repo >= manager->repo_count) {
                                    ui_state->selected_repo = manager->repo_count - 1;
                                }
                                snprintf(ui_state->message, sizeof(ui_state->message), "Repositorio eliminado");
                                ui_state->show_message = 1;
                            }
                        } else if (ui_state->selected_setting == 3) {
                            config_save(config);
                            ui_state->mode = UI_MODE_MAIN;
                        }
                    }
                }
                ui_draw_settings_menu(ui_state, config);
                break;
            }
            case UI_MODE_PATH_PICKER: {
                if (input.action == UI_ACTION_BACK) {
                    ui_state->mode = UI_MODE_SETTINGS;
                    if (ui_state->path_nav) {
                        path_navigator_destroy(ui_state->path_nav);
                        ui_state->path_nav = NULL;
                    }
                } else if (input.action == UI_ACTION_SELECT) {
                    if (input.param == 1) {
                        if (ui_state->path_nav) {
                            path_navigator_move_selection(ui_state->path_nav, 1);
                        }
                    } else if (input.param == -1) {
                        if (ui_state->path_nav) {
                            path_navigator_move_selection(ui_state->path_nav, -1);
                        }
                    } else if (input.param == 0) {
                        if (ui_state->path_nav) {
                            const char *entry = path_navigator_get_selected_entry(ui_state->path_nav);
                            if (entry) {
                                if (strcmp(entry, "..") == 0) {
                                    path_navigator_go_back(ui_state->path_nav);
                                } else {
                                    path_navigator_enter_folder(ui_state->path_nav, entry);
                                }
                            }
                        }
                    }
                } else if (input.action == UI_ACTION_SETTINGS) {
                    if (ui_state->path_nav) {
                        char new_folder[128] = {0};
                        if (ui_show_text_input(new_folder, sizeof(new_folder), "Nombre de nueva carpeta")) {
                            if (new_folder[0] != '\0' && path_navigator_create_folder(ui_state->path_nav, new_folder) == 0) {
                                ui_state->message[0] = '\0';
                                append_to_message(ui_state->message, sizeof(ui_state->message), "Carpeta creada: ");
                                append_to_message(ui_state->message, sizeof(ui_state->message), new_folder);
                                ui_state->show_message = 1;
                            } else {
                                ui_state->message[0] = '\0';
                                append_to_message(ui_state->message, sizeof(ui_state->message), "No se pudo crear la carpeta");
                                ui_state->show_message = 1;
                            }
                        }
                    }
                } else if (input.action == UI_ACTION_SET_PATH) {
                    if (ui_state->path_nav) {
                        config_set_download_path(config, path_navigator_get_current(ui_state->path_nav));
                        config_save(config);
                        ui_state->mode = UI_MODE_SETTINGS;
                        path_navigator_destroy(ui_state->path_nav);
                        ui_state->path_nav = NULL;
                    }
                }
                ui_draw_path_picker(ui_state, config);
                break;
            }
            default: {
                ui_state->mode = UI_MODE_MAIN;
                break;
            }
        }

        consoleUpdate(NULL);
    }

    ui_state_destroy(ui_state);
    repository_manager_destroy(manager);
    config_save(config);
    config_destroy(config);
    archive_org_exit();
    fsdevUnmountAll();
    consoleExit(NULL);

    return 0;
}
