#ifndef UI_H
#define UI_H

#include <switch.h>
#include "repository.h"
#include "path_navigator.h"
#include "config.h"
#include "downloader.h"
#include "save_manager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

typedef enum {
    UI_MODE_MAIN,
    UI_MODE_BROWSER,
    UI_MODE_SETTINGS,
    UI_MODE_PATH_PICKER,
    UI_MODE_ADD_REPO_URL,
    UI_MODE_ADD_REPO_NAME,
    UI_MODE_DOWNLOAD_INFO,
    UI_MODE_COPIAS,         // Renombrado de UI_MODE_FUTURE
    UI_MODE_REPO_MANAGER,   // Nuevo: Lista de gestión
    UI_MODE_REPO_SUBMENU,   // Nuevo: Opciones de un repo (Editar/Borrar)
    UI_MODE_SAVE_PATH_PICKER, // Nuevo: Selector de ruta para copias de guardado
    UI_MODE_RESTORE_MENU,   // Menú de restauración
    UI_MODE_SAVE_OPTIONS,   // Opciones para un save (Restaurar, Duplicar, Borrar)
    UI_MODE_CONFIRM_ACTION  // Diálogo de confirmación
} UIMode;

typedef struct UIState {
    UIMode mode;
    SDL_Renderer *renderer;
    TTF_Font *font_main;
    TTF_Font *font_small;
    
    SDL_Texture *tex_repo;
    SDL_Texture *tex_folder;
    SDL_Texture *tex_file;
    SDL_Texture *tex_link;
    SDL_Texture *tex_logo;      // Logo de la aplicación
    SDL_Texture *tex_game_icon; // Para las carátulas de los juegos
    SDL_Texture *tex_settings;

    int active_tab; // 0: Repos, 1: Futuro, 2: Ajustes
    int selected_repo;
    int selected_item;
    int selected_setting;
    char current_path[4096]; // Increased size to silence truncation warnings and accommodate deep paths
    char new_repo_url[512];
    char new_repo_name[128];
    char message[256];
    int show_message;
    PathNavigator *path_nav;
    int browser_scroll_offset;
    int path_scroll_offset;
    
    // Gestión de Saves
    SaveManager *save_mgr;
    
    // Progreso de descarga
    int is_downloading;
    double download_progress; // 0.0 a 1.0
    char download_status[128];

    // Estado de edición
    int editing_repo_index; // Índice del repo que estamos editando en el manager
    int is_creating_new;    // Flag para saber si estamos en flujo de creación

    // Estado para submenú de saves
    int selected_save_option; // 0: Restaurar, 1: Duplicar, 2: Borrar
    int pending_action;       // Para confirmación
    char target_save_path[4096]; // Ruta del save seleccionado
} UIState;

typedef struct {
    int action;
    int param;
} UIInput;

#define UI_ACTION_NONE 0
#define UI_ACTION_SELECT 1
#define UI_ACTION_BACK 2
#define UI_ACTION_EXIT 3
#define UI_ACTION_SETTINGS 4
#define UI_ACTION_SET_PATH 5
#define UI_ACTION_NEW_FOLDER 6
#define UI_ACTION_TAB_LEFT 7
#define UI_ACTION_TAB_RIGHT 8

void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color);
void ui_draw_tabs(UIState *state);
UIState* ui_state_create(void);
void ui_state_destroy(UIState *state);
UIInput ui_handle_input(PadState *pad);
int ui_show_text_input(char *out, size_t size, const char *guide_text);
void ui_draw_main_menu(RepositoryManager *manager, UIState *state, AppConfig *config);
void ui_draw_repository_browser(Repository *repo, UIState *state, AppConfig *config);
void ui_draw_settings_menu(UIState *state, AppConfig *config);
void ui_draw_path_picker(UIState *state, AppConfig *config);
void ui_draw_progress(UIState *state);
void ui_draw_message(UIState *state);
void ui_draw_saves_menu(UIState *state); // Nueva función para la pestaña "Copias"
void ui_draw_repo_manager(RepositoryManager *manager, UIState *state);
void ui_draw_repo_submenu(Repository *repo, UIState *state);
void ui_draw_restore_menu(UIState *state);
void ui_draw_save_options(UIState *state);
void ui_draw_confirm_dialog(UIState *state, const char *action_text);

#endif
