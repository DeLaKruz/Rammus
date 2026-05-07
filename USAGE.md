# Cómo Usar - Archive.org Navigator

## Pantalla Inicial - Menú Principal

Cuando inicias la app, verás:

```
=== ARCHIVE.ORG NAVIGATOR ===
Repositorios disponibles:

> [1] Juegos Clásicos      ← Seleccionado
  [2] Documentación
  [3] Películas
  [4] Música


Controles:
^ / v - Navegar
A - Seleccionar
+ - Salir
```

## Navegación Básica

### 1. Seleccionar un Repositorio

- Usa **D-Pad ↑/↓** para moverte entre repositorios
- Observa el `>` que indica qué está seleccionado
- Presiona **A** para entrar

### 2. Explorar Contenido

Una vez dentro, verás:

```
=== Juegos Clásicos ===
Ruta: /

> [CARPETA] Arcade
  [CARPETA] Atari
  [CARPETA] NES
  [CARPETA] Game Boy


Controles:
^ / v - Navegar
A - Entrar/Descargar
B - Atrás
```

### 3. Entrar en Carpetas

- Navega a una carpeta (aparecerá un `>` antes de ella)
- Presiona **A** para entrar
- La ruta se actualiza: `/` → `/Arcade/`

### 4. Ver Información de Archivos

Si seleccionas un archivo y presionas **A**:

```
=== INFORMACIÓN ===

Nombre: README.txt
Ruta: /README.txt
Tipo: Archivo
```

Espera 2 segundos y vuelve al navegador.

### 5. Volver Atrás

- Presiona **B** para salir de una carpeta
- Presiona **B** nuevamente en menú principal para volver a la app anterior
- Presiona **+** en cualquier momento para salir

## Estructura de Datos Fake

### Repositorio 1: Juegos Clásicos
- Arcade/
- Atari/
- NES/
- Game Boy/

### Repositorio 2: Documentación
- Manuals/
- Guides/
- README.txt (archivo)

### Repositorio 3: Películas
- classic/
- documentaries/
- shorts/

### Repositorio 4: Música
- rock/
- jazz/
- classical/
- podcasts/

## Flujo Completo de Ejemplo

```
1. Inicia la app
   → Ves menú principal con 4 repositorios

2. Presiona A en "Juegos Clásicos"
   → Entras en la carpeta, ves Arcade, Atari, NES, Game Boy

3. Navega a NES y presiona A
   → Entras en /NES/ (nota la ruta actualizada)

4. Presiona B
   → Vuelves a / (menú raíz del repositorio)

5. Presiona B nuevamente
   → Vuelves al menú principal

6. Selecciona "Documentación"
   → Ves: Manuals/, Guides/, README.txt

7. Selecciona README.txt y presiona A
   → Ves información del archivo por 2 segundos

8. Presiona + en cualquier momento para salir
```

## Limpieza de Pantalla

La app limpia la pantalla y renderiza desde cero en cada frame, así que:
- No habrá texto duplicado o confuso
- Siempre verás la pantalla actual correcta
- Los controles son responsivos

## Estado Actual

⚠️ **Nota importante**: Esto es un prototipo funcional con datos simulados.

- ✅ Navegación está 100% funcional
- ✅ UI muestra información correctamente
- ✅ Input es responsivo
- ⏳ Descargas no implementadas aún
- ⏳ Archive.org real no está integrado (solo datos fake)

## Próximas Sesiones

Cuando integremos Archive.org:
1. Reemplazaremos los datos fake con búsquedas reales
2. Agregaremos botón para descargar archivos
3. Mostraremos barra de progreso de descarga
4. Permitiremos agregar repositorios personalizados

## Troubleshooting

| Problema | Solución |
|----------|----------|
| App no responde | Presiona + para salir, reinicia |
| Pantalla confusa | Presiona B varias veces para volver al menú |
| No se ve texto | La consola debe estar activada en Debug Mode |
| Crashes | Revisa los logs de debug en la consola |

