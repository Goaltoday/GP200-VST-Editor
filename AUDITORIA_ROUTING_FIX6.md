# Segunda revisión de FIX5: FIX6

## Fallo reproducido y corrección
`sendFlexibleRouteFromRibbon` cancelaba el envío pendiente antes de verificar conexión, recepción completa del preset, validez del borrador y éxito del primer mensaje de sustitución.

Por ejemplo: se envía Series como paso intermedio; comienza una lectura del pedal y los datos pasan temporalmente a no recibidos; se edita P otra vez. FIX5 cancelaba la primera operación y rechazaba la nueva. Podían perderse los pasos pendientes de orden y modo final.

FIX6 valida el nuevo borrador usando una instantánea coherente y conserva la operación anterior hasta que la sustitución empieza a enviarse. Una edición rechazada por carga, geometría inválida o fallo de envío no borra el orden/P/modo pendientes de la primera operación. Una sustitución válida conserva el comportamiento de reemplazo. Las ediciones rechazadas no se encolan: hay que repetirlas cuando termine la lectura; la confirmación posterior muestra la ruta recibida del pedal.

## Pruebas realizadas
- Prueba añadida con los métodos reales del editor extraídos y mocks MIDI/interfaz: falla contra FIX5 en el caso de lectura intermedia y pasa contra FIX6.
- Comprueba también que un borrador inválido y un error del primer envío no cancelan la operación anterior, que ésta llega al modo final previsto y que una sustitución válida adopta el nuevo P.
- Pasan las pruebas previas de frescura de respuestas, cambios de slot, confirmación, cancelación por otras operaciones y timeout.
- Pasan las pruebas de recepción extraída y las 512 tramas de protocolo cortas/completas.

## Alcance y pendientes
No se ha compilado el plugin completo con JUCE ni probado FIX6 en hardware. El firmware sigue siendo FIX3; este paquete no incluye firmware nuevo. No se afirma que la prueba con mocks reproduzca USB, audio o flash.

Se mantienen los límites documentados en AUDITORIA_ROUTING_FIX5.md: Recall/A-B y PRST no garantizan restaurar modo/P, los modos legacy no contienen P, y las respuestas nativas de modo carecen de identificador de slot/consulta. No hay nueva evidencia física sobre VOL/EXP. Esta corrección evita abortar una operación por una edición rechazada; no completa esos otros caminos.

## Uso
Compila los fuentes FIX6 y sustituye el plugin. Prueba alternar Series/Parallel y mover P durante las lecturas del pedal. Una edición rechazada muestra NOT SENT; espera la confirmación anterior y repite esa edición. Para comprobar persistencia usa los slots guardados en la GP-200 y consulta la limitación de Recall/A-B antes de usarlo.
