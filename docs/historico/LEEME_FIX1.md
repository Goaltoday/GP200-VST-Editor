# R15 SPR FIX1 — prueba de routing real

Esta revisión corrige el VST; el firmware R15 anterior se mantiene.
No incluye un VST3 compilado. Compila con JUCE en tu entorno habitual.

Cambios:
- Antes de editar, importa el orden y Send/Return del preset recibido, evitando conservar el orden offline. P no se puede recuperar de forma fiable con el getter actual: comprueba su posición en la cinta antes de enviar.
- Cada edición envía Serie, espera al menos 150 ms, envía orden/Send/Return, espera al menos otros 150 ms y envía el modo ampliado. Una nueva edición sustituye la transacción pendiente.
- No envía sin conexión o sin un preset recibido válido. Cancela al desconectar o fallar el envío.
- Indica VOL: OFF, LEVEL o BLEND según la ruta dibujada. Este indicador no confirma el estado DSP del pedal.

Prueba:
1. Conecta y espera a que cargue el preset real antes de editar.
2. Coloca un efecto reconocible en A y otro distinto en B. Configura S/P/R para que ambos queden dentro de sus respectivas ramas.
3. Activa VOL y colócalo en OUT COMMON, después de Return. Selecciona PARALLEL y espera a que termine el envío.
4. En la implementación prevista, Blend=0 deja solo A, Blend=100 deja solo B. Que B no suene en 0 es esperado; no demuestra que sea Serie.
5. Alterna SERIES/PARALLEL y compara los extremos. Si no hay diferencia, la aplicación física del modo sigue sin verificarse; hará falta contrastar el tráfico MIDI y la recepción/DSP del firmware.

Verificación local: 728 paquetes de modo y pruebas sobre el cuerpo real de los métodos de envío (tiempos, secuencia, sustitución, guardas, cancelación y fallo). No se ha compilado el plugin completo porque JUCE no está incluido. No se ha comprobado esta revisión en hardware. El estado «mode sent» significa envío MIDI, no acuse ni confirmación de paralelo.

Limitación: después de editar, la cinta conserva el borrador; para esta prueba no cambies de preset durante una transacción y reabre el editor antes de probar otro preset.
