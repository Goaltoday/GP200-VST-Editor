# FIX10 VST + FIX8 firmware: PRE automático y edición de routing

Alcance: puntos 2, 4, 5 y 7 solicitados.

## Cambios

- SEND/SPLIT puede superar P hasta MIX/RETURN; MIX/RETURN puede retroceder por debajo de P hasta SEND/SPLIT. P acompaña a la flecha al soltarla. Se mantiene S ≤ P ≤ R. El cruce entre las dos flechas sigue prohibido.
- En paralelo las flechas se llaman SPLIT y MIX; en serie conservan SEND y RETURN.
- Se retiran los textos de diagnóstico SPR S/P/R y VOL LEVEL/BLEND del fondo. El botón indica SERIES o PARALLEL.
- VOL aparece como BLEND en paralelo cuando está activado y después de MIX. Tanto la cinta como el editor de parámetros muestran la función. El control muestra A y B en los extremos y una proporción intermedia; conserva los valores MIDI y la curva de mezcla R14. A/B describe la posición del control, no dos ganancias lineales: en el centro la mezcla actual mantiene ambas ramas a ganancia unidad.
- PRE se recupera automáticamente al conectar los puertos MIDI con el firmware FIX8. Se transfieren las 15 asignaciones, incluidos los huecos originales, y se actualizan nombres y perfiles del catálogo a través del ID del efecto fuente. La caché GP200_PRE_BANK.json se escribe automáticamente; si falla la escritura, la configuración recibida sigue activa en memoria.
- Al reconectar MIDI se repite la lectura; abrir y cerrar solamente la ventana del VST no reinicia la transferencia.

## Uso

1. Abre GP200_R15_SPR_FIX8_PRE_MIDI_SYNC_V180_V182_HOT1_HOT2_TEST.html y genera el firmware con tus asignaciones PRE y demás opciones.
2. Instala ese firmware siguiendo el procedimiento que ya utilizas. Conserva tu BIN anterior para volver a él durante las pruebas.
3. Compila estas fuentes con tu entorno JUCE habitual y build_release.cmd (o tu comando de compilación habitual). El ZIP contiene fuentes, no un VST3 precompilado.
4. Carga el VST nuevo y conecta los puertos MIDI de la GP200. Espera a MOD_SYNC OK / PRE synchronized. No hay que copiar manualmente GP200_PRE_BANK.json.
5. Comprueba los nombres PRE y abre sus controles. Para comparar, genera un firmware con un hueco PRE restaurado al original: al reconectar, el VST debe retirar su asignación antigua.
6. Prueba SPLIT avanzando por encima de P y MIX retrocediendo por debajo de P. En cada caso P debe acompañar a la flecha.
7. En paralelo coloca VOL después de MIX y actívalo: debe mostrarse BLEND. Comprueba A, centro y B; al poner VOL en otra posición o al pasar a serie, recupera su presentación de volumen.

Para PRE automático se necesita la pareja de firmware y VST nuevos. El VST nuevo acepta las 19 páginas CAB/AMP del firmware anterior, pero ese firmware no proporciona PRE. El plugin anterior no entiende la nueva capacidad PRE.

## Validación y límites

- Pruebas del generador con el BIN TNT V180 adjunto: aceptación, sustitución de carry-over por SPR, conservación de los demás mods y coexistencia con la extensión PRE.
- Metadatos PRE originales y asignaciones MOD, DLY, RVB y WAH; actualización desde el emisor MOD_SYNC anterior.
- Emulación de las instrucciones Thumb emitidas: 23 páginas, metadatos PRE exactos, nonce, checksums, pila conservada y página fuera de rango rechazada. Las funciones nativas de nombres CAB y envío MIDI se sustituyen por dobles de prueba.
- 74 comprobaciones del decodificador real usando respuestas del emisor ARM; registros alterados, nonce y checksum incorrectos rechazados.
- 6128 comprobaciones de drag/drop/cancelación, incluidas las flechas cruzando P; 71 Save/Recall/DAW; 302 conexión/lectura/guardado; 728 paquetes de modo; 512 respuestas de modo; 192192 movimientos del modelo de routing. JUCE/puertos/XML están simulados en estas pruebas.
- No se ha compilado el VST3 completo: este entorno no tiene el SDK JUCE del proyecto. Pendientes la compilación en tu equipo, la inspección visual en el DAW y la prueba USB/audio en la GP200.
- V180 probado con BIN real. V182 conserva la adaptación por perfiles y pasa la validación de JavaScript; no hay BIN V182 real ni pedal V182 para validar la instalación aquí. Se mantiene el bloqueo de sustitución de carry-over TNT en V182 hasta disponer de su restauración original verificada.

La política de sustitución de carry-over y conservación de otros mods TNT procede del FIX7 anterior. Esta versión amplía la lectura de metadatos PRE; no cambia el DSP de esos efectos ni la curva del blend.
