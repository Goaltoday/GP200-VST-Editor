> Documento histórico. Para esta entrega usa README_FACTORY_NAM_ES.md.

> Histórico: para VST 0.1.12 usa README_NAM_NAMES_ES.md.

**Actualización 0.1.11:** consulta `README_ACK_FIX_ES.md` para corregir la confirmación. El texto siguiente describe el alcance del paquete firmware 1.4 original; este ZIP de corrección contiene únicamente las fuentes del VST.

# GP200 NAM HOT persistente — versión experimental

Incluye el generador **HTML 1.4** para GP-200 V180/V182 y las **fuentes del VST 0.1.10**, basadas en 0.1.9. No incluye un VST3 Windows compilado ni un BIN configurado con tus modelos. El botón «Fuentes y pruebas» del HTML descarga las fuentes del firmware y sus pruebas.

## Verificación de tu última versión

El HTML adjunto de 37.213.703 bytes es idéntico al archivo `actual/GP200_Unified_M7OPT_1.3_180_182.html` del paquete de fuentes unificadas del 17/10/2026. SHA-256: `a1066a598a803553fcabccb177623ad279126fadadeabb4bff56666cf2dc2ccf`. Es la versión actual dentro de esas fuentes; no se afirma que sea la última publicada fuera de ellas.

Tu JSON es V182, con FNDR (`0x07000015`, destino Factory 11, originalmente Match CL) y REC (`0x07000048`, destino Factory 39, originalmente Match OD), ambos NAM2 Input/Output y neutro 50. No contiene asignaciones Factory CLO. El JSON no contiene los pesos de los modelos.

## Cómo usarlo

1. Abre `GP200_Unified_M7OPT_1.4_180_182.html`. Elige V182 y configura tus mods como antes. Carga los `.nam` originales y asigna los AMP que quieras dedicar a NAM. Para mantener FNDR/REC, usa los destinos Match CL/Match OD y esos nombres.
2. Genera el ZIP. El generador instala automáticamente el soporte Factory necesario y el receptor NAM HOT cuando hay modelos NAM. Usa el BIN y los JSON de esa misma generación. Los BIN ya ampliados con NAM siguen sin admitirse como entrada.
3. Compila el VST con el procedimiento Windows habitual, desde la carpeta `GP200-VST-Editor` incluida. Es una versión de fuentes; en este entorno no se ha producido un binario Windows.
4. En la pedalera selecciona un AMP diferente al destino y espera al preset vivo en el VST. Bypass no basta. En la ventana de modelos del VST abre la carpeta con tus `.nam`, elige un destino `[NAM HOT · .nam only]` e importa.
5. El VST consulta primero el soporte NH01 real del dispositivo. Solo entonces envía el modelo. El estado **«flash verified»** requiere el ACK del receptor después de leer y verificar la flash. Después selecciona el AMP de destino. El modelo guardado se restaura al cargarlo tras reiniciar.

Solo se sustituyen en caliente destinos **ya preparados como NAM por el HTML**. No convierte un AMP stock o CLO a NAM durante la transferencia. Se admite exactamente un WaveNet A2 Lite compatible, a 48 kHz y con 1.871 pesos; un contenedor debe tener exactamente un submodelo compatible. No convierte NAM a SnapTone. Se mantienen los nombres de destino y los dos controles Input/Output.

## Capacidad y persistencia

Se reutilizan exclusivamente los 18 almacenes físicos AMP18 ya delimitados para SnapTone: **uno para el receptor y uno por NAM**. Con dos NAM quedan **15** almacenes para Factory SnapTone; con cinco quedan 12. Sin NAM no se reserva ninguno. No se reduce el número de destinos AMP elegibles ni las diez posiciones SnapTone nativas.

El almacén físico 18 queda reservado para código. Si contiene un CLO, el generador rechaza la combinación; no borra ni reubica ese CLO silenciosamente. Cada NAM recibe otro almacén libre. El receptor impide que CL/CR escriban o renombren un destino NAM y el asignador SnapTone omite los almacenes ocupados.

Los pesos se guardan en dos páginas de flash por NAM, con CRC32, identificación del motor y marcador de confirmación escrito al final. Se verifica la lectura antes del ACK. Hay una sola copia persistente: si se interrumpe una sustitución, el siguiente arranque usa el NAM original del BIN si el registro es inválido; **no garantiza conservar la última sustitución HOT anterior**.

## Rendimiento y pruebas

No cambia el núcleo M7OPT, el procesamiento por lotes de ocho, SRC, filtros, FIFO, controles ni `gp_process`. El VST conserva el callback de audio, los serializadores de parámetros y efectos, y la caché del banco genérico de 0.1.9. No añade consultas MIDI periódicas. El trabajo adicional está en la transferencia y en la inicialización de un AMP NAM: verificar y copiar sus pesos guardados.

Eso no demuestra impacto cero durante una escritura flash. Falta medir en pedalera posibles pausas de audio, tiempos de carga y consumo durante la transferencia.

Pruebas completadas:

- Generación 0/1/3/5 NAM y DOM completo con cinco NAM en V180 y V182; bloqueo de controles y conservación de las zonas TNT protegidas después de instalar los almacenes NAM.
- Receptor Thumb compilado ejecutado con el cargador nativo en emulación ARM en ambos perfiles. Consulta de los 71 destinos, rechazo del AMP activo, CRC/motor/destino incorrectos, bloqueo CL/CR sobre NAM, escritura aislada, confirmación final, restauración persistente simulada y caída al modelo del BIN ante registro inválido.
- Validador de arranque SnapTone original ejecutado sobre los nuevos almacenes NAM y del receptor.
- 7.696 comprobaciones del importador y transporte NAM del VST, incluida comparación binaria independiente del orden de todos los pesos, respuestas antiguas, timeout sin escritura y timeout sin falsa confirmación.
- Regresiones NAM2 y banco genérico, recepción MIDI y catálogos. Las comprobaciones de SHA confirman que el audio del VST y sus serializadores siguen iguales.
- Sintaxis completa de `GP200NAM.cpp` y `MidiConnection.cpp` con cabeceras JUCE 8.0.6 reales. El editor presenta los mismos 15 errores previos de cabeceras ToneMatch con GCC que 0.1.9, sin errores nuevos; **no equivale a una compilación enlazada del plugin**.

**Pendiente de prueba física:** transferencia MIDI real de este tamaño, escritura/lectura flash, apagado y encendido, carga y SAVE/recarga de presets, escucha y estabilidad prolongada. La flash de las pruebas ARM está emulada. El paquete es experimental, no una versión físicamente validada.

Los BIN de pruebas usan modelos sintéticos y no se entregan como firmware para tu pedalera. El HTML genera tu firmware con tus propios modelos.
