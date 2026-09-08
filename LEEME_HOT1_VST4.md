# HOT1 para GP200 VST Experimental (4)

Base exacta: `GP200-VST-Editor-Experimental-firmware (4).zip`
SHA-256 de la base: `d54022126700a271a755d3f57af2d932477793e41ea97d7dcff5997dc55e1e74`

Copia la carpeta `source` de este ZIP sobre la raíz de tu proyecto (4),
conservando las rutas, y ejecuta `build_release_hot1_clean.cmd` desde la raíz del proyecto. Usa `build_hot1` para impedir que se reutilice un objeto anterior. Se sustituyen cinco archivos:

- `source/libgp200/MidiConnection.cpp`
- `source/libgp200/MidiConnection.h`
- `source/libgp200/GP200SoundClone.cpp`
- `source/libgp200/GP200SoundClone.h`
- `source/GP200Plugin/PluginEditor.cpp`

Este VST requiere el firmware STEP11 HOT1 INACTIVE. No incluye un VST3
precompilado y no debes reutilizar el VST3 antiguo incluido en la base.

## Prueba

1. Abre una sola instancia y espera a que recupere el preset y termine MOD_SYNC.
2. En la GP-200 selecciona un AMP distinto del slot Factory que vas a convertir.
   Poner el destino en bypass no basta.
3. Importa el CLO en el destino Factory.
4. No cambies preset ni AMP durante la transferencia y no uses otra instancia
   o editor MIDI para modificar la GP-200.
5. Espera `transfer sent (no activation ACK)` y selecciona el destino.
6. Comprueba audio y los cinco controles CLO sin reiniciar la GP-200.

El VST comprueba al comenzar y antes del commit que el destino no está
seleccionado en ninguno de los 11 bloques del preset vivo. Si cambia o pierde
el estado vivo, cancela antes del commit.

El nombre/perfil local se registra después de enviar el commit. Sigue siendo
optimista porque HOT1 aún no añade una respuesta de confirmación desde la GP-200.

## Alcance conservado

La conexión automática, el temporizador conectado, MOD_SYNC, las múltiples
instancias, los tiempos de transferencia y el resto de la versión (4) no se
modifican. HOT1 solo añade la marca `0xA1`, la protección del destino y el
registro posterior al commit.

Validación local: catálogo real de 71 AMP, todos los 11 bloques, estados sin
preset vivo/restauración, orden de commit y comparación exacta del temporizador.
Compilación VST3 Windows y prueba física pendientes.

## Corrección V2 tras captura física

La captura `captura_clo.pcapng` mostró que el VST probado envió wrapper byte 11 = `0x00`, aunque el BIN sí contenía el helper HOT1. Por eso el firmware usó la ruta anterior. Esta versión comprueba el primer bloque SysEx ya codificado antes de comenzar. Debe contener `0A 01` en las posiciones que representan `0xA1`; si falta, aborta con `HOT1 build mismatch`.

## Corrección V3 para GitHub Actions

Se añade `#include "GP200EffectDatabase.h"` a `MidiConnection.cpp`. Sin esta
cabecera MSVC no puede resolver `GP200EffectDatabase::getEffectsForModule` y
detiene la compilación en las antiguas líneas 248 y 408. Los errores posteriores
de `begin`, `end` y el bucle `for` eran consecuencias del mismo tipo desconocido.

## Cambio V4: nombre Factory AMP editable

El campo de nombre vuelve a estar habilitado al seleccionar un Factory AMP.
Al elegir un archivo se rellena con su nombre, limitado a 16 caracteres, pero
puede editarse antes de pulsar `Import selected clone`. El texto elegido viaja
en el wrapper HOT1, se guarda en `slot + 0x1300` y MOD_SYNC lo recupera después.

Para cambiar el nombre de un CLO ya importado, selecciona el mismo archivo CLO,
escribe el nombre nuevo y vuelve a importarlo en el mismo slot. Esa sustitución
es hot porque el slot ya está convertido. El botón `Rename` continúa reservado
para SnapTone; Factory AMP aplica el nombre mediante la importación.
