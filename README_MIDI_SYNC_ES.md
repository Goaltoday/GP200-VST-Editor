# GP200 VST MIDI Sync 1 — 11/10/2026

Esta entrega añade ALIGN e inversión de polaridad B debajo de BLEND. El VST actualiza los parámetros y ON/OFF al recibir una Scene confirmada desde el firmware de esta entrega. El audio mantiene CTRL nativo; la espera de sincronización sólo afecta al editor.

## Instalación y compilación

1. Conserva tu checkout y `external/JUCE`. Copia los fuentes de este ZIP sobre tu proyecto sin borrar JUCE, tus imágenes ni archivos propios. Como alternativa, aplica `GP200_VST_MIDI_Sync.patch` sobre los fuentes originales adjuntos al proyecto.
2. Compila en CMD, una orden por línea:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

La dependencia usada para la comprobación de cabeceras es JUCE `3cc224c3e84d6acbabf62358ba1ccbf133cd2095`, la revisión restaurada en tu checkout. El ZIP no incluye JUCE ni un VST3 compilado. No seleccionar otra revisión sólo por su versión.

3. Abre `Custom_Firmware_0.1_Scenes_MIDI_Sync.html`, carga un BIN compatible con el generador y genera tu firmware conservando las opciones deseadas. CHAIN habilita el receptor de ALIGN/PHASE; Scenes habilita los avisos de cambio de Scene. No uses los BIN de pipeline del kit como selección predeterminada: son fixtures de pruebas con varios mods.
4. Instala el VST3 compilado y el firmware generado. Conecta y espera a que llegue el preset completo. En CHAIN aparecerán BLEND, ALIGN y PHASE B.

## Uso

ALIGN: −32 a +32 muestras estéreo; positivo retrasa A, negativo retrasa B. Doble clic pone ALIGN a cero. PHASE B alterna NORMAL/INVERTED. Editar uno conserva el otro y el VST exige lectura de confirmación antes de otra escritura de estos controles.

En Scenes, FS5–FS8 recuperan A–D. El editor sigue el sonido activo tras recibir el aviso y leer el estado completo. Editar desde el VST cambia ese sonido activo; para incorporarlo a una Scene se conserva el gesto de captura FS4+Scene y después STORE/STORE AS. No se añade captura automática ni un selector A–D en el VST.

Si cambias una Scene en el pedal, se cancelan gestos CHAIN pendientes y una restauración DAW en curso. Se descartan respuestas de otro slot y lecturas que estuvieran en curso. El VST espera estado fresco; no reenvía un valor rechazado.

## Tráfico y audio

El cambio de Scene sólo publica un aviso pendiente desde CTRL; no llama a USB, reloj ni serialización desde audio. Control agrupa cambios durante 200 ms y encola un aviso de 46 bytes; la bomba MIDI nativa lo entrega. Cola llena: reintento de control 200 ms después, sin bucle de espera. Desconexión: se descarta el aviso; la reconexión usa la lectura habitual.

El VST conserva una sola lectura de preset en vuelo y limita los refrescos ordinarios a uno cada 350 ms. ALIGN/PHASE y BLEND agrupan gestos y esperan confirmación. Los sliders ordinarios de efectos ya enviaban al soltar; volumen, PAN y tempo ahora siguen ese comportamiento durante arrastre. Se conservan el flujo de transfers, los guards, los ACK y las esperas existentes. No hay sondeo continuo nuevo de Scenes.

Estos límites reducen carga y separan control de audio. No equivalen a una medición de latencia ni garantizan el comportamiento físico de cualquier transferencia. ALIGN añade su retardo intencionado; es distinto de un retraso provocado por MIDI.

## Evidencia y pruebas pendientes

PRUEBA LOCAL: regresiones de transporte, readback, 130 combinaciones ALIGN/PHASE, parser y cancelación de Scene; Save/Recall/DAW y routing anteriores. Cabeceras JUCE reales: MidiConnection pasa sintaxis GCC; PluginEditor conserva los mismos errores previos de GCC en defaults de Options de ToneMatch. No se modificó esa funcionalidad. No es una compilación enlazada de Windows.

PENDIENTE: compilar con Visual Studio 2022, abrir en DAW, verificar ambos sentidos MIDI y UI en GP200, guardar/reiniciar, y medir cortes/latencia bajo cambios rápidos de Scene, controles, consultas y transfers. La convivencia con el editor oficial del nuevo aviso reservado también requiere prueba física.

Pruebas de fuentes: `python tests/run_tests.py`. Las pruebas usan puertos, codec y dibujo modelados; no prueban audio USB real.
