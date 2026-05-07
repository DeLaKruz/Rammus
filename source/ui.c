#include "ui.h"
#include <stdio.h>
#include <switch.h>
#include <string.h>

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
    printf("\x1b[2J");
    printf("\x1b[0;0H");
    printf("=== ARCHIVE.ORG NAVIGATOR ===\n");
    printf("Ruta descarga: %s\n", config ? config->download_path : "sdmc:/");
    printf("Repositorios disponibles:\n\n");

    if (!manager || manager->repo_count == 0) {
        printf("No hay repositorios.\n");
    } else {
        for (int i = 0; i < manager->repo_count; i++) {
            printf(i == state->selected_repo ? "> " : "  ");
            printf("[%d] %s\n", i + 1, manager->repositories[i].name);
        }
    }

    printf("\n\nControles:\n");
    printf("^ / v - Navegar\n");
    printf("A - Seleccionar\n");
    printf("X - Ajustes\n");
    printf("+ - Salir\n");
}

void ui_draw_repository_browser(Repository *repo, UIState *state, AppConfig *config) {
    printf("\x1b[2J");
    printf("\x1b[0;0H");
    printf("=== %s ===\n", repo->name);
    printf("Ruta: %s\n", state->current_path);
    printf("Guardado en: %s\n\n", config ? config->download_path : "sdmc:/");

    if (repo->item_count == 0) {
        printf("Carpeta vacía.\n");
    } else {
        ui_ensure_scroll(&state->browser_scroll_offset, state->selected_item, repo->item_count);
        int start = state->browser_scroll_offset;
        int end = start + 10;
        if (end > repo->item_count) {
            end = repo->item_count;
        }
        if (start > 0) {
            printf("  ...\n");
        }
        for (int i = start; i < end; i++) {
            printf(i == state->selected_item ? "> " : "  ");
            if (repo->items[i].is_dir) {
                printf("[CARPETA] %s\n", repo->items[i].name);
            } else {
                printf("[ARCHIVO] %s\n", repo->items[i].name);
            }
        }
        if (end < repo->item_count) {
            printf("  ...\n");
        }
    }

    printf("\n\nControles:\n");
    printf("^ / v - Navegar\n");
    printf("A - Entrar/Descargar\n");
    printf("B - Atrás\n");
    printf("X - Ajustes\n");
}

void ui_draw_settings_menu(UIState *state, AppConfig *config) {
    const char *items[] = {
        "Seleccionar ruta de descarga",
        "Añadir repositorio",
        "Eliminar repositorio",
        "Guardar y volver"
    };
    int item_count = 4;

    printf("\x1b[2J");
    printf("\x1b[0;0H");
    printf("=== AJUSTES ===\n");
    printf("Ruta actual: %s\n\n", config ? config->download_path : "sdmc:/");

    for (int i = 0; i < item_count; i++) {
        printf(i == state->selected_setting ? "> " : "  ");
        printf("%s\n", items[i]);
    }

    printf("\n\nControles:\n");
    printf("^ / v - Navegar\n");
    printf("A - Seleccionar\n");
    printf("B - Volver\n");
}

void ui_draw_path_picker(UIState *state, AppConfig *config) {
    printf("\x1b[2J");
    printf("\x1b[0;0H");
    printf("=== SELECCIONAR RUTA DE DESCARGA ===\n");
    printf("Ruta actual: %s\n\n", state->path_nav ? path_navigator_get_current(state->path_nav) : "");

    if (state->path_nav) {
        int count = path_navigator_get_entry_count(state->path_nav);
        char **entries = path_navigator_get_entries(state->path_nav);

        if (count == 0 || !entries) {
            printf("No hay entradas disponibles o no se puede acceder a la ruta.\n");
        } else {
            ui_ensure_scroll(&state->path_scroll_offset, state->path_nav->selected_entry, count);
            int start = state->path_scroll_offset;
            int end = start + 10;
            if (end > count) {
                end = count;
            }
            if (start > 0) {
                printf("  ...\n");
            }
            for (int i = start; i < end; i++) {
                printf(i == state->path_nav->selected_entry ? "> " : "  ");
                printf("%s\n", entries[i]);
            }
            if (end < count) {
                printf("  ...\n");
            }
        }
    } else {
        printf("No se puede abrir la ruta.\n");
    }

    printf("\n\nControles:\n");
    printf("^ / v - Navegar\n");
    printf("A - Entrar en directorio / Volver (..)\n");
    printf("X - Crear carpeta\n");
    printf("Y - Seleccionar esta ruta\n");
    printf("B - Volver\n");
}

void ui_draw_message(UIState *state) {
    printf("\x1b[2J");
    printf("\x1b[0;0H");
    printf("=== MENSAJE ===\n\n");
    printf("%s\n", state->message);
    printf("\nPresiona B para continuar.\n");
}
