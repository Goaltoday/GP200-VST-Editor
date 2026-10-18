> Documento histórico. Para esta entrega usa README_FACTORY_NAM_ES.md.

# GP200 VST 0.1.9 — banco genérico de efectos

Añade la lectura de `GP200_EFFECT_BANK.json` y muestra nombres, parámetros y rangos de las asignaciones **WAH, PRE, DIST/DST y EQ** del firmware Unified M7OPT 1.3. Conserva NAM2 0.1.8, que el usuario confirmó funcionando.

## Usarlo

1. Compila e instala estas fuentes VST3/Standalone con el JUCE utilizado por el proyecto.
2. Copia `GP200_EFFECT_BANK.json` del ZIP que generó el **BIN instalado** a `Documentos/GP200/GP200_EFFECT_BANK.json` (la carpeta Documentos que use Windows, aunque esté redirigida).
3. Mantén allí `GP200_MOD_SYNC.json` de la **misma generación**, necesario para NAM2. Si el banco y ese manifiesto identifican versiones o hashes distintos, el banco no se aplica y aparece un aviso.
4. Abre el VST/Standalone. Si ya está abierto, detectará el archivo en aproximadamente un segundo. Los cuatro bloques mostrarán el algoritmo instalado y sus controles.
5. Si cambias de firmware, sustituye ambos JSON por los de esa generación. Un banco válido con `slots: []` restaura la presentación stock y evita reutilizar configuraciones PRE/WAH antiguas. Quitar el archivo vuelve a la compatibilidad anterior PRE/WAH.

**No hay que modificar ni volver a flashear el firmware para esta actualización del VST.** No incorpora algoritmos ni modifica sus límites DSP o RAM. Solo interpreta los metadatos de las asignaciones ya instaladas. La sincronización automática completa desde la pedalera sigue pendiente de una ampliación del protocolo.

## Rendimiento

- El callback de audio no cambia; se comprueba su SHA frente a 0.1.8.
- No se añaden peticiones MIDI periódicas, modelos DSP ni conversores de audio.
- Los nombres, rangos, opciones y relaciones de parámetros se preparan al importar y se consultan en memoria.
- Se elimina la comprobación de archivos de los getters anteriores de metadatos, incluidos los NAM. La interfaz comprueba cambios de archivos como máximo una vez por segundo; solo parsea un archivo cambiado.
- Los readbacks Sync repetidos no provocan reconstrucciones de controles de delay si su modalidad no cambia.
- Los guards de escritura SnapTone fuerzan una revisión antes de preparar o confirmar una transferencia, conservando la protección de NAM.

En la prueba local de 100.000 iteraciones, cuatro consultas por iteración produjeron **cero comprobaciones de archivos, lecturas o parseos JSON**. El benchmark adjunto también compara el coste de esas consultas con 0.1.8. Son medidas de código host con dobles de JUCE, no una medida de DSP de la pedalera ni de rendimiento de una DAW real. La carga inicial de un archivo y su posterior modificación tienen un coste puntual de validación en el hilo de control/interfaz; no se promete coste total cero.

## Presentación y parámetros

- La clave es **bloque de destino + ID físico**. Un ID compartido por PRE y DIST puede tener nombres y algoritmos diferentes sin confundirse.
- Se usan los índices, nombres, valores por defecto, pasos y rangos del JSON. Por ejemplo, Pure adaptado a 2.000 ms conserva ese máximo.
- Se conservan los IDs físicos al enviar cambios MIDI. El ID del algoritmo fuente se usa para la presentación, no para sustituir el destino en los mensajes.
- Las opciones discretas se asocian al parámetro fuente por nombre durante la importación. Esto evita heredar la opción de otro parámetro cuando Time y Feedback cambian de índice. Los booleanos sin etiqueta anterior muestran ON/OFF.
- Time/Sync también funciona para delays reubicados; Dual Echo identifica los controles y sincronías A/B por separado.
- El banco completo tiene prioridad sobre los metadatos PRE recibidos por el protocolo antiguo y sobre el antiguo archivo WAH. Los destinos omitidos en un banco válido se muestran stock.
- Micro Boost se añade a la lista DIST del VST para completar los 80 destinos que contempla el firmware.

Los fixtures de `tests/effect_bank_fixtures` sirven para pruebas y **no son los metadatos de tu BIN**.

## Validación y compilación

Consulta `docs/EFFECT_BANK_HANDOFF_ES.md` y `docs/effect_bank_validation/`.

```sh
python tests/run_tests.py
python tests/test_receive.py
python tools/validate_catalogs.py
```

Para Windows, con Visual Studio 2022, CMake >=3.22 y el checkout JUCE del proyecto:

```bat
cmake -S . -B build -A x64 -DJUCE_SOURCE_DIR="C:/path/to/JUCE"
cmake --build build --config Release --target GP200Plugin_VST3 GP200Plugin_Standalone --parallel
```

La entrega es de fuentes. No incluye EXE/VST3 compilados ni JUCE. Quedan pendientes la compilación/enlace Windows, la comprobación visual de la GUI y la prueba de esta versión en USB/DAW y pedalera.
