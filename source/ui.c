#include "ui.h"
#include <stdio.h>
#include <switch.h>
#include <math.h> // Para round
#include <stdlib.h> // Para malloc y free
#include <string.h>

// Helper para dibujar texto
void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    if (!text || text[0] == '\0') return;
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void ui_draw_tabs(UIState *state) {
    const char *tabs[] = {"REPOSITORIOS", "COPIAS", "AJUSTES"}; // Renombrado de FUTURO a COPIAS
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color active_cyan = {0, 255, 255, 255};

    // Fondo barra superior (Azul oscuro eléctrico)
    SDL_Rect top_bar = {0, 0, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 10, 40, 75, 255);
    SDL_RenderFillRect(state->renderer, &top_bar);

    int tab_width = 300;
    int start_x = (1280 - (tab_width * 3)) / 2;

    for (int i = 0; i < 3; i++) {
        int x = start_x + (i * tab_width);
        // Centrado perfecto del texto de las pestañas
        int tw, th;
        TTF_SizeUTF8(state->font_main, tabs[i], &tw, &th);
        int text_x = x + (tab_width - tw) / 2;
        draw_text(state->renderer, state->font_main, tabs[i], text_x, 25, (state->active_tab == i) ? active_cyan : white);
        
        if (state->active_tab == i) {
            SDL_Rect indicator = {text_x - 10, 65, tw + 20, 5};
            SDL_SetRenderDrawColor(state->renderer, 0, 255, 255, 255);
            SDL_RenderFillRect(state->renderer, &indicator);
        }
    }

    // Dibujar logo de la aplicación en la parte superior izquierda
    if (state->tex_logo) {
        SDL_Rect logo_rect = {25, 15, 50, 50};
        SDL_RenderCopy(state->renderer, state->tex_logo, NULL, &logo_rect);
    }

    // Dibujar usuario actual en la esquina superior derecha
    if (state->save_mgr && state->save_mgr->user_count > 0) {
        char user_text[64];
        if (state->save_mgr->current_user_index == 0) 
            snprintf(user_text, sizeof(user_text), "Modo: %s", state->save_mgr->users[0].name);
        else
            snprintf(user_text, sizeof(user_text), "User: %s", state->save_mgr->users[state->save_mgr->current_user_index].name);
            
        int utw, uth;
        TTF_SizeUTF8(state->font_small, user_text, &utw, &uth);
        draw_text(state->renderer, state->font_small, user_text, 1240 - utw, 30, white);
    }
}

void ui_draw_progress(UIState *state) {
    if (!state || !state->is_downloading) return;

    // Fondo oscuro para el modal
    SDL_Rect modal = {340, 280, 600, 160};
    SDL_SetRenderDrawColor(state->renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(state->renderer, &modal);
    SDL_SetRenderDrawColor(state->renderer, 0, 255, 255, 255);
    SDL_RenderDrawRect(state->renderer, &modal);

    SDL_Color white = {255, 255, 255, 255};
    draw_text(state->renderer, state->font_main, "Copiando archivo...", 360, 300, white);
    draw_text(state->renderer, state->font_small, state->download_status, 360, 335, white);

    // Barra de progreso
    SDL_Rect bar_bg = {360, 375, 560, 30};
    SDL_SetRenderDrawColor(state->renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(state->renderer, &bar_bg);

    SDL_Rect bar_fill = {360, 375, (int)(560 * state->download_progress), 30};
    SDL_SetRenderDrawColor(state->renderer, 0, 200, 255, 255); // Azul brillante
    SDL_RenderFillRect(state->renderer, &bar_fill);

    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", (int)(state->download_progress * 100));
    draw_text(state->renderer, state->font_small, pct, 860, 300, white);
}

static void ui_ensure_scroll(int *offset, int selected, int count, int visible_items) {
    if (!offset) return;
    if (count <= visible_items) {
        *offset = 0;
        return;
    }
    if (*offset > selected) {
        *offset = selected;
    }
    if (*offset + visible_items <= selected) {
        *offset = selected - visible_items + 1;
    }
    if (*offset < 0) {
        *offset = 0;
    }
    if (*offset > count - visible_items) {
        *offset = count - visible_items;
    }
}

UIState* ui_state_create(void) {
    UIState *state = (UIState*)malloc(sizeof(UIState));
    if (state) {
        state->mode = UI_MODE_MAIN;
        state->active_tab = 0;
        state->selected_repo = 0;
        state->selected_item = 0;
        state->selected_setting = 0;
        state->current_path[0] = '/';
        state->current_path[1] = '\0';
        state->new_repo_url[0] = '\0';
        state->new_repo_name[0] = '\0';
        state->message[0] = '\0';
        state->show_message = 0;
        state->path_nav = NULL;
        state->browser_scroll_offset = 0;
        state->path_scroll_offset = 0;
        state->is_downloading = 0;
        state->renderer = NULL;
        state->font_main = NULL;
        state->font_small = NULL;
        state->tex_repo = NULL;
        state->tex_folder = NULL;
        state->tex_file = NULL;
        state->tex_link = NULL;
        state->tex_logo = NULL;
        state->tex_game_icon = NULL; // Inicializar nueva textura
        state->save_mgr = save_manager_create();
        state->tex_settings = NULL;
        state->selected_save_option = 0;
        state->pending_action = 0;
        state->target_save_path[0] = '\0';
    }
    return state;
}

void ui_state_destroy(UIState *state) {
    if (state) {
        if (state->path_nav) {
            path_navigator_destroy(state->path_nav);
        }
        if (state->save_mgr) {
            save_manager_destroy(state->save_mgr, state->renderer);
        }
        free(state);
    }
}

UIInput ui_handle_input(PadState *pad) {
    UIInput input = {UI_ACTION_NONE, 0};
    u64 kDown = padGetButtonsDown(pad);

    if (kDown & HidNpadButton_Down) {
        input.action = UI_ACTION_SELECT;
        input.param = 1;
    } else if (kDown & HidNpadButton_Up) {
        input.action = UI_ACTION_SELECT;
        input.param = -1;
    } else if (kDown & HidNpadButton_A) {
        input.action = UI_ACTION_SELECT;
        input.param = 0;
    } else if (kDown & HidNpadButton_B) {
        input.action = UI_ACTION_BACK;
    } else if (kDown & HidNpadButton_Plus) {
        input.action = UI_ACTION_EXIT;
    } else if (kDown & HidNpadButton_X) {
        input.action = UI_ACTION_SETTINGS;
    } else if (kDown & HidNpadButton_Y) {
        input.action = UI_ACTION_SET_PATH;
    }

    if (kDown & HidNpadButton_L) input.action = UI_ACTION_TAB_LEFT;
    if (kDown & HidNpadButton_R) input.action = UI_ACTION_TAB_RIGHT;

    return input;
}

int ui_show_text_input(char *out, size_t size, const char *guide_text) {
    SwkbdConfig config;
    swkbdCreate(&config, 0);
    swkbdConfigMakePresetDefault(&config);
    swkbdConfigSetGuideText(&config, guide_text);
    swkbdConfigSetInitialText(&config, out);
    swkbdConfigSetOkButtonText(&config, "Aceptar");
    swkbdConfigSetHeaderText(&config, "Input");

    Result rc = swkbdShow(&config, out, size);
    swkbdClose(&config);
    return R_SUCCEEDED(rc);
}

void ui_draw_main_menu(RepositoryManager *manager, UIState *state, AppConfig *config) {
    SDL_SetRenderDrawColor(state->renderer, 240, 245, 250, 255); // Fondo claro/alegre
    SDL_RenderClear(state->renderer);
    ui_draw_tabs(state);
    SDL_Color dark_text = {30, 30, 30, 255};
    SDL_Color white = {255, 255, 255, 255}; // Declarar 'white' localmente
    SDL_Rect icon_rect = {100, 0, 48, 48};

    if (!manager || manager->repo_count == 0) {
        draw_text(state->renderer, state->font_main, "No hay repositorios configurados.", 100, 150, dark_text);
    } else {
        for (int i = 0; i < manager->repo_count; i++) {
            int y = 150 + (i * 60);
            if (i == state->selected_repo) {
                SDL_Rect highlight = {80, y - 5, 1120, 55};
                SDL_SetRenderDrawColor(state->renderer, 180, 220, 255, 255); // Azul celeste de selección
                SDL_RenderFillRect(state->renderer, &highlight);
            }
            
            icon_rect.y = y; // Posición Y del icono
            if (state->tex_repo) SDL_RenderCopy(state->renderer, state->tex_repo, NULL, &icon_rect); // Usar tex_repo
            draw_text(state->renderer, state->font_main, manager->repositories[i].name, 160, y, dark_text);
        }
    }

    // Barra inferior de instrucciones
    SDL_Rect footer = {0, 640, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &footer);
    draw_text(state->renderer, state->font_small, "(A) Seleccionar Repo  (L/R) Cambiar Pestaña  (+) Salir", 40, 665, white);
}

void ui_draw_repository_browser(Repository *repo, UIState *state, AppConfig *config) {
    SDL_SetRenderDrawColor(state->renderer, 240, 245, 250, 255);
    SDL_RenderClear(state->renderer);

    // Fondo del encabezado
    SDL_Rect header_bg = {0, 0, 1280, 100};
    SDL_SetRenderDrawColor(state->renderer, 25, 60, 85, 255);
    SDL_RenderFillRect(state->renderer, &header_bg);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color dark_grey = {60, 60, 60, 255};
    
    draw_text(state->renderer, state->font_main, repo->name, 40, 20, white); // Título del repositorio
    draw_text(state->renderer, state->font_small, state->current_path, 40, 60, white);

    if (repo->item_count == 0) {
        draw_text(state->renderer, state->font_main, "Cargando o carpeta vacía...", 100, 200, dark_grey);
    } else {
        ui_ensure_scroll(&state->browser_scroll_offset, state->selected_item, repo->item_count, 11);
        
        int start = state->browser_scroll_offset;
        int visible_count = 11;

        for (int i = 0; i < visible_count && (start + i) < repo->item_count; i++) {
            int idx = start + i;
            int y = 120 + (i * 50);

            if (idx == state->selected_item) {
                SDL_Rect highlight = {20, y - 5, 1240, 45};
                SDL_SetRenderDrawColor(state->renderer, 200, 230, 250, 255);
                SDL_RenderFillRect(state->renderer, &highlight);
            }

            SDL_Rect icon_rect = {40, y - 2, 40, 40};
            SDL_Texture *icon = repo->items[idx].is_dir ? state->tex_folder : state->tex_file;
            if (icon) SDL_RenderCopy(state->renderer, icon, NULL, &icon_rect);
            
            draw_text(state->renderer, state->font_main, repo->items[idx].name, 100, y, dark_grey);
            if (!repo->items[idx].is_dir && repo->items[idx].size_text[0] != '\0') { // Solo mostrar tamaño si no es directorio y tiene texto
                draw_text(state->renderer, state->font_small, repo->items[idx].size_text, 1050, y + 5, dark_grey);
            }
        }
    }

    // Barra inferior de instrucciones
    SDL_Rect footer = {0, 640, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &footer);
    draw_text(state->renderer, state->font_small, "(A) Abrir/Descargar  (B) Atrás", 40, 665, white);
}

void ui_draw_settings_menu(UIState *state, AppConfig *config) {
    SDL_SetRenderDrawColor(state->renderer, 240, 245, 250, 255); // Fondo claro/alegre
    SDL_RenderClear(state->renderer);
    ui_draw_tabs(state);
    SDL_Color dark_text = {30, 30, 30, 255};

    const char *items[] = {
        "Gestionar repositorios",
        "Elegir carpeta de destino de copias de juegos" // Nueva opción
    };

    for (int i = 0; i < 2; i++) { // Ahora hay 2 opciones
        int y = 160 + (i * 60);
        if (i == state->selected_setting) {
            SDL_Rect highlight = {80, y - 5, 1120, 50};
            SDL_SetRenderDrawColor(state->renderer, 180, 220, 255, 255);
            SDL_RenderFillRect(state->renderer, &highlight);
        }
        draw_text(state->renderer, state->font_main, items[i], 120, y, dark_text);
    }
    
    // Mostrar ruta actual de copias de guardado
    SDL_Color light_grey_text = {100, 100, 100, 255};
    char path_label[1100];
    snprintf(path_label, sizeof(path_label), "Ruta actual: %s", config_get_save_backup_path(config));
    draw_text(state->renderer, state->font_small, path_label, 120, 280, light_grey_text); // Bajado a 280

    // Barra inferior de instrucciones
    SDL_Color white = {255, 255, 255, 255};
    SDL_Rect footer = {0, 640, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &footer);
    draw_text(state->renderer, state->font_small, "(A) Seleccionar  (L/R) Cambiar Pestaña", 40, 665, white);
}

void ui_draw_restore_menu(UIState *state) {
    SDL_SetRenderDrawColor(state->renderer, 240, 245, 250, 255);
    SDL_RenderClear(state->renderer);
    ui_draw_tabs(state);
    SDL_Color dark_text = {30, 30, 30, 255}; SDL_Color white = {255, 255, 255, 255};
    SaveManager *sm = state->save_mgr;
    if (!sm || !sm->restore_entry) return;

    char title[1024];
    snprintf(title, sizeof(title), "Restaurar: %s", sm->restore_entry->name);
    draw_text(state->renderer, state->font_main, title, 100, 100, dark_text);

    if (sm->restore_count == 0) {
        draw_text(state->renderer, state->font_small, "No se encontraron copias para este juego.", 100, 150, dark_text);
    } else {
        for (int i = 0; i < sm->restore_count; i++) {
            int y = 160 + (i * 50);
            if (i == sm->restore_selected) {
                SDL_Rect highlight = {80, y - 5, 1120, 45};
                SDL_SetRenderDrawColor(state->renderer, 180, 220, 255, 255);
                SDL_RenderFillRect(state->renderer, &highlight);
            }
            draw_text(state->renderer, state->font_small, sm->restore_folders[i], 120, y, dark_text);
        }
    }

    SDL_Rect footer = {0, 640, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &footer);
    draw_text(state->renderer, state->font_small, "(A) Restaurar Seleccionada  (B) Cancelar", 40, 665, white);
}

void ui_draw_save_options(UIState *state) {
    ui_draw_restore_menu(state); // Fondo
    SDL_SetRenderDrawBlendMode(state->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 150);
    SDL_Rect overlay = {0, 0, 1280, 720}; SDL_RenderFillRect(state->renderer, &overlay);
    SDL_SetRenderDrawBlendMode(state->renderer, SDL_BLENDMODE_NONE);

    SDL_Rect modal = {440, 200, 400, 300};
    SDL_SetRenderDrawColor(state->renderer, 45, 45, 45, 255); SDL_RenderFillRect(state->renderer, &modal);
    SDL_SetRenderDrawColor(state->renderer, 0, 255, 255, 255); SDL_RenderDrawRect(state->renderer, &modal);

    SDL_Color white = {255, 255, 255, 255}; SDL_Color cyan = {0, 255, 255, 255};
    draw_text(state->renderer, state->font_main, "¿Qué quieres hacer?", 460, 220, cyan);

    const char *options[] = {"Restaurar", "Duplicar", "Borrar"};
    for (int i = 0; i < 3; i++) {
        int y = 280 + (i * 50);
        if (i == state->selected_save_option) {
            SDL_Rect highlight = {450, y - 5, 380, 45};
            SDL_SetRenderDrawColor(state->renderer, 70, 70, 70, 255); SDL_RenderFillRect(state->renderer, &highlight);
            draw_text(state->renderer, state->font_main, options[i], 470, y, cyan);
        } else draw_text(state->renderer, state->font_main, options[i], 470, y, white);
    }
    draw_text(state->renderer, state->font_small, "(A) Confirmar  (B) Atrás", 460, 460, white);
}

void ui_draw_confirm_dialog(UIState *state, const char *action_text) {
    ui_draw_save_options(state); // Mantener capas visibles
    SDL_SetRenderDrawBlendMode(state->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 180);
    SDL_Rect overlay = {0, 0, 1280, 720}; SDL_RenderFillRect(state->renderer, &overlay);
    SDL_SetRenderDrawBlendMode(state->renderer, SDL_BLENDMODE_NONE);

    SDL_Rect modal = {340, 260, 600, 200};
    SDL_SetRenderDrawColor(state->renderer, 40, 40, 40, 255); SDL_RenderFillRect(state->renderer, &modal);
    SDL_SetRenderDrawColor(state->renderer, 255, 50, 50, 255); SDL_RenderDrawRect(state->renderer, &modal);

    SDL_Color white = {255, 255, 255, 255}; SDL_Color red = {255, 80, 80, 255};
    draw_text(state->renderer, state->font_main, "Confirmar acción", 360, 280, red);
    char question[256]; snprintf(question, sizeof(question), "¿Estás seguro de %s?", action_text);
    draw_text(state->renderer, state->font_small, question, 360, 330, white);
    draw_text(state->renderer, state->font_small, "(A) SI, ESTOY SEGURO  (B) CANCELAR", 360, 420, white);
}

void ui_draw_saves_menu(UIState *state) {
    SDL_SetRenderDrawColor(state->renderer, 240, 245, 250, 255);
    SDL_RenderClear(state->renderer);
    ui_draw_tabs(state);
    SDL_Color dark_text = {30, 30, 30, 255};
    SDL_Color white = {255, 255, 255, 255};
    
    if (!state->save_mgr || state->save_mgr->count == 0) {
        draw_text(state->renderer, state->font_main, "No se encontraron datos de guardado.", 100, 150, dark_text);
    } else {
        ui_ensure_scroll(&state->browser_scroll_offset, state->selected_item, state->save_mgr->count, 6);
        int start = state->browser_scroll_offset;
        
        // Dibujamos una lista "gorda" con iconos grandes
        for (int i = 0; i < 6 && (start + i) < state->save_mgr->count; i++) {
            int idx = start + i;
            int y = 120 + (i * 85);
            
            if (idx == state->selected_item) {
                SDL_Rect highlight = {40, y - 5, 1200, 80};
                SDL_SetRenderDrawColor(state->renderer, 180, 220, 255, 255);
                SDL_RenderFillRect(state->renderer, &highlight);
            }
            
            SaveEntry *e = &state->save_mgr->entries[idx];
            
            // Dibujar Carátula del juego (70x70)
            SDL_Rect icon_rect = {60, y, 70, 70};
            if (e->icon) {
                SDL_RenderCopy(state->renderer, e->icon, NULL, &icon_rect);
            } else if (state->tex_game_icon) {
                SDL_RenderCopy(state->renderer, state->tex_game_icon, NULL, &icon_rect);
            }
            
            draw_text(state->renderer, state->font_main, e->name, 150, y + 15, dark_text);
        }
    }

    // Barra inferior de instrucciones
    SDL_Rect footer = {0, 640, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &footer);
    draw_text(state->renderer, state->font_small, "(A) Backup Save  (L/R) Cambiar Pestaña  (+) Salir", 40, 665, white);
}

void ui_draw_repo_manager(RepositoryManager *manager, UIState *state) {
    SDL_SetRenderDrawColor(state->renderer, 240, 245, 250, 255);
    SDL_RenderClear(state->renderer);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color dark_text = {30, 30, 30, 255};

    // Cabecera superior
    SDL_Rect header = {0, 0, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &header);
    draw_text(state->renderer, state->font_main, "Gestionar Repositorios", 40, 20, white);

    // Instrucciones (Cabecera inferior/Bottom Bar)
    SDL_Rect footer = {0, 640, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &footer);
    draw_text(state->renderer, state->font_small, "(X) Añadir nuevo repo  (A) Gestionar seleccionado  (B) Volver", 40, 665, white);

    if (manager->repo_count == 0) {
        draw_text(state->renderer, state->font_main, "No hay repositorios. Pulsa (X) para añadir.", 100, 200, dark_text);
    } else {
        ui_ensure_scroll(&state->browser_scroll_offset, state->selected_repo, manager->repo_count, 8);
        int start = state->browser_scroll_offset;
        int visible_count = 8; // Ajustado para mostrar más repositorios

        for (int i = 0; i < visible_count && (start + i) < manager->repo_count; i++) {
            int idx = start + i;
            int y = 120 + (i * 60);
            if (idx == state->selected_repo) {
                SDL_Rect highlight = {80, y - 5, 1120, 50};
                SDL_SetRenderDrawColor(state->renderer, 180, 220, 255, 255);
                SDL_RenderFillRect(state->renderer, &highlight);
            }
            draw_text(state->renderer, state->font_main, manager->repositories[idx].name, 120, y, dark_text);
            draw_text(state->renderer, state->font_small, manager->repositories[idx].download_path, 700, y + 10, dark_text);
        }
    }
}

void ui_draw_repo_submenu(Repository *repo, UIState *state) {
    SDL_SetRenderDrawColor(state->renderer, 240, 245, 250, 255);
    SDL_RenderClear(state->renderer);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color dark_text = {30, 30, 30, 255};

    SDL_Rect header = {0, 0, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &header);
    char title[600];
    snprintf(title, sizeof(title), "Opciones: %s", repo->name);
    draw_text(state->renderer, state->font_main, title, 40, 20, white);

    const char *items[] = {
        "Editar URL",
        "Seleccionar ruta de descarga",
        "Editar nombre",
        "Eliminar repositorio"
    };

    for (int i = 0; i < 4; i++) {
        int y = 160 + (i * 70);
        if (i == state->selected_setting) {
            SDL_Rect highlight = {80, y - 5, 1120, 60};
            SDL_SetRenderDrawColor(state->renderer, 180, 220, 255, 255);
            SDL_RenderFillRect(state->renderer, &highlight);
        }
        draw_text(state->renderer, state->font_main, items[i], 120, y, dark_text);
        
        // Mostrar valor actual
        if (i == 0) draw_text(state->renderer, state->font_small, repo->id, 500, y + 15, dark_text);
        if (i == 1) draw_text(state->renderer, state->font_small, repo->download_path, 500, y + 15, dark_text);
    }

    SDL_Rect footer = {0, 640, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &footer);
    draw_text(state->renderer, state->font_small, "(A) Seleccionar  (B) Volver", 40, 665, white);
}

void ui_draw_path_picker(UIState *state, AppConfig *config) {
    SDL_SetRenderDrawColor(state->renderer, 240, 245, 250, 255); // Fondo claro/alegre
    SDL_RenderClear(state->renderer);

    SDL_Color white = {255, 255, 255, 255}; // Para el texto del encabezado
    SDL_Color gold = {0, 200, 255, 255}; // Cian para el título
    SDL_Color dark_text = {30, 30, 30, 255}; // Para los elementos de la lista
    draw_text(state->renderer, state->font_main, "Seleccionar ruta de descarga", 40, 20, gold);
    draw_text(state->renderer, state->font_small, state->path_nav ? path_navigator_get_current(state->path_nav) : "", 40, 60, white);

    if (state->path_nav) {
        int count = path_navigator_get_entry_count(state->path_nav);
        char **entries = path_navigator_get_entries(state->path_nav);

        ui_ensure_scroll(&state->path_scroll_offset, state->path_nav->selected_entry, count, 11);
        
        for (int i = 0; i < 11 && (state->path_scroll_offset + i) < count; i++) {
            int idx = state->path_scroll_offset + i;
            int y = 120 + (i * 50);
            
            if (idx == state->path_nav->selected_entry) {
                SDL_Rect highlight = {20, y - 5, 1240, 45};
                SDL_SetRenderDrawColor(state->renderer, 180, 220, 255, 255); // Azul celeste de selección
                SDL_RenderFillRect(state->renderer, &highlight);
            }

            SDL_Rect icon_rect = {40, y - 2, 40, 40};
            if (state->tex_folder) SDL_RenderCopy(state->renderer, state->tex_folder, NULL, &icon_rect);
            draw_text(state->renderer, state->font_main, entries[idx], 100, y, dark_text);
        }
    }

    // Barra inferior de instrucciones
    SDL_Rect footer = {0, 640, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255);
    SDL_RenderFillRect(state->renderer, &footer);
    draw_text(state->renderer, state->font_small, "(A) Entrar  (Y) Seleccionar Ruta  (X) Nueva Carpeta  (B) Volver", 40, 665, white);
}

void ui_draw_message(UIState *state) {
    // Fondo semi-transparente para el modal de mensaje
    SDL_SetRenderDrawBlendMode(state->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 180); // Negro semi-transparente
    SDL_Rect overlay = {0, 0, 1280, 720};
    SDL_RenderFillRect(state->renderer, &overlay);
    SDL_SetRenderDrawBlendMode(state->renderer, SDL_BLENDMODE_NONE);

    SDL_Rect modal = {340, 210, 600, 300};
    SDL_SetRenderDrawColor(state->renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(state->renderer, &modal);
    SDL_SetRenderDrawColor(state->renderer, 255, 215, 0, 255);
    SDL_RenderDrawRect(state->renderer, &modal);

    SDL_Color white = {255, 255, 255, 255};
    draw_text(state->renderer, state->font_main, "Información", 360, 230, white);
    draw_text(state->renderer, state->font_small, state->message, 360, 300, white);
    draw_text(state->renderer, state->font_small, "Presiona B para continuar", 500, 460, white);
}
