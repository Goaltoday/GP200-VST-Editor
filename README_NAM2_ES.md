# GP200 VST — NAM2 0.1.8

Nota de continuidad: para la actualización 0.1.9, lee también `README_EFFECT_BANK_ES.md`. Esta guía describe el alcance original de 0.1.8.

Esta entrega añade soporte de edición para los NAM A2 Lite 48 kHz M7OPT ya instalados con el generador Unified 1.3, perfiles GP-200 V180 y V182.

## Uso

1. Compila estas fuentes como VST3 o Standalone y sustituye tu compilación anterior. Conserva el checkout JUCE elegido para el proyecto. Se mantienen el nombre y los identificadores del plugin y el formato de estado DAW.
2. Del ZIP que produjo el HTML **para el BIN realmente instalado**, copia `GP200_MOD_SYNC.json` a la carpeta `GP200` de tus Documentos. En Windows suele ser `Documentos/GP200/GP200_MOD_SYNC.json`; si Documentos está redirigido, usa esa ubicación. El VST utiliza `juce::File::userDocumentsDirectory`.
3. Abre el editor. El JSON se lee al iniciar y cuando cambia. Los AMP que contienen NAM muestran su nombre y solamente **Input y Output**, ambos de 0 a 100, con doble clic a **50**. Un valor recibido del preset se muestra sin restablecerlo a 50.
4. Conecta por USB MIDI. La sincronización de CAB y CLO conserva los NAM importados. El estado indica que NAM procede del JSON y no está verificado por MIDI.
5. En Sound Clone, los destinos NAM aparecen como `[NAM - protected]` y no permiten cargar CLO ni renombrarlos con HOT2. Los destinos Factory stock o sin mapa CLO conocido también quedan deshabilitados. Los diez destinos SnapTone normales conservan su funcionamiento.
6. Después de regenerar o cambiar firmware, sustituye el JSON por el de esa nueva generación. Para retirar NAM importa el JSON de una generación sin NAM; no reutilices un archivo anterior.

No se cargan archivos `.nam` desde el VST: el HTML los integra en firmware. No hay carga HOT NAM en esta entrega. No hace falta modificar ni volver a generar el firmware para usar estos controles si ya tienes el Unified 1.3 adecuado instalado.

## Límite de identificación

El protocolo MS11 del firmware 1.3 solo anuncia el mapa CLO, CAB y PRE antiguo. Un registro AMP que no es CLO **no demuestra que sea stock**: también puede ser NAM. Por eso el refresco MIDI no elimina los perfiles NAM del JSON.

El VST no puede comprobar por MIDI que el SHA del JSON sea el del firmware instalado. Se conserva ese hash como procedencia, no como verificación del equipo. La detección automática de NAM sin archivo requerirá ampliar el protocolo del firmware en otro cambio.

Un JSON inválido conserva los últimos perfiles válidos en RAM y bloquea las operaciones Factory CLO. Un conflicto explícito entre el mapa CLO recibido y un destino NAM también rechaza esa actualización. Corrige el problema copiando el JSON que corresponda al firmware instalado. El mensaje `MOD_SYNC OK` no certifica los NAM.

## Qué incluye

- Fuentes completas VST3/Standalone, versión CMake 0.1.8.
- NAM2 separado de CLO5 en carga y escritura de caché.
- Validación de hasta cinco destinos NAM, nombres ASCII, dos controles, valor neutro y perfil.
- Restauración de metadatos en un proceso nuevo y conservación al reconectar.
- Perfil de parámetros continuo, sin selectores discretos heredados del AMP stock.
- Protección en interfaz, inicio de transferencia, constructores HOT1/HOT2 y comprobación anterior al commit.
- Rechazo de índices de parámetro reservados y valores NAM fuera de 0–100 o no finitos antes de enviar MIDI.
- Pruebas y handoff en `docs/NAM2_HANDOFF_ES.md` y `docs/nam2_validation`.

Se conservan Scenes, CHAIN/BLEND/ALIGN, PRE/WAH anteriores, IR/CAB, SnapTone, afinador y Tone Match. Este cambio no incorpora el banco genérico `GP200_EFFECT_BANK.json`, la transferencia del banco SC10 completo, DSP Load remoto ni correcciones de audio del firmware.

## Compilar

Necesitas Visual Studio 2022 con C++, CMake >=3.22 y el checkout JUCE del proyecto:

```bat
cmake -S . -B build -A x64 -DJUCE_SOURCE_DIR="C:/path/to/JUCE"
cmake --build build --config Release --target GP200Plugin_VST3 GP200Plugin_Standalone --parallel
```

Este paquete contiene fuentes, **no un VST3/EXE compilado**. El ZIP de origen no incluye JUCE ni fija su commit; no se ha seleccionado una revisión arbitraria. La compilación Windows, la apertura en DAW, el funcionamiento USB real y la presentación física de controles siguen pendientes.

## Pruebas locales

Con Python 3 y g++ con C++20:

```sh
python tests/run_tests.py
python tests/test_receive.py
python tools/validate_catalogs.py
```

`run_tests.py` incluye las pruebas NAM2. También puedes ejecutar `python tests/run_nam2_tests.py`. Estas compilan los `.cpp` reales de ModSync, las bases de datos y SoundClone; las pruebas de transporte extraen los métodos completos reales de MidiConnection. JUCE, JSON, archivos, puertos y reloj son dobles de prueba expresamente documentados. No equivalen a enlazar JUCE, ejecutar la GUI ni probar la pedalera.
