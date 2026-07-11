# GP200 Studio — refactorización v1

Este paquete es una refactorización de mantenimiento basada en los archivos entregados.
Conserva el protocolo MIDI y el comportamiento actual de la interfaz, pero corrige los
problemas de consistencia y mantenibilidad más importantes.

## Cambios principales

- Las descripciones se buscan por el ID de 32 bits, evitando colisiones como `Tube`
  (DST frente a DLY) y `Dark Twin` (AMP frente a CAB).
- Los diseños de parámetros repetidos se comparten en vez de copiarse cientos de veces.
- Los tipos de parámetro usan un enum (`continuous`, `toggle`, `choice`) en lugar de
  cadenas como `"knob"`, `"switch"` y el typo `"combox"`.
- Los tamaños y offsets comunes del protocolo están en `GP200Constants.h`.
- `MidiConnection` reutiliza `MidiDeviceScanner`, protege el estado compartido y expone
  contadores de revisión para actualizar la interfaz de forma más eficiente.
- El editor consulta el estado a 20 Hz durante el funcionamiento normal y usa 100 Hz
  únicamente mientras restaura un preset. Ya no genera firmas Base64 continuamente.
- Un preset restaurado solo se adopta como estado confirmado al terminar la secuencia.
- Se eliminaron helpers sin uso, includes duplicados y una declaración duplicada accidental.
- Todo el código está formateado mediante el archivo `.clang-format` incluido.

## Configuración en Windows

El `CMakeLists.txt` detecta automáticamente el diseño actual `external/JUCE`. Con esa
estructura:

```cmd
cd D:\Development\Projects\GP200Studio_refactor_v1
cmake -S . -B build
```

También puedes indicar otra copia de JUCE:

```cmd
cmake -S . -B build -DJUCE_SOURCE_DIR=D:\Development\JUCE
```

JUCE también puede colocarse en `external/JUCE` o `JUCE` junto a este README. Si está
instalado como paquete CMake, no hace falta indicar `JUCE_SOURCE_DIR`.

Para generar únicamente VST3:

```cmd
cmake -S . -B build -DGP200_BUILD_STANDALONE=OFF
```

## Compilación

El paquete incluye dos scripts:

```cmd
build_debug.cmd
build_release.cmd
```

El comando manual equivalente para Debug es:

```cmd
cmake --build build --config Debug
```

Ruta esperada del VST3:

```text
build\GP200Plugin_artefacts\Debug\VST3\GP200 Studio.vst3
```

## Validación de los catálogos

Python solo es necesario para esta comprobación estática opcional:

```cmd
python tools\validate_catalogs.py
```

Comprueba IDs ordenados y únicos, referencias inexistentes, tamaños de layouts y restos
de los tipos antiguos basados en cadenas.

## Migración recomendada

Mantén intacto el proyecto que funciona. Compila este paquete en otra carpeta y comprueba,
en este orden: conexión, dump de preset, parámetros, ON/OFF, reordenamiento, nombres User
IR/SnapTone y restauración completa. Sustituye la rama anterior únicamente después de esas
pruebas.

## Identidad del plugin

Los valores predeterminados son:

```text
GP200_MANUFACTURER_CODE=GpSt
GP200_PLUGIN_CODE=Gp20
GP200_BUNDLE_ID=com.gp200studio.plugin
```

No pude recuperar tu `CMakeLists.txt` anterior. Si sus códigos eran diferentes, conserva
los originales para que Studio One no interprete esta compilación como otro plugin:

```cmd
cmake -S . -B build ^
  -DGP200_MANUFACTURER_CODE=GpSt ^
  -DGP200_PLUGIN_CODE=Gp20 ^
  -DGP200_BUNDLE_ID=com.gp200studio.plugin
```

## Estado de verificación

La validación de catálogos, el formato y la configuración CMake se han ejecutado
correctamente. No fue posible producir el VST3 completo en el entorno de generación porque
no contiene tu copia de JUCE ni el compilador MSVC de Windows. La primera compilación
autoritativa debe realizarse en tu equipo.
