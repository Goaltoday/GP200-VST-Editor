# Compilar VST3 y aplicación independiente — 11/10/2026

## Compilar desde GitHub

Sube al repositorio los cambios de `CMakeLists.txt` (FORMATS VST3 Standalone) y `.github/workflows/build-windows.yml`. En GitHub abre **Actions → Build Windows VST3 + Standalone → Run workflow**. También se ejecuta al hacer push en main, develop o feature/*. Cuando termine correctamente, descarga el artefacto **GP200-VST-Windows-VST3-Standalone** de esa ejecución; contiene `GP200 VST.vst3` y `GP200 VST.exe`. El artefacto queda disponible 14 días. Si la acción falla, abre el job y revisa el paso que marque error.

## Compilar localmente

Conserva external/JUCE y su revisión actual. Copia CMakeLists.txt en la raíz del proyecto y build_windows.cmd junto a él. Cada compilación del proyecto completo genera ambos formatos, compartiendo fuentes. También puedes usar los fuentes completos incluidos.

En CMD, desde la raíz:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

O ejecuta `build_windows.cmd`, que reconfigura antes de compilar y detiene el proceso si hay errores.

Salidas Windows con el directorio build por defecto:

- `build\GP200Plugin_artefacts\Release\VST3\GP200 VST.vst3` (copiar la carpeta completa para instalar el plugin).
- `build\GP200Plugin_artefacts\Release\Standalone\GP200 VST.exe` (abrir sin DAW).

Un build dirigido sólo al target GP200Plugin_VST3 genera sólo ese formato. El script compila explícitamente GP200Plugin_VST3 y GP200Plugin_Standalone. No cambia el nombre, identidad o versión del plugin ni requiere nuevo firmware.

El EXE usa el wrapper Standalone estándar de JUCE: mismo editor y puertos USB MIDI propios del proyecto. Sus ajustes de audio permiten seleccionar dispositivo/entrada para afinador y ToneMatch; no es un motor de efectos GP200 en el PC. El DSP de la GP200 sigue en el pedal. Si otro programa ocupa sus puertos MIDI, ciérralo antes de conectar.

CONFIRMADO EN ARCHIVOS: FORMATS VST3 Standalone; comparten GP200Plugin, sus fuentes, recursos y bibliotecas; no se modifica ningún .cpp/.h ni el tráfico MIDI.

HARDWARE/USUARIO REPORTADO: el usuario indica que ha compilado la revisión MIDI Sync anterior. No consta log, hash del binario ni confirmación de todas las funciones.

PRUEBA LOCAL: cambio de configuración y comparación byte a byte del resto de fuentes con el ZIP aportado. No se ha compilado un EXE Windows en este entorno.

PENDIENTE: compilar ambos targets con Visual Studio 2022; abrir EXE sin DAW; comprobar conexión/reconexión, Scenes, ALIGN/PHASE, guardado; comprobar entrada de audio/afinador/ToneMatch y ausencia de cortes bajo tráfico. Mantener las comprobaciones físicas pendientes de MIDI Sync 1.
