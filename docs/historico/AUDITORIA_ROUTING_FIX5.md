# Auditoría completa del routing — FIX5

## Alcance y resultado
Se revisaron FIX4 VST y FIX3 firmware: mensaje de escritura/consulta, serializador nativo, receptor MIDI, caché, cambios de slot, confirmación, Store, Recall/A-B, PRST, límites S/P/R y hooks DSP. Esta revisión entrega solo fuentes VST. Conserva el firmware FIX3.

## Errores corregidos
| Hallazgo comprobado en código | Efecto | Corrección FIX5 |
|---|---|---|
| Se aceptaba un modo de caché igual al solicitado sin exigir una respuesta posterior al último envío. | Podía anunciar recepción sin una respuesta nueva. | Se guarda la revisión antes del envío final y se exige una revisión recibida posterior. |
| `presetRevision` cambia tanto por lecturas recibidas como por ediciones locales optimistas. | Una copia editada en VST podía contar como lectura fresca del pedal. | Contador separado `livePresetRevision`, incrementado únicamente al completar los siete chunks del slot activo. |
| Las respuestas de parámetro no incluyen slot; el parser les asignaba el slot actual incluso durante su carga. | Una respuesta en cola podía atribuirse al nuevo preset. | Descarta respuestas de modo mientras no haya un preset completo recibido. La consulta posterior recupera el modo. |
| Se leían modo, slot, datos y contadores mediante llamadas independientes. | Un callback MIDI podía cambiar la caché entre lecturas. | Se obtiene una instantánea de esos campos bajo un único bloqueo. |
| Recall marcaba como recibido el snapshot local adoptado. | Podía sustituir la lectura física por datos locales. | La adopción se mantiene provisional hasta recibir el preset real. |
| El envío SPR podía comenzar o continuar durante Recall o una carga IR/HOT. | Podían cruzarse transacciones. | Bloquea el comienzo y cancela los pasos restantes si otra operación empieza. Un comando ya enviado no se deshace automáticamente. |
| El editor enviaba Series antes de comprobar la validez de todo el borrador S/P/R. | Una ruta inválida podía dejar aplicado el Series intermedio. | Valida orden y `0 <= S <= P <= R <= 11` antes del primer mensaje. |
| PRST y la reconstrucción del dump seguían limitando Send/Return a 1–10. | Un 0/11 podía convertirse en 4 o recortarse al exportar/importar. | Admite y conserva 0–11 en esos campos y en el antiguo editor de nodos. |
| El cálculo de P legacy llamaba a `jlimit` antes de comprobar Send <= Return. | Una geometría inválida recibida podía disparar una aserción de JUCE. | Comprueba Send/Return antes de calcular la frontera. |

## Aspectos revisados que se conservan
- La respuesta nativa corta contiene 8 bytes decodificados: 30 bytes MIDI, longitud 08 en la cabecera. FIX4 ya corrigió ese formato; FIX5 conserva el parser.
- El payload nativo del modo es `06 00 04 00 05 00 MODE 00`. Se validan fabricante, comando, longitud, offset, ruta y rango del modo.
- La consulta usa comando 11; el receptor del firmware distingue lectura 11 y escritura 12. La respuesta usa comando 12 y longitud real.
- FIX3 muestra 0/1 en la GP y conserva P al alternar desde su interfaz. La llamada DSP del retorno final conserva su tratamiento especial. No se han modificado estas rutinas.
- Copy/swap/blend, tamaños de búfer, llamadas de efecto, Series ampliado y tratamiento de VOL mantienen los bytes del firmware previamente entregado.
- No se modifica EXP, el algoritmo VOL ni las cargas de modelos.

## Límites aún pendientes, sin presentarlos como resueltos
1. **Recall/A-B y exportación/importación PRST no garantizan restaurar el modo SPR y P.** La lista de restauración del VST incluye parámetros, efectos y orden/Send/Return, pero no un paso de modo. El modelo de preset tampoco expone modo/P. Guardar en la GP y volver a cargar sus slots es una ruta distinta. No se ha inventado un offset PRST para esos campos. No uses A/B/Recall como prueba de persistencia completa SPR.
2. **Presets legacy 00/01:** no incluyen P. El VST todavía calcula una frontera de presentación a partir de su estado local; no puede considerarse una recuperación de P guardado. La ruta R14 depende de DIST/AMP; cambia a un modo ampliado desde el VST para trabajar con S/P/R libre.
3. **Identidad de respuestas:** el protocolo nativo del parámetro no lleva slot ni identificador de consulta. El descarte durante la carga reduce el cruce más evidente, pero no permite demostrar la identidad de cualquier respuesta retrasada que llegase tras terminar la carga. La consulta periódica reconcilia el estado; no se afirma correlación perfecta.
4. **VOL/EXP:** no se ha reproducido un fallo independiente de guardado sin EXP. Sigue siendo necesaria la prueba física sin destinos EXP asignados a VOL, separando valor manual y posición del pedal.
5. **Hardware/build:** no se ha compilado el plugin completo aquí (JUCE no está incluido), ni probado FIX5 en el pedal. El firmware V182 previo se comprobó con fixture sintética de recepción, no con un BIN completo V182 real.

## Verificación de FIX5
- Parser: 512 tramas nativas cortas/completas, rechazo de longitud, ruta y offset incorrectos, y rechazo de consulta saliente.
- Métodos reales del editor extraídos y compilados con mocks de interfaz/MIDI: caché coincidente no confirma, edición local no cuenta como recepción, lectura nueva sí confirma, cambio de slot, cancelación, rechazo de datos solo cacheados, rechazo de borrador inválido antes de Series, errores y timeout.
- Rama real de recepción extraída: descarta durante carga, adopta al recibir datos completos, aumenta la revisión con respuestas repetidas sin provocar otra lectura completa innecesaria.
- Límites del códec: cambios de rango revisados en decode/encode PRST y construcción del dump. No se ejecutó el códec completo con JUCE.
Estas pruebas no emulan USB ni audio de hardware ni guardado en flash. No justifican afirmar que todos los escenarios estén validados físicamente.

## Uso
Compila FIX5, reemplaza el VST y conserva el firmware FIX3. Prueba dos slots guardados en la GP, uno Series y otro Parallel, con P distinto; después una edición de routing y Store tras la confirmación. La corrección de lectura de FIX4 se mantiene. Para esta prueba usa guardado/carga de la GP, no Recall/A-B.
