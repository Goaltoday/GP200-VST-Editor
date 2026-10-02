# GP200 VST FIX9 — previsualización al arrastrar bloques

Parte de FIX8, que conserva el Save/Recall corregido y confirmado por el usuario. Esta entrega cambia únicamente el componente gráfico de routing en PluginEditor.cpp y PluginEditor.h. No hay cambios de firmware, protocolo MIDI, procesador, persistencia ni DSP.

## Uso

- Arrastra un bloque: una copia semitransparente sigue al cursor conservando el punto de agarre. El original se atenúa cuando su zona no está ocupada por los bloques de la previsualización.
- Los otros bloques se distribuyen para reservar un hueco real en la posición de destino. La marca amarilla está dentro de ese hueco, antes del primero, entre bloques o después del último. No atraviesa los bloques de destino.
- En paralelo, los límites de las ramas se mantienen fijos durante el arrastre. Los bloques se redistribuyen dentro de cada rama. No se añade resaltado de rama ni texto de destino.
- Soltar fuera del rectángulo del routing cancela el movimiento. Puedes salir y volver a entrar antes de soltar. Escape cancela también; el componente toma foco de teclado al comenzar el gesto. La pérdida de foco cancela.
- Soltar en la posición original no envía una operación de routing. Un clic sin arrastre mantiene la selección de parámetros.
- La previsualización es local y no modifica el orden real. Al soltar se aplica la misma operación usada para previsualizar. Se recalcula el destino con la posición de liberación, incluso si no llega un último evento de movimiento.

Las actualizaciones periódicas que mantienen el orden y los marcadores no cancelan el gesto. Un cambio real recibido en el orden o los marcadores sí lo cancela para evitar aplicar un arrastre sobre otra distribución. Los marcadores Send/Return también permiten cancelar al soltar fuera o con Escape y no envían si mantienen su posición.

## Compilación y prueba

Este paquete contiene fuentes completas, no un VST3 precompilado. Utiliza JUCE y `build_release.cmd` con CMake/Visual Studio según el proyecto. Se conservan el identificador, nombre y versión CMake existentes. Cierra el DAW antes de sustituir el VST3 compilado. Sigue utilizando tu firmware actual.

Prueba mover un bloque al inicio/final y entre dos bloques, dentro de una rama y entre ramas. Comprueba que lo que se ve antes de soltar coincide con el resultado. Comprueba Escape, soltar fuera, salir y volver a entrar, mantener posición y ramas vacías. Verifica también Save/Recall A/B en tu DAW.

## Pruebas y límites

`python3 tests/run_tests.py` ejecuta las pruebas con Python 3 y g++ C++20.

- 6124 comprobaciones del componente real de routing: previsualización sin mutación permanente, geometría de la marca sin cruzar los bloques de destino, aplicación al soltar, orden válido, inicio/final, ramas vacías, S/P/R coincidentes, cancelación, posición original, selección y actualización del dispositivo.
- 71 Save/Recall y persistencia A/B, 302 conexión/lectura/guardado, 728 paquetes de modo, 512 readback y 192192 movimientos de routing siguen pasando.

Las pruebas compilan los métodos reales con eventos, geometría y dibujo JUCE simulados; las pruebas MIDI usan transporte/codec/XML simulados. No se ha compilado el VST3 completo ni probado el gesto en un host real en este entorno. Queda pendiente esa validación visual y USB en tu equipo.

El parche `FIX8_to_FIX9_DRAG_PREVIEW.patch` contiene únicamente los dos archivos de producción modificados. Los documentos FIX7/FIX8 incluidos son históricos; para el arrastre prevalece este documento.
