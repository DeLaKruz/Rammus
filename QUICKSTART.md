# QUICKSTART - Archive.org Navigator

## ⚡ Inicio Rápido

### Compilar
```bash
cd c:\devkitPro\Rammus
make clean && make
```

El archivo `Rammus.nro` estará listo en el directorio raíz.

### Instalar
1. Copia `Rammus.nro` a tu Switch (usando Dolphin u otro método)
2. Ejecuta desde HBMenu

### Usar

**En Menú Principal:**
- **↑/↓** = Navegar repositorios
- **A** = Entrar en repositorio
- **+** = Salir

**Navegando Archivos:**
- **↑/↓** = Navegar archivos/carpetas
- **A** = Entrar en carpeta o ver info
- **B** = Volver atrás
- **+** = Salir

## 📋 Contenido Fake Disponible

- **Juegos Clásicos**: Arcade, Atari, NES, Game Boy
- **Documentación**: Manuals, Guides, README.txt
- **Películas**: Classic, Documentaries, Shorts
- **Música**: Rock, Jazz, Classical, Podcasts

## 📚 Documentación

- `README.md` - Visión general del proyecto
- `USAGE.md` - Guía detallada de uso
- `DEVELOPMENT.md` - Guía para desarrolladores
- `source/fake_data.c` - Cómo agregar más datos fake
- `include/archive_org.h` - Futura integración con Archive.org

## 🔧 Desarrollo

Para agregar tu propio contenido fake, edita `source/fake_data.c`:

```c
repository_add(manager, "Mi Repo", "my_repo");
Repository *repo = repository_get(manager, 4);
repository_add_item(repo, "Carpeta", "/folder", 1);  // 1 = carpeta
repository_add_item(repo, "Archivo", "/file", 0);   // 0 = archivo
```

Luego recompila: `make clean && make`

## 📦 Estructura

```
Rammus/
├── source/          # Código fuente
├── include/         # Headers
├── build/          # Compilados
├── Makefile        # Build system
├── README.md       # Descripción general
├── USAGE.md        # Cómo usar
├── DEVELOPMENT.md  # Guía desarrollo
└── QUICKSTART.md   # Este archivo
```

## ✨ Próximas Mejoras

Cuando implementemos Archive.org real:
- [ ] Búsqueda en línea
- [ ] Descargas de archivos
- [ ] Caché de repositorios
- [ ] Repositorios personalizados
- [ ] Mejor UI

## 🐛 Bugs Conocidos

- Warnings de compilación (menores, no afecta funcionamiento)
- UI muy simple (texto, sin gráficos)
- Datos completamente fake (no conecta a Internet)

## 💡 Tips

- Presiona B varias veces si te pierdes en la navegación
- El menú principal siempre está a presionar B repetidamente
- La app limpia la pantalla en cada frame (sin ghosting)

---

**¡Ya está listo para usar!** Compila, copia a tu Switch y explora. 🎮
