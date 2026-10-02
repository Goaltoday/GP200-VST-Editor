# GP200 VST FIX7 y firmware R15 SPR FIX4

Versión del 2 de octubre de 2026. Parten de VST FIX6 y generador de firmware FIX3.

## Ficheros

- `GP200_VST_R15_SPR_FIX7_source.zip`: proyecto completo de fuentes. **Debe recompilarse**; no contiene un VST3 nuevo precompilado.
- `GP200_R15_SPR_FIX4_V180_V182_HOT1_HOT2_TEST.html`: generador actualizado. Abrir, cargar el BIN compatible y seleccionar FX Loop R15 SPR.
- `GP200_R15_SPR_FIX4_TECH_KIT.zip`: código Thumb, regiones, resolver, scripts reproducibles, pruebas y base necesaria para reconstruir el generador.
- `CAMBIOS_Y_PRUEBAS.md`: relación con la auditoría, resultados y límites.

## Compilación del VST

Extraer el ZIP de fuentes. Usar JUCE en `external/JUCE`, en `JUCE`, o indicar `JUCE_SOURCE_DIR`. Ejecutar `build_release.cmd` con las herramientas de CMake/Visual Studio del proyecto. FIX7 corrige también la selección de JUCE y las rutas con espacios de ese script. El identificador del plugin y su nombre se conservan; la versión CMake es 0.1.7.

Cerrar el host antes de sustituir el VST3 compilado. La ruta normal de FIX7 funciona con firmware FIX3; FIX4 añade la corrección de estados inválidos. Para probar esta revisión completa, utilizar ambos ficheros nuevos.

## Correcciones

El envío SPR y los avances IR/HOT/SnapTone se ejecutan en el timer de la conexión MIDI, independientemente de la ventana. Cada etapa SPR comprueba slot y generación bajo el bloqueo de recepción. Store y el guardado/exportación conectados esperan datos vivos, routing conciliado y una respuesta de modo posterior a la lectura. Un timeout no libera ese bloqueo: inicia una recuperación de lectura.

Las lecturas incompletas se reinician tras 1,5 segundos; tres reintentos próximos usan una pausa de 300 ms y después se aplica una pausa de 5 segundos. La captura nueva descarta los paquetes anteriores. Los paquetes se validan por longitud total declarada, cobertura de offsets, terminador y nibbles. La petición de estado encolada se consume al liberar la lectura. Una edición rechazada vuelve a la ruta confirmada o a la operación ya aceptada.

Al cerrar el editor durante Recall/Import PRST, se cancela la restauración pendiente y se consulta el estado real: no se deja la conexión permanentemente en modo restore. **Recall no continúa con la ventana cerrada**; el buffer puede haber recibido algunos pasos, y se concilia antes de permitir Store.

El firmware acepta 00/01, 80–8B y 90–9B. Rechaza 8C–8F; al cargar un modo inválido lo normaliza a Series. El getter muestra Series para esos estados y mantiene la protección del retorno DSP. Se conserva P al cambiar Series/Parallel desde las rutas nativas/MIDI.

## Comprobación física

1. Alternar Series/Parallel y comprobar que GP-200 y botón coinciden.
2. Cambiar S/P/R, cerrar la ventana inmediatamente y volver a abrirla: esperar lectura confirmada.
3. Cambiar de preset durante el envío: la operación debe cancelarse y recuperarse sin más etapas sobre el slot observado nuevo.
4. Store durante envío/recuperación debe quedar bloqueado. Tras confirmación, guardar, cambiar de preset, recargar y reiniciar el pedal.
5. Verificar VOL blend 0/50/100 con VOL activo en Return o después, y repetir con la asignación EXP a volumen controlada. FIX7 no cambia las asignaciones de expresión.
6. Recall: cerrar a mitad, reabrir y comprobar la recuperación del estado parcial real.

Las pruebas automáticas no sustituyen la validación USB, flash o audio en la GP-200. La revisión nueva no se ha ejecutado físicamente ni se ha compilado como VST3 completo en este entorno. V180 usa un BIN real; V182 se verifica con fixture sintético. No se incorpora HOT DIRECT V3: el generador conserva la base R14 HOT1/HOT2 disponible, como FIX3. La exportación PRST y Recall/A-B no añaden aquí un formato nuevo para guardar/restaurar P y el modo ampliado.
