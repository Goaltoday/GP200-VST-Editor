# GP200 1.7 NH04 · NAM en el espacio propio del AMP

Rama experimental basada exactamente en HTML 1.5 y VST 0.1.12 adjuntos. No continúa la conversión dinámica NH03/1.6 que produjo un cuelgue en la pedalera. El origen concreto de ese cuelgue no se ha reproducido físicamente; esta implementación elimina sus descriptores y asignaciones NAM en almacenes AMP18.

## Uso

1. Abre `GP200_Unified_M7OPT_1.7_180_182.html` y elige GP200 1.8.0 o 1.8.2. No sirve para GP200LT.
2. Parte de la base oficial incluida o de un BIN anterior a NAM que el configurador reconozca. No importes un BIN 1.6. Conserva una copia de tus presets y de tu BIN anterior.
3. Puedes generar con **cero NAM precargados**. El generador instala un único motor común; los AMP originales siguen funcionando como stock hasta que los sustituyas. Opcionalmente incorpora hasta cinco modelos iniciales en los destinos habilitados.
4. Genera el ZIP e instala su BIN con tu procedimiento habitual. Importa el `GP200_MOD_SYNC.json` de ese mismo ZIP en el VST nuevo.
5. Compila el proyecto VST 0.1.14 con el procedimiento Windows incluido (`build_release_hot1_clean.cmd`). Se entregan fuentes, no un VST3 Windows compilado.
6. En Models / Sound Clone elige un `.nam` y un Factory AMP habilitado para NAM HOT. Selecciona antes **otro AMP en la GP200** y espera la lectura del preset vivo. Bypass no basta.
7. Introduce un nombre de 1–12 caracteres ASCII, carga y espera `flash verified`. Después selecciona el destino. Nombre y pesos se guardan juntos.
8. Comprueba controles, cambio de preset, SAVE y reinicio en la pedalera. La validación automatizada no sustituye esta prueba física.

Los cambios son globales para todos los presets que usen ese ID AMP. Para recuperar el AMP stock hay que reinstalar un BIN que lo contenga. El original sobrescrito no se conserva como copia permanente.

## Capacidad y exclusiones

62 destinos simultáneos sobre base stock. Si un AMP ya contiene un Factory SnapTone/CLO, queda protegido y no se ofrece para NAM. El nuevo paquete ocupa **7.696 bytes**: cabecera nativa 184, pesos 7.484 y metadatos/CRC 28. Nombre y controles están en la cabecera.

| AMP excluido | Paquete original (bytes) |
|---|---:|
| Tweedy | 5.708 |
| SUPDual CL | 5.784 |
| SUPDual OD | 7.312 |
| J-120 CL | 5.264 |
| Z38 CL | 7.528 |
| Juice R100 | 6.560 |
| Foxy Bass | 4.608 |
| Mini Bass | 4.492 |
| Bass Pre | 5.824 |

El menor destino admitido es Bad-KT CL, con 7.696 bytes exactos. No se aumenta su región ni se invade el siguiente paquete.

## Almacenamiento nuevo

Cada modelo, nombre y perfil NAM2 reside en el espacio original de su AMP en la partición de efectos. La tabla AMP y las direcciones de los descriptores registrados permanecen intactas. Se modifica la cabecera de ese paquete para cargar el cuerpo del motor NAM común, añadido una sola vez. No se copia un motor completo por modelo.

El motor añade 35.740 bytes a la partición; conserva código, coeficientes, SRC/FIFO y ocupación de ejecución de la 1.5. Su modelo de reserva es silencioso. En init se validan y copian los pesos del paquete elegido desde la copia RAM nativa de la partición. No se añaden operaciones al procesamiento de cada bloque de audio.

Los paquetes stock no están alineados con sectores NOR de 4 KB. Una actualización necesita preservar dos o tres sectores completos, incluidos los bytes vecinos. Para ello se reservan **tres zonas globales de 8 KB** en los huecos PCM ya conocidos: almacén 18 para código/configuración; 16 y 17 para un diario temporal de recuperación de 16 KB y su RAM de trabajo. No almacenan asignaciones NAM permanentes. Factory SnapTone conserva hasta 15 almacenes; los diez SnapTone estándar no cambian.

Antes de borrar el destino se guarda y verifica la copia anterior y se confirma el diario. Solo se confirma éxito después de verificar todos los sectores y cerrar el diario. Un reinicio con diario pendiente intenta restaurar los sectores anteriores, antes de la selección del AMP, usando el punto de arranque de la 1.5. Durante una recuperación en caliente no se reescriben las cabeceras RAM de AMP vecinos activos.

## Protocolo y protecciones

NH04 es distinto de NH01/NH02/NH03. Consulta previa sin escritura; no se envían pesos a firmware anterior. Se conservan la fragmentación de 183 bytes, el nonce, la confirmación armada antes del último paquete y los guards del preset vivo. La transferencia sigue siendo 45 mensajes, sin commit adicional. La espera de confirmación es de 30 segundos.

Se comprueban ID público, destino local, cabecera reconocida, tamaño, propiedad CLO, motor, CRC, pesos finitos, escala positiva y nombre. BogSV OD usa su ID público del campo 0x34 para la protección del AMP activo, evitando la anomalía de su campo 0x14.

Estado 6: escritura/verificación fallida; si se había modificado el destino se intenta restaurar la copia anterior. Estado 11: lectura del diario fallida, diario pendiente o recuperación no completada; reiniciar y comprobar antes de repetir. No se reintenta automáticamente una transferencia sin ACK.

No se ha implementado descubrimiento automático de NAM mediante MS11. El VST conserva localmente los nombres y perfiles confirmados; no vuelvas a importar el JSON inicial en cada reinicio, porque sustituye esa caché. Para otro ordenador hay que trasladar la caché actualizada o gestionar de nuevo los modelos confirmados. Un firmware distinto requiere su JSON correspondiente.

## Rendimiento y validación

No se modifica el kernel NAM ni el callback de audio del VST. El control de elegibilidad del VST consulta una tabla en memoria. La validación CRC y la copia de pesos suceden al inicializar el modelo; el diario solo se usa durante escrituras o recuperación. No se ha medido latencia física de cambio de modelo ni se promete carga HOT sin cortes de audio.

Los informes `tests/*NH04*REPORT.json` describen qué se ha ejecutado. Las operaciones NOR y los extremos USB se modelan: no se han medido temporización física, desgaste ni fallos eléctricos a mitad de un programa/borrado. Los cortes se inyectan entre llamadas de escritura/borrado; no cubren todos los fallos eléctricos posibles. El arranque de pruebas ejecuta la inicialización nativa y el registro de efectos y después llama al mismo hook compilado de la 1.5; no reproduce la interfaz gráfica completa del aparato.

El motor NAM se procesa en emulación en los 62 destinos. Algunos motores stock usan instrucciones F64 que la configuración M7 de Unicorn no ejecuta: la comprobación de AMP stock activo se hace con la carga nativa y su puntero activo, sin afirmar procesamiento de audio stock exhaustivo. No se excluyen por ello de NAM.

**Pendiente de validación física.** No es una versión estable certificada por las pruebas de software.

## Reconstrucción

Desde las fuentes del botón «Fuentes y pruebas», con Python, Node, Zig/Clang ARM, pyelftools y capstone:

```
python build_unified_profiles.py V180
python build_unified_profiles.py V182
python build_v13.py
python build_v14.py
python nam_inplace/build_runtime.py /ruta/a/zig
python build_inplace.py
```

`build_v14.py` reconstruye la base 1.5 byte a byte; `build_inplace.py` comprueba los hashes antes de aplicar NH04. Para las pruebas JS se necesitan @xmldom/xmldom y linkedom en `test_deps/node_modules`. Ejecutar `node test_inplace.cjs V180` y V182 genera las entradas de las pruebas ARM. Después ejecutar `python nam_inplace/test_arm.py V180` y V182 con Unicorn, y `test_preloaded.py` para los modelos iniciales. Las pruebas VST se ejecutan con `python tests/run_tests.py`; el intercambio nativo con `tests/test_nam_hot_native.py --firmware-root ...`.

El núcleo C++ se ha comprobado contra JUCE real. El chequeo GCC completo del editor conserva los errores previos de argumentos por defecto en ToneMatch; no se presenta como compilación completa Windows verificada.
