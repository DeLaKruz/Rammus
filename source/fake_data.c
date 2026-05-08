#include "fake_data.h"

void populate_fake_data(RepositoryManager *manager) {
    // Repositorio 1: Juegos Clásicos
    repository_add(manager, "Juegos Clásicos", "classic_games", "sdmc:/roms/classic");
    Repository *repo1 = repository_get(manager, 0);
    
    repository_add_item(repo1, "Arcade", "/arcade", 1);
    repository_add_item(repo1, "Atari", "/atari", 1);
    repository_add_item(repo1, "NES", "/nes", 1);
    repository_add_item(repo1, "Game Boy", "/gameboy", 1);
    
    // Repositorio 2: Documentación
    repository_add(manager, "Documentación", "documentation", "sdmc:/docs");
    Repository *repo2 = repository_get(manager, 1);
    
    repository_add_item(repo2, "Manuales", "/manuals", 1);
    repository_add_item(repo2, "Guías", "/guides", 1);
    repository_add_item(repo2, "README.txt", "/README.txt", 0);
    
    // Repositorio 3: Películas
    repository_add(manager, "Películas", "movies", "sdmc:/movies");
    Repository *repo3 = repository_get(manager, 2);
    
    repository_add_item(repo3, "Clásicas", "/classic", 1);
    repository_add_item(repo3, "Documentales", "/documentaries", 1);
    repository_add_item(repo3, "Cortometrajes", "/shorts", 1);
    
    // Repositorio 4: Música
    repository_add(manager, "Música", "music", "sdmc:/music");
    Repository *repo4 = repository_get(manager, 3);
    
    repository_add_item(repo4, "Rock", "/rock", 1);
    repository_add_item(repo4, "Jazz", "/jazz", 1);
    repository_add_item(repo4, "Clásica", "/classical", 1);
    repository_add_item(repo4, "Podcast", "/podcasts", 1);
}
