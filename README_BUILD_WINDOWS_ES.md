> Documento vigente para distribución VST. Firmware vigente: CAPTURE LED. Este paquete consolida documentación y conserva el código existente.

# Compilar VST3 y Standalone desde GitHub — GP200 VST

## Integración requerida

En el repositorio del editor, actualiza `CMakeLists.txt` para usar `FORMATS VST3 Standalone` y sustituye `.github/workflows/build-windows.yml` por el workflow actualizado de la rama Standalone (no incluido en el overlay documental adjunto). Conserva el submódulo JUCE y su revisión fijada en el repositorio. No hace falta modificar fuentes C++ ni firmware para añadir el formato Standalone.

## Ejecutar

1. Sube los cambios a `main`, `develop` o `feature/*`, o abre **Actions → Build Windows VST3 + Standalone → Run workflow**.
2. Espera a que termine el job `build-windows` en Windows 2022.
3. Descarga el artefacto `GP200-VST-Windows-VST3-Standalone` de esa ejecución.

El ZIP contiene `GP200 VST.vst3` (bundle completo) y `GP200 VST.exe`. GitHub Actions conserva el artefacto durante 14 días. Un fallo de compilación o la ausencia de cualquiera de las dos salidas hace fallar el job. Si la acción falla, abre el paso marcado en rojo y conserva el log completo.

## Build local

Desde el directorio del proyecto VST en CMD:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

La salida habitual es `build\GP200Plugin_artefacts\Release\VST3\GP200 VST.vst3` y `build\GP200Plugin_artefacts\Release\Standalone\GP200 VST.exe`.

## Estado de verificación

La configuración de CMake y workflow debe comprobarse al integrar los dos archivos actualizados. En este árbol siguen estando los archivos VST3-only; su actualización está pendiente. El artefacto de GitHub sólo se considera generado cuando una ejecución nueva termina correctamente. Después, probar apertura del EXE, MIDI bidireccional, Scene, ALIGN/PHASE, presets y dispositivos de audio. No hay aún medición de audio/latencia del EXE.

## Actualización Scene Remote — archivos incluidos

Esta entrega ya incorpora `FORMATS VST3 Standalone` y el workflow que exige y publica ambos resultados. Sustituir CMakeLists y el workflow junto a las fuentes de los controles. Conservar el submódulo JUCE restaurado en el repositorio, revisión `3cc224c3e84d6acbabf62358ba1ccbf133cd2095`. La ejecución y aceptación de los binarios Windows sigue pendiente.
