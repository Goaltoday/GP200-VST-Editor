> Documento histórico. Para esta entrega usa README_FACTORY_NAM_ES.md.

# Controles de Scenes desde el VST — 11/10/2026

Este paquete es una actualización sobre `GP200_Fuentes_Consolidadas_CAPTURE_LED_VST_Standalone_2026-10-11.zip`; no es un sustituto completo de sus fuentes.

## Aplicar los archivos

1. Descomprimir sobre la carpeta raíz del kit consolidado, sustituyendo las rutas incluidas. El paquete conserva rutas `sources/VST`, `integration_scenes` y documentos raíz.
2. Para GitHub, copiar el contenido actualizado de `sources/VST` sobre la raíz del repositorio del editor. Conservar el submódulo JUCE ya restaurado. El workflow compila VST3 y Standalone desde el mismo código.
3. Abrir `Custom_Firmware_0.1_Scenes_Remote.html` y generar firmware desde el BIN oficial GP200 1.8.0 o 1.8.2, con Scenes habilitado y los mods habituales seleccionados. Cargar mediante el procedimiento habitual del proyecto. Scenes Remote funciona también sin CHAIN.
4. Abrir el nuevo VST3/EXE y conectar la GP200. El botón SCENE se habilita cuando llega el anuncio de compatibilidad del firmware. Con firmware anterior permanece deshabilitado y no envía sondeos.

## Uso

- SCENE encendido activa modo Scene; apagado vuelve a modo Patch. El estado cambia al recibir confirmación del equipo.
- A, B, C, D y Capture Scene permanecen ocultos en modo Patch. En modo Scene aparecen; la activa se resalta.
- A–D activan la Scene correspondiente. Capture Scene captura el sonido actual en la activa, con el mismo commit RAM y feedback LED que FS4 + FS5–FS8.
- Después de capturar, pulsar Store to GP200 para conservar las Scenes al apagar. Capture Scene por sí solo no escribe la memoria persistente.
- Durante una orden y la actualización del preset, los controles esperan. Si no llega confirmación, no se repite automáticamente la escritura.

## Documentos que debes sustituir en las fuentes

`INICIO.md`, `ESTADO_ACTUAL.md`, `MAPA.md`, `DECISIONES.md`, `Scenes_CAPTURE_LED_Technical_Handoff_EN.md`, `CHAIN_MIDI_Sync_Appendix_J_EN.md`, `VST_Standalone_Build_Handoff_EN.md` y `sources/VST/README_BUILD_WINDOWS_ES.md`. Añadir `Scenes_Remote_Control_Technical_Handoff_EN.md`, este LEEME, el HTML nuevo y los archivos de integración entregados. El manifiesto de esta entrega enumera cada archivo con su hash; los manifiestos históricos describen sus respectivas revisiones.

## Validación y pendientes

Pruebas locales: compilación Thumb V180/V182; receptor MIDI real emulado; selección y captura A–D; avisos pasivos y reconexión; rechazo de slot/opcode/marca incorrectos; coexistencia BLEND/ALIGN/PHASE; pipeline HTML y checksum; regresiones LED/STORE; métodos reales del VST con puertos/codec modelados. Sintaxis MIDI con JUCE fijado pasa; el editor mantiene los errores GCC previos de ToneMatch, sin nuevos errores en esta revisión.

Pendiente: compilación Windows en GitHub; abrir VST3 en DAW y EXE; comprobar posición/ocultación de botones; modo cambiado desde ambos lados; captura en A–D y STORE/reinicio; reconexión/cambio de preset; y audio continuo durante operaciones MIDI, incluyendo carga DSP alta. No se ha medido latencia ni certificado USB/audio físico. PRST y Save to DAW siguen sin incluir el banco completo de Scenes.
