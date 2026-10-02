# FIX17 firmware / FIX12 VST — fondo negro y sincronización SEND/RETURN

Cambios limitados a los dos problemas reportados:

1. Firmware V1.8.0: el control EDIT 2300 de la flecha naranja configura sus tres colores de fondo a negro con EDIT_SetBkColor (0x8000068a). Conserva bitmap, forma, naranja, posición y control de movimiento de FIX16. No cambia audio, DSP, MIDI, presets, PRE ni AMP18.
2. VST, basado en FIX11: reconoce el aviso nativo de routing que envía la GP200 cuando se mueven SEND o RETURN en el pedal. Es un payload de 20 bytes, 54 bytes MIDI, con ruta [8,0,16,0], slot, SEND, RETURN y orden de 11 módulos. Antes no se reconocía porque el campo de longitud 20 no coincidía con los casos existentes de 8/16/24 bytes.

El receptor valida tamaño, cabecera, offset, nibbles, slot, límites y unicidad del orden. Ignora el último byte reservado, que el setter nativo no inicializa. Solicita el preset activo mediante la lectura existente de siete fragmentos, con debounce de 120 ms y serialización de lecturas. La interfaz recibe las posiciones confirmadas mediante su revisión de preset habitual. Los avisos durante una lectura dejan pendiente otra para recoger el último movimiento; avisos de otro slot, restauración DAW y transacciones de routing activas no interfieren.

El ZIP VST contiene únicamente tres fuentes que deben sustituirse en el proyecto FIX11:
- source/libgp200/MidiConnection.cpp
- source/libgp200/MidiConnection.h
- source/libgp200/GP200FlexibleRouting.h
Después hay que recompilar el plugin con el procedimiento habitual. No contiene un DLL/VST3 compilado.

Firmware: GP200_BLACK_ARROW_FIX17_6bae304a.bin conserva los mods del BIN adjunto del usuario. HTML: GP200_R15_SPR_FIX14_BLACK_ARROW_V180_PRE_SYNC_TEST.html permite generar el mod, también desde las P UI previas reconocidas, incluida FIX16. V1.8.2 conserva su perfil previo sin esta P UI.

Validación: 3276 casos Thumb del mando, 202 cambios de página/nivel, geometría y ciclo de vida del marcador; además ejecución de la rutina nativa EDIT_SetBkColor y comprobación de sus tres campos. Hooks del BIN, bitmap nativo, checksum e instalación/actualización del HTML verificados. VST: 59904 avisos nativos válidos, mensajes malformados rechazados, 381 casos de conexión/lectura (78 nuevos límites S/R y protección durante lectura/recall), 71 Save/Recall, 6128 drag, 728 mode, 512 mode readback, 192192 routing moves y 74 PRE. Se compilan los métodos reales con puertos/codec/JUCE simulados. No se ha compilado el VST completo ni comprobado esta versión en hardware.

Prueba física: comprobar fondo negro al mover P en ambas filas. Con el VST recompilado y conectado, mover SEND y RETURN en la GP200; las flechas del plugin deben actualizarse después de la lectura MIDI, sin importar JSON. Repetir movimientos rápidos y confirmar la posición final. Save/Recall conserva su comportamiento previo.
