# Cómo Usar Archive.org Navigator en Nintendo Switch

## Descripción

Esta aplicación permite navegar repositorios de Archive.org directamente desde tu Nintendo Switch y descargar archivos a tu tarjeta SD.

## Estructura de Carpetas

La app crea automáticamente:
```
sdmc:/switch/archive_navigator/
├── config.json (guarda configuración y repositorios)
```

Los archivos descargados se guardan en la ruta que configures.

## Pasos Iniciales

1. **Configurar ruta de descarga**:
   - Presiona X en el menú principal → Ajustes
   - Selecciona "Elegir ruta de descarga"
   - Navega por la SD (empieza en `sdmc:/switch/`), presiona Y para confirmar

2. **Añadir Repositorio**:
   - En Ajustes, selecciona "Añadir repositorio"
   - Ingresa la **URL completa** de Archive.org (ej: `https://archive.org/download/world-ends-with-you-the/Roms%20ds/`)
   - Ingresa un **Nombre** para mostrar en el menú
   - El repo aparecerá en el menú principal

3. **Navegar y Descargar**:
   - Selecciona un repo con ^ y v, presiona A
   - La app descarga la lista de archivos desde Archive.org
   - Navega los archivos con ^ y v
   - Presiona A en un archivo para descargarlo desde internet a tu ruta configurada

4. **Eliminar Repositorio**:
   - En Ajustes, selecciona "Eliminar repositorio"
   - Borra el repo seleccionado

## Ejemplo Práctico

1. Configura ruta descarga: `sdmc:/downloads/games/`
2. Añade repo con URL: `https://archive.org/download/world-ends-with-you-the/Roms%20ds/`
3. Nombre: "The World Ends With You"
4. Selecciona el repo y verás todos los archivos disponibles online
5. Presiona A en `dogz.nds` → se descarga desde Archive.org a `sdmc:/downloads/games/dogz.nds`

## Controles

| Botón | Acción |
|-------|--------|
| **↑ / ↓** | Navegar entre items |
| **A** | Seleccionar / Descargar |
| **B** | Volver / Cancelar |
| **X** | Abrir Ajustes |
| **Y** | Confirmar ruta de descarga |
| **+** | Salir de la app |

## Notas

- **Requiere conexión a internet** para navegar repositorios y descargar
- Los repositorios se guardan en `sdmc:/switch/archive_navigator/config.json`
- Las descargas son REALES desde Archive.org (no simuladas)
- Solo HTTP (no HTTPS por limitaciones de la consola)
- La navegación muestra todos los archivos del repositorio en un nivel plano
