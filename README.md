# Archive.org Navigator para Nintendo Switch

Una aplicación homebrew para Nintendo Switch que permite navegar y descargar contenido de repositorios (próximamente Archive.org).

## Estado Actual

La aplicación está en fase **BETA** con funcionalidad de navegación simulada usando datos fake.

### ✅ Funcionalidades Implementadas

1. **Menú Principal**: Muestra lista de repositorios disponibles
2. **Navegación**: Sistema D-Pad para seleccionar repositorios
3. **Exploración**: Navega por carpetas dentro de cada repositorio
4. **Visualización**: Muestra información de archivos

### 📥 Controles

| Botón | Acción |
|-------|--------|
| **↑ / ↓** | Navegar en la lista |
| **A** | Seleccionar/Entrar |
| **B** | Volver atrás |
| **+** | Salir de la aplicación |

## Estructura del Proyecto

```
Rammus/
├── source/
│   ├── main.c           # Lógica principal
│   ├── repository.c     # Sistema de repositorios
│   ├── ui.c            # Sistema de UI
│   └── fake_data.c     # Datos de prueba
├── include/
│   ├── repository.h     # Definiciones de repositorios
│   ├── ui.h            # Definiciones de UI
│   └── fake_data.h     # Definiciones de datos fake
├── Makefile            # Sistema de compilación
└── build/              # Archivos compilados
```

## Repositorios Fake Disponibles

1. **Juegos Clásicos** - Arcade, Atari, NES, Game Boy
2. **Documentación** - Manuales, Guías, README
3. **Películas** - Clásicas, Documentales, Cortometrajes
4. **Música** - Rock, Jazz, Clásica, Podcasts

## Compilación

```bash
make clean
make
```

Esto genera `Rammus.nro` que puedes copiar a tu Switch.

## Próximas Mejoras

- [ ] Integración real con Archive.org API
- [ ] Sistema de descargas HTTP
- [ ] Caché de repositorios
- [ ] Mejor renderizado de UI
- [ ] Búsqueda de contenido
- [ ] Historial de navegación
- [ ] Configuración de repositorios

## Notas para el Desarrollo

### Agregar nuevo repositorio

En `fake_data.c`, usa:

```c
repository_add(manager, "Nombre", "id_unique");
Repository *repo = repository_get(manager, index);
repository_add_item(repo, "Item Name", "/path", is_directory);
```

### Estructura de datos

- `Repository`: Contiene nombre, ID y lista de items
- `RepositoryItem`: Nombre, ruta, y tipo (carpeta/archivo)
- `UIState`: Mantiene el estado actual de la UI

## Licencia

Homebrew para educational purposes.
