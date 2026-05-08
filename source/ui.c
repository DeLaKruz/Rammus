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
    const char *tabs[] = {"REPOSITORIOS", "FUTURO", "AJUSTES"};
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

static void ui_ensure_scroll(int *offset, int selected, int count) {
    const int visible_items = 10;
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
        state->tex_settings = NULL;
    }
    return state;
}

void ui_state_destroy(UIState *state) {
    if (state) {
        if (state->path_nav) {
            path_navigator_destroy(state->path_nav);
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
        ui_ensure_scroll(&state->browser_scroll_offset, state->selected_item, repo->item_count);
        
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

    int y = 160;
    if (state->selected_setting == 0) {
        SDL_Rect highlight = {80, y - 5, 1120, 50};
        SDL_SetRenderDrawColor(state->renderer, 180, 220, 255, 255);
        SDL_RenderFillRect(state->renderer, &highlight);
    }
    draw_text(state->renderer, state->font_main, "Gestionar repositorios", 120, y, dark_text);

    // Barra inferior de instrucciones
    SDL_Color white = {255, 255, 255, 255}; // Declarar 'white' localmente
    SDL_Rect footer = {0, 640, 1280, 80};
    SDL_SetRenderDrawColor(state->renderer, 20, 60, 90, 255); // Corregido: state->renderer->renderer a state->renderer
    SDL_RenderFillRect(state->renderer, &footer);
    draw_text(state->renderer, state->font_small, "(A) Seleccionar  (L/R) Cambiar Pestaña", 40, 665, white); // Usar 'white' local
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
        for (int i = 0; i < manager->repo_count; i++) {
            int y = 120 + (i * 60);
            if (i == state->selected_repo) {
                SDL_Rect highlight = {80, y - 5, 1120, 50};
                SDL_SetRenderDrawColor(state->renderer, 180, 220, 255, 255);
                SDL_RenderFillRect(state->renderer, &highlight);
            }
            draw_text(state->renderer, state->font_main, manager->repositories[i].name, 120, y, dark_text);
            draw_text(state->renderer, state->font_small, manager->repositories[i].download_path, 700, y + 10, dark_text);
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

        ui_ensure_scroll(&state->path_scroll_offset, state->path_nav->selected_entry, count);
        
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
