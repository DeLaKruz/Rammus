#ifndef UI_H
#define UI_H

#include <switch.h>
#include "repository.h"
#include "path_navigator.h"
#include "config.h"
#include "downloader.h"

typedef enum {
    UI_MODE_MAIN,
    UI_MODE_BROWSER,
    UI_MODE_SETTINGS,
    UI_MODE_PATH_PICKER,
    UI_MODE_ADD_REPO_URL,
    UI_MODE_ADD_REPO_NAME,
    UI_MODE_DOWNLOAD_INFO
} UIMode;

typedef struct {
    UIMode mode;
    int selected_repo;
    int selected_item;
    int selected_setting;
    char current_path[1024];
    char new_repo_url[512];
    char new_repo_name[128];
    char message[256];
    int show_message;
    PathNavigator *path_nav;
    int browser_scroll_offset;
    int path_scroll_offset;
    DownloadProgress download_progress;
    int is_downloading;
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

UIState* ui_state_create(void);
void ui_state_destroy(UIState *state);
UIInput ui_handle_input(PadState *pad);
int ui_show_text_input(char *out, size_t size, const char *guide_text);
void ui_draw_main_menu(RepositoryManager *manager, UIState *state, AppConfig *config);
void ui_draw_repository_browser(Repository *repo, UIState *state, AppConfig *config);
void ui_draw_settings_menu(UIState *state, AppConfig *config);
void ui_draw_path_picker(UIState *state, AppConfig *config);
void ui_draw_message(UIState *state);

#endif
