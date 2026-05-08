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
            repository_add(manager, saved->name, saved->id, saved->download_path);
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
    // Inicializar hardware y RomFS
    romfsInit();
    ensure_sdmc_mounted();

    // Inicializar SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) return -1;
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    // Crear ventana y renderizador
    SDL_Window *window = SDL_CreateWindow("Rammus", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    UIState *ui_state = ui_state_create();
    if (!ui_state) {
        printf("Error: No se pudo crear el estado de la UI.\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return -1;
    }
    ui_state->renderer = renderer;

    // Cargar Fuentes y Texturas desde las nuevas rutas de romfs
    ui_state->font_main = TTF_OpenFont("romfs:/font/font.ttf", 30); // Un poco más grande para legibilidad
    ui_state->font_small = TTF_OpenFont("romfs:/font/font.ttf", 20);
    ui_state->tex_repo = IMG_LoadTexture(renderer, "romfs:/img/repo.png");
    ui_state->tex_folder = IMG_LoadTexture(renderer, "romfs:/img/folder.png");
    ui_state->tex_file = IMG_LoadTexture(renderer, "romfs:/img/file.png");
    ui_state->tex_link = IMG_LoadTexture(renderer, "romfs:/img/link.png");

    // Solo cerramos si falta la fuente, que es vital. Las texturas pueden fallar.
    if (!ui_state->font_main || !ui_state->font_small) {
        printf("Error fatal: No se pudo cargar la fuente en romfs:/font/font.ttf\n");
        if (ui_state->font_main) TTF_CloseFont(ui_state->font_main);
        if (ui_state->font_small) TTF_CloseFont(ui_state->font_small);
        if (ui_state->tex_repo) SDL_DestroyTexture(ui_state->tex_repo);
        if (ui_state->tex_folder) SDL_DestroyTexture(ui_state->tex_folder);
        if (ui_state->tex_file) SDL_DestroyTexture(ui_state->tex_file);
        if (ui_state->tex_link) SDL_DestroyTexture(ui_state->tex_link);
        
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        ui_state_destroy(ui_state);
        return -1;
    }

    // Inicializar la ruta actual
    ui_state->current_path[0] = '/';
    ui_state->current_path[1] = '\0';

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    // Inicializar Archive.org
    if (archive_org_init() != 0) {
        printf("Error inicializando Archive.org\n");
        return -1;
    }

    AppConfig *config = config_create();
    config_load(config);

    RepositoryManager *manager = repository_manager_create();
    add_persistent_repos(manager, config);

    int running = 1;

    while (appletMainLoop() && running)
    {
        padUpdate(&pad);
        UIInput input = ui_handle_input(&pad);

        // Limpiar pantalla con color de fondo (Gris oscuro estilo Goldleaf)
        SDL_SetRenderDrawColor(renderer, 45, 45, 45, 255);
        SDL_RenderClear(renderer);

        if (ui_state->show_message) {
            ui_draw_message(ui_state);
            if (input.action == UI_ACTION_BACK) {
                ui_state->show_message = 0;
                // Permanecer en el modo actual en lugar de volver al menú principal
                if (ui_state->mode == UI_MODE_MAIN) ui_state->mode = UI_MODE_MAIN; 
            }
            SDL_RenderPresent(renderer); // Necesario para que el mensaje sea visible
            continue;
        }

        // Lógica de cambio de Pestaña Superior (Corregida)
        if (ui_state->mode == UI_MODE_MAIN || ui_state->mode == UI_MODE_SETTINGS || ui_state->mode == UI_MODE_FUTURE) {
            if (input.action == UI_ACTION_TAB_LEFT) {
                    ui_state->active_tab = (ui_state->active_tab == 0) ? 2 : ui_state->active_tab - 1;
                // Sincronizar el Modo con la Pestaña activa
                if (ui_state->active_tab == 0) ui_state->mode = UI_MODE_MAIN;
                else if (ui_state->active_tab == 1) ui_state->mode = UI_MODE_FUTURE;
                else if (ui_state->active_tab == 2) ui_state->mode = UI_MODE_SETTINGS;
            
                ui_state->selected_repo = 0;
                ui_state->selected_item = 0;
                ui_state->selected_setting = 0;
            } else if (input.action == UI_ACTION_TAB_RIGHT) {
                    ui_state->active_tab = (ui_state->active_tab == 2) ? 0 : ui_state->active_tab + 1;
                // Sincronizar el Modo con la Pestaña activa
                if (ui_state->active_tab == 0) ui_state->mode = UI_MODE_MAIN;
                else if (ui_state->active_tab == 1) ui_state->mode = UI_MODE_FUTURE;
                else if (ui_state->active_tab == 2) ui_state->mode = UI_MODE_SETTINGS;
            
                ui_state->selected_repo = 0;
                ui_state->selected_item = 0;
                ui_state->selected_setting = 0;
            }
        }

        switch (ui_state->mode) {
            case UI_MODE_MAIN: {
                if (input.action == UI_ACTION_EXIT) {
                    running = 0;
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
                            // Al entrar en un repo, el modo cambia a Browser
                            ui_state->mode = UI_MODE_BROWSER;
                            ui_state->selected_item = 0;
                            ui_state->browser_scroll_offset = 0;
                            ui_state->current_path[0] = '/';
                            ui_state->current_path[1] = '\0';
                            Repository *repo = repository_get(manager, ui_state->selected_repo); // Obtener el repo después de cambiar el modo
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
            case UI_MODE_FUTURE: {
                ui_draw_tabs(ui_state);
                SDL_Color white = {255, 255, 255, 255};
                draw_text(ui_state->renderer, ui_state->font_main, "Esta sección estará disponible en el futuro.", 100, 200, white);
                break;
            }
            case UI_MODE_BROWSER: {
                Repository *current_repo = repository_get(manager, ui_state->selected_repo);
                if (!current_repo) {
                    ui_state->mode = UI_MODE_MAIN;
                    break;
                }

                if (input.action == UI_ACTION_BACK) {
                    if (strcmp(ui_state->current_path, "/") != 0) {
                        int len = strlen(ui_state->current_path);
                        // Eliminar barra final si existe
                        if (ui_state->current_path[len - 1] == '/') {
                            ui_state->current_path[len - 1] = '\0';
                        }
                        
                        char *last_slash = strrchr(ui_state->current_path, '/');
                        if (last_slash && last_slash != ui_state->current_path) {
                            *last_slash = '\0'; // Cortar en la última carpeta
                        } else {
                            strcpy(ui_state->current_path, "/");
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
                                char old_path[sizeof(ui_state->current_path)];
                                strncpy(old_path, ui_state->current_path, sizeof(old_path) - 1);
                                old_path[sizeof(old_path) - 1] = '\0';

                                if (strcmp(ui_state->current_path, "/") == 0) {
                                    snprintf(ui_state->current_path, sizeof(ui_state->current_path), "/%s", selected->name);
                                } else {
                                    snprintf(ui_state->current_path, sizeof(ui_state->current_path), "%s/%s", old_path, selected->name);
                                }

                                if (archive_org_list_files(current_repo->id, ui_state->current_path, current_repo) != 0) {
                                    strncpy(ui_state->current_path, old_path, sizeof(ui_state->current_path));
                                    snprintf(ui_state->message, sizeof(ui_state->message), "Error al abrir la carpeta");
                                    ui_state->show_message = 1;
                                }
                                ui_state->selected_item = 0;
                                ui_state->browser_scroll_offset = 0;
                            } else {
                                // Descargar archivo desde Archive.org
                                char output_path[2048] = {0}; // Aumentar tamaño para rutas largas
                                // Usamos selected->name para guardar el archivo directamente en la carpeta elegida
                                join_sdmc_path(output_path, sizeof(output_path), current_repo->download_path, selected->name);

                                int result = archive_org_download_file(current_repo->id, selected->path, output_path, ui_state, manager, config);
                                if (result == 0) {
                                    ui_state->message[0] = '\0';
                                    printf("Descarga completada: %s\n", selected->name);
                                    append_to_message(ui_state->message, sizeof(ui_state->message), "Archivo '");
                                    append_to_message(ui_state->message, sizeof(ui_state->message), selected->name);
                                    append_to_message(ui_state->message, sizeof(ui_state->message), "' copiado en ");
                                    append_to_message(ui_state->message, sizeof(ui_state->message), current_repo->download_path);
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
                if (input.action == UI_ACTION_SELECT) {
                    if (input.param == 0) {
                        ui_state->mode = UI_MODE_REPO_MANAGER;
                        ui_state->selected_repo = 0;
                    }
                }
                ui_draw_settings_menu(ui_state, config);
                break;
            }
            case UI_MODE_REPO_MANAGER: {
                if (input.action == UI_ACTION_BACK) {
                    ui_state->mode = UI_MODE_SETTINGS;
                } else if (input.action == UI_ACTION_SETTINGS) { // Botón X para añadir
                    ui_state->is_creating_new = 1;
                    if (ui_show_text_input(ui_state->new_repo_url, sizeof(ui_state->new_repo_url), "URL completa de Archive.org")) {
                        if (ui_show_text_input(ui_state->new_repo_name, sizeof(ui_state->new_repo_name), "Nombre del repositorio")) {
                            ui_state->path_nav = path_navigator_create("sdmc:/");
                            ui_state->mode = UI_MODE_PATH_PICKER;
                        }
                    }
                } else if (input.action == UI_ACTION_SELECT) {
                    if (input.param == 1) {
                        ui_state->selected_repo = (ui_state->selected_repo + 1) % manager->repo_count;
                    } else if (input.param == -1) {
                        ui_state->selected_repo = (ui_state->selected_repo - 1 + manager->repo_count) % manager->repo_count;
                    } else if (input.param == 0 && manager->repo_count > 0) {
                        ui_state->mode = UI_MODE_REPO_SUBMENU;
                        ui_state->selected_setting = 0;
                        ui_state->editing_repo_index = ui_state->selected_repo;
                    }
                }
                ui_draw_repo_manager(manager, ui_state);
                break;
            }
            case UI_MODE_REPO_SUBMENU: {
                Repository *edit_repo = &manager->repositories[ui_state->editing_repo_index];
                if (input.action == UI_ACTION_BACK) {
                    ui_state->mode = UI_MODE_REPO_MANAGER;
                } else if (input.action == UI_ACTION_SELECT) {
                    if (input.param == 1) ui_state->selected_setting = (ui_state->selected_setting + 1) % 4;
                    else if (input.param == -1) ui_state->selected_setting = (ui_state->selected_setting - 1 + 4) % 4;
                    else if (input.param == 0) {
                        if (ui_state->selected_setting == 0) { // Editar URL
                            ui_show_text_input(edit_repo->id, sizeof(edit_repo->id), "Nueva URL");
                            // Sincronizar con config
                            Repository *c_repo = config_get_custom_repo(config, ui_state->editing_repo_index);
                            strcpy(c_repo->id, edit_repo->id);
                            config_save(config);
                        } else if (ui_state->selected_setting == 1) { // Editar Path
                            ui_state->is_creating_new = 0;
                            ui_state->path_nav = path_navigator_create(edit_repo->download_path);
                            ui_state->mode = UI_MODE_PATH_PICKER;
                        } else if (ui_state->selected_setting == 2) { // Editar Nombre
                            ui_show_text_input(edit_repo->name, sizeof(edit_repo->name), "Nuevo Nombre");
                            Repository *c_repo = config_get_custom_repo(config, ui_state->editing_repo_index);
                            strcpy(c_repo->name, edit_repo->name);
                            config_save(config);
                        } else if (ui_state->selected_setting == 3) { // Eliminar
                            config_remove_custom_repo(config, ui_state->editing_repo_index);
                            repository_manager_destroy(manager);
                            manager = repository_manager_create();
                            add_persistent_repos(manager, config);
                            config_save(config);
                            ui_state->mode = UI_MODE_REPO_MANAGER;
                            ui_state->selected_repo = 0;
                        }
                    }
                }
                ui_draw_repo_submenu(edit_repo, ui_state);
                break;
            }
            case UI_MODE_PATH_PICKER: {
                if (input.action == UI_ACTION_BACK) {
                    ui_state->mode = ui_state->is_creating_new ? UI_MODE_REPO_MANAGER : UI_MODE_REPO_SUBMENU;
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
                        const char *new_path = path_navigator_get_current(ui_state->path_nav);
                        if (ui_state->is_creating_new) {
                            config_add_custom_repo(config, ui_state->new_repo_name, ui_state->new_repo_url, new_path);
                            repository_add(manager, ui_state->new_repo_name, ui_state->new_repo_url, new_path);
                            config_save(config);
                            ui_state->mode = UI_MODE_REPO_MANAGER;
                        } else {
                            Repository *edit_repo = &manager->repositories[ui_state->editing_repo_index];
                            strcpy(edit_repo->download_path, new_path);
                            Repository *c_repo = config_get_custom_repo(config, ui_state->editing_repo_index);
                            strcpy(c_repo->download_path, new_path);
                            config_save(config);
                            ui_state->mode = UI_MODE_REPO_SUBMENU;
                        }
                        
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

        if (ui_state->is_downloading) {
            ui_draw_progress(ui_state);
        }

        SDL_RenderPresent(renderer);
    }

    // Cleanup
    TTF_CloseFont(ui_state->font_main);
    TTF_CloseFont(ui_state->font_small);
    SDL_DestroyTexture(ui_state->tex_repo);
    SDL_DestroyTexture(ui_state->tex_folder);
    SDL_DestroyTexture(ui_state->tex_file);
    SDL_DestroyTexture(ui_state->tex_link);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    romfsExit();
    ui_state_destroy(ui_state);
    // ... resto de destroyers ...

    return 0;
}
