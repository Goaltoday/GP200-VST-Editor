> Histórico: para VST 0.1.12 usa README_NAM_NAMES_ES.md.

# VST 0.1.11 — corrección de confirmación NAM HOT

Corrige el bloqueo «waiting for firmware capability response» seguido de «firmware did not confirm NH01; model write was not sent» de 0.1.10.

## Instalar esta corrección

Recompila el VST desde esta carpeta completa y sustituye la versión 0.1.10, con el host cerrado. Usa una compilación limpia, por ejemplo el procedimiento habitual de `build_release_hot1_clean.cmd`. El proyecto CMake declara versión 0.1.11. Se entregan fuentes, sin VST3 Windows compilado.

**Si ya instalaste un BIN generado por el HTML 1.4 con tus destinos NAM, no necesitas regenerar ni volver a instalar el firmware para esta corrección.** Conserva el JSON de ese BIN. Si la pedalera aún lleva el firmware 1.3, necesitarás el BIN 1.4 preparado con NAM HOT del paquete anterior.

Selecciona en la pedalera un AMP diferente al destino, espera al preset vivo en el VST, abre la carpeta con el `.nam`, selecciona el destino `[NAM HOT · .nam only]` e importa. El VST debe pasar de la consulta de compatibilidad al envío de los 42 bloques de pesos y finalmente a «flash verified». Ese mensaje sigue requiriendo la respuesta del dispositivo.

## Causa y corrección

El firmware nativo reconstruye el mensaje HQ/HN y ejecuta el receptor en cuanto llega el último paquete de datos. La confirmación puede regresar antes de que `sendMessageNow` termine. La versión 0.1.10 esperaba aceptar respuestas únicamente después de otro mensaje denominado commit; por ello descartaba la confirmación real de la consulta y terminaba por timeout.

0.1.11 activa la espera del ACK **antes de enviar el último paquete**. Elimina el mensaje separado de commit en NAM y comprueba el destino vivo y sus metadatos justo antes del paquete que dispara la operación. Copia el mensaje final antes de enviarlo para que una respuesta inmediata pueda cambiar o limpiar el estado con seguridad.

Los errores explícitos NH01 de destino no preparado o destino seleccionado se muestran como tales, aunque la respuesta de error no contenga un ID de AMP. Los fallos NAM ya no aparecen en verde al terminar la transferencia.

Esta corrección afecta al transporte NAM del VST. No cambia el firmware 1.4, la distribución de almacenes, el motor de audio, NAM2, el banco genérico ni los transportes anteriores de SnapTone/CAB/IR.

## Validación

- Reproducción de la respuesta anticipada con el parser, reconstrucción de paquetes, despacho y codificador de salida **nativos** de V180 y V182, ejecutados en ARM emulado.
- Métodos reales del VST conectados por MIDI simulado a ese firmware: consulta HQ en el paquete 2; escritura HN de 7.512 bytes en el paquete 45; ambas confirmaciones aceptadas durante el envío final. Ningún commit adicional; ninguna escritura de flash durante la consulta.
- 7.759 comprobaciones de importación y transporte NAM, incluyendo confirmación síncrona, cancelación antes del paquete final, respuestas atrasadas y errores de destino. Se conservan las regresiones NAM2 y del banco genérico.
- Sintaxis de GP200NAM y MidiConnection comprobada con cabeceras JUCE 8.0.6 reales.

Las pruebas simulan los extremos USB, la memoria de asignación de mensajes y la flash física. No equivalen a validación de esta corrección en una pedalera real ni a una compilación Windows enlazada. Los informes anteriores de 0.1.10 se conservan como antecedentes; el informe nuevo es `docs/NAM_HOT_ACK_FIX_VALIDACION.json`.
