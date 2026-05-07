# Guía de Desarrollo - Archive.org Navigator

## Arquitectura General

La aplicación está dividida en módulos independientes que pueden evolucionar sin afectar los demás:

```
┌─────────────────────────────────┐
│         main.c (Loop)           │
│  - Gestiona estado global       │
│  - Input handler                │
│  - Render loop                  │
└────────────────┬────────────────┘
                 │
        ┌────────┼────────┐
        │        │        │
   ┌────▼───┐ ┌──▼────┐ ┌─▼─────────┐
   │ UI (.c)│ │Repos  │ │Archive.org│
   │        │ │.c     │ │(futuro)   │
   └────────┘ └───────┘ └───────────┘
```

## Módulos

### 1. **repository.h/c** - Sistema de Datos
Maneja la estructura de repositorios y archivos.

**Responsabilidades:**
- Crear/destruir gestores de repositorios
- Agregar/obtener repositorios
- Agregar/limpiar items dentro de repositorios

**Funciones principales:**
```c
RepositoryManager* repository_manager_create();
void repository_add(RepositoryManager *manager, const char *name, const char *id);
void repository_add_item(Repository *repo, const char *name, const char *path, int is_dir);
```

### 2. **ui.h/c** - Sistema de Interfaz
Maneja la renderización y input.

**Responsabilidades:**
- Mantener estado de UI (selección, página actual, etc.)
- Renderizar menús
- Procesar input del gamepad

**Funciones principales:**
```c
UIInput ui_handle_input(PadState *pad);
void ui_draw_main_menu(RepositoryManager *manager, UIState *state);
void ui_draw_repository_browser(Repository *repo, UIState *state);
```

### 3. **fake_data.h/c** - Datos de Prueba
Carga repositorios fake para testing.

**Responsabilidades:**
- Crear estructura de datos de ejemplo
- Facilitar testing sin dependencias externas

### 4. **archive_org.h/c** - Integración (Futuro)
Será responsable de comunicar con Archive.org.

**Responsabilidades (planeadas):**
- Hacer requests HTTP a Archive.org
- Parsear respuestas JSON
- Descargar archivos

## Flujo de Ejecución

```
1. main() inicializa:
   - Consola y gamepad
   - RepositoryManager
   - UIState
   
2. Main Loop:
   a) Procesar input → UIInput
   b) Actualizar estado basado en input
   c) Renderizar basado en estado
   d) Mostrar en pantalla
   
3. En menú principal:
   - Navegar con D-Pad ↑↓
   - Seleccionar con A
   - El estado cambia a is_browsing = 1
   
4. En navegación:
   - Navegar items con D-Pad ↑↓
   - Entrar carpetas con A
   - Volver con B
   - Actualizar current_path
```

## Cómo Agregar Nuevas Características

### Agregar un nuevo repositorio
En `fake_data.c`:
```c
repository_add(manager, "Mi Repositorio", "my_repo_id");
Repository *repo = repository_get(manager, last_index);
repository_add_item(repo, "Carpeta 1", "/folder1", 1);
repository_add_item(repo, "Archivo.txt", "/file.txt", 0);
```

### Agregar funcionalidad de descarga
1. Extender `ui_state` con información de descarga
2. En el manejador de input, detectar cuando se selecciona un archivo
3. Iniciar descarga asincrónica
4. Mostrar barra de progreso

```c
// En UIState
typedef struct {
    // ... campos existentes
    int downloading;
    int download_progress;
    char download_filename[256];
} UIState;
```

### Integrar con Archive.org
1. Implementar funciones en `archive_org.c`
2. En `fake_data.c`, reemplazar con llamadas reales:

```c
// Cambiar de:
repository_add(manager, "Arcade", "arcade_collection");

// A:
archive_org_search("arcade_collection", &response);
repository_add(manager, response.title, response.identifier);
archive_org_list_files(response.identifier, repo);
```

3. Manejar errores de red en `ui.c`

## Mejoras Futuras Sugeridas

### Corto Plazo
- [ ] Persistencia de preferencias (repositorios favoritos)
- [ ] Buscar dentro de un repositorio
- [ ] Ordenar archivos por nombre/tamaño/fecha
- [ ] Mostrar más información de archivos

### Mediano Plazo
- [ ] Implementar descarga HTTP
- [ ] Barra de progreso de descarga
- [ ] Caché de metadatos
- [ ] Historial de navegación reciente

### Largo Plazo
- [ ] Interfaz gráfica mejorada
- [ ] Soporte para múltiples fuentes (no solo Archive.org)
- [ ] Sistema de categorías personalizado
- [ ] Sincronización en la nube de preferencias

## Testing

### Para probar cambios:
```bash
make clean
make
# Copiar Rammus.nro a la Switch
```

### Para debug:
- Los printf() van a la consola
- Mantén abierta la consola debug en la Switch durante pruebas
- Usa el botón B para volver atrás cuando algo falla

## Recursos

- **libnx documentation**: `/opt/devkitpro/libnx/docs/`
- **Switch devkitPro**: https://devkitpro.org/
- **Archive.org API**: https://archive.org/developers/
- **Nintendo Switch devkitPro**: https://github.com/devkitPro/
