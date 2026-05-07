#include "repository.h"

RepositoryManager* repository_manager_create(void) {
    RepositoryManager *manager = (RepositoryManager*)malloc(sizeof(RepositoryManager));
    if (manager) {
        manager->repo_count = 0;
        manager->repositories = NULL;
    }
    return manager;
}

void repository_manager_destroy(RepositoryManager *manager) {
    if (manager) {
        for (int i = 0; i < manager->repo_count; i++) {
            if (manager->repositories[i].items) {
                free(manager->repositories[i].items);
            }
        }
        if (manager->repositories) {
            free(manager->repositories);
        }
        free(manager);
    }
}

void repository_add(RepositoryManager *manager, const char *name, const char *id) {
    if (!manager || !name || !id) return;
    
    Repository *new_repos = (Repository*)realloc(manager->repositories, 
                                                  sizeof(Repository) * (manager->repo_count + 1));
    if (new_repos) {
        manager->repositories = new_repos;
        Repository *repo = &manager->repositories[manager->repo_count];
        
        strncpy(repo->name, name, sizeof(repo->name) - 1);
        repo->name[sizeof(repo->name) - 1] = '\0';
        
        strncpy(repo->id, id, sizeof(repo->id) - 1);
        repo->id[sizeof(repo->id) - 1] = '\0';
        
        repo->item_count = 0;
        repo->items = NULL;
        
        manager->repo_count++;
    }
}

Repository* repository_get(RepositoryManager *manager, int index) {
    if (!manager || index < 0 || index >= manager->repo_count) {
        return NULL;
    }
    return &manager->repositories[index];
}

void repository_add_item(Repository *repo, const char *name, const char *path, int is_dir) {
    if (!repo || !name || !path) return;
    
    RepositoryItem *new_items = (RepositoryItem*)realloc(repo->items,
                                                         sizeof(RepositoryItem) * (repo->item_count + 1));
    if (new_items) {
        repo->items = new_items;
        RepositoryItem *item = &repo->items[repo->item_count];
        
        strncpy(item->name, name, sizeof(item->name) - 1);
        item->name[sizeof(item->name) - 1] = '\0';
        
        strncpy(item->path, path, sizeof(item->path) - 1);
        item->path[sizeof(item->path) - 1] = '\0';
        
        item->is_dir = is_dir;
        
        repo->item_count++;
    }
}

static int repository_item_compare(const void *a, const void *b) {
    const RepositoryItem *itemA = (const RepositoryItem *)a;
    const RepositoryItem *itemB = (const RepositoryItem *)b;

    if (itemA->is_dir != itemB->is_dir) {
        return itemB->is_dir - itemA->is_dir;
    }
    return strcmp(itemA->name, itemB->name);
}

void repository_clear_items(Repository *repo) {
    if (repo && repo->items) {
        free(repo->items);
        repo->items = NULL;
        repo->item_count = 0;
    }
}

void repository_sort_items(Repository *repo) {
    if (!repo || !repo->items || repo->item_count <= 1) return;
    qsort(repo->items, repo->item_count, sizeof(RepositoryItem), repository_item_compare);
}

int repository_populate_by_url(Repository *repo) {
    if (!repo) return -1;
    repository_clear_items(repo);
    
    // Fake loader for archive.org-like URLs
    if (strstr(repo->id, "archive.org")) {
        repository_add_item(repo, "dogz.nds", "/dogz.nds", 0);
        repository_add_item(repo, "readme.txt", "/readme.txt", 0);
        repository_add_item(repo, "manuals", "/manuals", 1);
        repository_add_item(repo, "screenshots", "/screenshots", 1);
        return 0;
    }
    
    // Fallback a un repository básico si no es URL
    repository_add_item(repo, "file1.dat", "/file1.dat", 0);
    repository_add_item(repo, "file2.zip", "/file2.zip", 0);
    repository_add_item(repo, "docs", "/docs", 1);
    return 0;
}
