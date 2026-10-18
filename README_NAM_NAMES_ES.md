> Documento histórico. Para esta entrega usa README_FACTORY_NAM_ES.md.

# GP200 NAM HOT 1.5 + VST 0.1.12 — nombres persistentes

Esta entrega corrige el nombre de los modelos enviados a destinos NAM ya preparados. Mantiene la recuperación de pesos de 1.4.1, que el usuario confirmó en pedalera. La función nueva de nombres está comprobada en emulación y pendiente de confirmación física.

## Uso

1. Abre GP200_Unified_M7OPT_1.5_180_182.html, elige V180/V182 y genera tu BIN desde la misma base compatible anterior a NAM, con tus asignaciones y mods.
2. Instala ese BIN. Recompila y utiliza las fuentes completas del VST 0.1.12 incluidas en esta entrega. No se incluye un VST3 Windows compilado.
3. En el importador, elige un destino NAM y el archivo .nam. El campo de nombre queda editable: hasta 12 caracteres ASCII, igual que los nombres NAM del generador. El nombre del archivo es el valor inicial; puedes cambiarlo antes de importar.
4. Selecciona otro AMP en la pedalera, importa y espera la confirmación de modelo y nombre guardados. Vuelve al destino; comprueba su nombre y sonido.
5. Apaga y enciende. El nombre se restaura durante el arranque desde flash y los pesos se recuperan mediante la lectura explícita de 1.4.1. No vuelvas a importar para comprobar la persistencia.

Cambiar el nombre de un NAM sin volver a enviar el modelo no se ha añadido al botón Rename. Para NAM, el campo de nombre acompaña a la importación.

## Compatibilidad y protocolo

NH02 mantiene la fragmentación y los 45 mensajes del intercambio anterior: 2 prepare, 1 consulta y 42 fragmentos de modelo. Añade 16 bytes de nombre al payload, sin añadir fragmentos. El ACK conserva su longitud y usa una firma NH02 que confirma soporte de nombres. Se conserva el tratamiento del ACK inmediato corregido en 0.1.11.

El VST 0.1.12 pregunta por NH02 antes de enviar los pesos. Con firmware NH01 anterior informa de que hay que actualizar y no envía el modelo. El firmware 1.5 sigue aceptando los envíos NH01 del VST 0.1.11, conservando el último nombre válido cuando lo haya; ese VST antiguo no puede enviar un nombre nuevo.

## Implementación

El nombre ocupa 16 bytes en +0x1D60 del almacén NAM, seguido de firma y CRC propios. No se solapa con los pesos ni con los metadatos previos. Se verifica junto a la escritura antes del marcador final/ACK. La cabecera nativa registrada enlaza a la cadena mediante el mismo campo relativo +0x38 que utiliza Factory SnapTone.

El arranque ejecuta el inicializador nativo previo y restaura los nombres de los NAM preparados desde flash antes de continuar la restauración existente. Se preservan el kernel NAM, Input/Output y el audio por bloque. El VST actualiza su caché NAM2 después del ACK correcto; conserva su tipo y controles, sin convertir el destino en CLO.

Se añade trabajo al arranque y durante la selección/inicialización de modelo, con lecturas de flash. No se añade trabajo por muestra ni acceso a archivos a los getters de metadatos. El coste real de arranque/selección y la presentación de la pantalla requieren comprobación en pedalera.

## Verificación

- V180 y V182: receptor compilado, init/proceso nativos, nombres tras envío y al arrancar con CPU/RAM nueva; se ejecuta también el inicializador nativo previo del arranque.
- Nombre corrupto: vuelve al nombre embebido. Nombres/longitudes inválidos se rechazan antes de escribir.
- Persistencia de pesos, error de lectura, registro incompleto, destino activo y aislamiento de almacenes conservados.
- C++ real y parser MIDI/ACK nativos: consulta + envío NH02, nombre escrito en flash, ACK inmediato y caché actualizada.
- Regresiones del VST completas: Scenes, CHAIN, guardado/restauración, NAM2, banco de efectos y getters sin I/O.
- GP200NAM.cpp, MidiConnection.cpp y GP200ModSync.cpp pasan comprobación de sintaxis con JUCE 8.0.6. El editor mantiene los 15 errores GCC previos de ToneMatch, sin errores adicionales; no se ha enlazado un VST Windows aquí.

## Ampliación a AMP stock

Lee AMPLIACION_FACTORY_NAM_ES.md: se incluye el prototipo que demuestra motor compartido en los 71 destinos de ambos perfiles. La conversión persistente por MIDI a AMP stock sigue pendiente de integración; esta entrega no la activa.
