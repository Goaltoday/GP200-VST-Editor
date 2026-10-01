# GP-200 R15 SPR — candidato para pruebas físicas (2026-10-01)

## Archivos
- GP200_R15_SPR_V180_V182_HOT1_HOT2_TEST.html: generador dual autocontenido.
- GP200_VST_R15_SPR_TEST_source.zip: fuentes VST basados en tu último ZIP, con envío MIDI real.
- GP200_R15_SPR_TECH_KIT.zip: ensamblador, regiones, pruebas, parche VST y este documento.
No contiene un VST3 nuevo compilado ni un BIN universal: el HTML genera el BIN a partir del firmware que cargues.

## Preparar la primera prueba
1. Compila estos fuentes VST en tu entorno habitual, con el submódulo JUCE disponible.
2. Abre el HTML y carga el BIN R14 exacto que utilizas (1.8.0 o 1.8.2). Conserva ese BIN como referencia.
3. Activa FX Loop R15 SPR. Los mods reconocidos del BIN se conservan mediante el generador heredado; no reconfigures AMP18/PRE para esta primera prueba. Si aparece conflicto, no genera firmware.
4. Genera la salida e instala el BIN resultante por el procedimiento habitual.
5. Abre el VST SPR y un preset sencillo. El botón SPR: PARALLEL envía la ruta al pedal; no es solo un cambio visual.
6. Arrastra bloques de la cinta entre IN COMMON, A, B y OUT COMMON. Cada suelta envía Send, Return, orden y P.
7. Empieza con límites interiores y bloques fáciles de distinguir. Mueve AMP a A, B y un tramo común para comprobar que ya no fija la frontera. Alterna Serie/Paralelo y prueba Blend con VOL activo después de Return.
8. Después prueba ramas vacías, Send=0 y Return=11. Registrar SAVE/cambio/reinicio es una prueba posterior; no se ha confirmado persistencia física.

## Modelo y protocolo
Lista: entrada común [0,S), A [S,P), B [P,R), salida común [R,11).
Condición: 0 <= S <= P <= R <= 11. Cualquier bloque puede estar en cualquier tramo.
El VST calcula los límites al mover bloques. Los bypass no cambian la pertenencia.
P se codifica en el campo de modo FX Loop existente, evitando asignar un byte reservado desconocido:
- Paralelo: 0x80 | P.
- Serie: 0x90 | P.
- MIDI: mismo SysEx 12 10 capturado, índices 41 y 42 = nibble alto y bajo del valor.
- Send/Return/orden: SysEx 12 20 existente, Send en índices 41–42 y Return en 43–44.
Cada edición envía Serie legacy 01, luego 12 20, luego el modo ampliado. Esta secuencia aún requiere comprobarse en el dispositivo.
El firmware amplía la validación del setter nativo, conserva el modo ampliado al construir el estado DSP y aplica S/P/R sin buscar DIST ni AMP. El getter nativo devuelve 0 para los modos ampliados, evitando activar el loop externo al llegar a Return=11; por ello la pantalla del pedal no describe el modo ampliado (incluida Serie).
Los modos legacy 00/01 siguen utilizando las rutinas R14 existentes.
VOL después de Return conserva la regla R14: 0=A, 50=A+B, 100=B; en Serie actúa como volumen normal.

## Alcance de la prueba VST
Con conexión, envía MIDI real. Sin conexión, indica NOT SENT y mantiene una edición local.
SENT significa que el VST entregó los mensajes a la salida MIDI; no acredita confirmación del DSP.
El VST inicia un reparto de prueba S=2, P=5, R=8 sobre el orden recibido. No consulta ni reconstruye automáticamente P al abrir/cambiar de preset.
Los refrescos actualizan las características de los efectos, conservando el reparto de esta sesión.
Para estas pruebas, edita un solo preset sin alternar presets ni snapshots. Al cerrar el editor se pierde el reparto de la interfaz.
Save sigue enviando el commit nativo, pero la persistencia de P y los extremos 0/11 NO está validada. Export/Recall/Compare y sincronización de P quedan pendientes.
La primera prueba está limitada al routing en vivo. El editor oficial no representa P ni entiende la enumeración ampliada; no lo utilices para editar routing durante la prueba.
El arrastre de las tarjetas de parámetros se redirige a la cinta. Parámetros, selección y funciones no relacionadas con routing conservan el código de la base.

## Evidencia disponible
- Los dos perfiles R14 reconstruidos coinciden con sus SHA internos antes de añadir SPR.
- 3.714 casos de instrucciones Thumb: ambas versiones, todos los tríos S/P/R válidos con orden barajado, Blend, Serie, validación de límites y normalización del modo.
- Se interpreta el código ensamblado real mediante desensamblado LLVM y un intérprete limitado. Los callbacks de efectos DSP están simulados; no es emulación completa de la GP-200.
- 192.192 movimientos del modelo de edición comprobados; bloques únicos y límites válidos.
- 728 paquetes de modo comprobados y mensajes legacy comparados.
- Sintaxis JavaScript, instalación, upgrade R14, idempotencia, scope y guards comprobados en ambos perfiles.
- V180: comparación de regiones con el BIN stock adjunto real. V182: fixture de regiones y receptor sintético, NO un BIN stock completo; el generador comprueba las regiones del BIN real que cargues, incluida la cave libre.
- Desde R14, cambian los hooks de routing, el setter/getter del modo y la cave SPR; el checksum FRMW lo recalcula el generador heredado.
- NO se ha compilado el plugin completo aquí; falta JUCE/CMake. NO hay prueba de recepción MIDI ampliada, boot, audio, SAVE o reinicio en hardware.

## Reproducción
Los archivos .s son las fuentes ARM. make_spr.py usa arm_llvm.py y la biblioteca LLVM-20 para ensamblar; build_spr_html.py integra las tablas en el HTML base. test_spr_arm.py interpreta las instrucciones emitidas. test_html.js ejecuta guards y comprueba scope del instalador.
Los profile_*_audit.html son perfiles materializados para auditoría, no deben sustituir al generador dual de entrega.

## Qué observar si no cambia el sonido
Si el modo ampliado se pierde en la recepción MIDI, se oirá Serie aunque el VST indique SENT. Capturar la respuesta del pedal al 12 10 permite comprobar el valor enviado; para P=5, modo paralelo debe corresponder a 0x85 (nibbles 08 05).
No confundir esta observación con la validación de las instrucciones DSP, que supone que el modo llegó a la configuración.

La dirección de retorno del setter se resuelve desde el BL nativo del final del bucle DSP. El generador exige un receptor único, literal RAM esperado, getter/setter reconocidos y puerta de Return reconocida; aborta si el BIN real no coincide. La constante del setter en el .bin de cave es una plantilla y se relocaliza al generar. Estos .bin pequeños NO son firmware flasheable.
