# Handoff NAM2 — VST 0.1.8

Fecha: 17/10/2026. Base: `antecedentes/CF0.3/sources/VST` del paquete Unified M7OPT 1.3. La revisión analizada anteriormente confirmó que `source/` de esa base coincidía byte por byte con el ZIP VST (9) adjunto. Tras la limpieza del espacio se recuperó esa base del paquete suministrado. No se ha usado el VST original antiguo como base ni se ha modificado firmware.

## Contrato que se implementa

La autoridad de instalación sigue siendo el HTML Unified 1.3 y el BIN generado. `GP200_MOD_SYNC.json` mantiene `factory_amp_overrides` (CLO5) y `nam_amp_overrides` (NAM2) separados. El VST lee esos perfiles por su ID físico, conserva el nombre y utiliza el transporte normal de parámetros.

Ejemplo de entrada NAM exportada (el ID, nombre y hash reales proceden de la generación; no instalar un ejemplo como metadatos de otro BIN):

```json
{
  "effect_id": 117440513,
  "display_name": "MY_NAM",
  "parameter_profile": "NAM2",
  "parameter_count": 2,
  "controls": ["Input", "Output"],
  "neutral": 50
}
```

Debe estar en `nam_amp_overrides`, en un documento de esquema `gp200-mod-sync/v1`, producto `GP-200`, firmware `V180` o `V182`, con `firmware_sha256` de 64 dígitos hexadecimales. No se deduce el perfil a partir del nombre, de la familia AMP ni del número de controles del modelo stock.

## Código modificado

| Archivo | Cambio |
| --- | --- |
| `GP200ModSync.h/.cpp` | Marca NAM independiente de CLO; valida documentos y colisiones; conserva NAM al refrescar MS11; serializa perfil, controles, neutro, versión y SHA importados; declara su procedencia no verificada por MIDI. |
| `GP200EffectParamDatabase.cpp` | Input P0 y Output P1 continuos 0–100/default50; desactiva metadatos discretos stock para los destinos NAM. |
| `EffectBlockComponent.cpp` | Extiende la excepción de opciones discretas a NAM. La tabla dinámica determina los dos controles; no altera los valores del preset al mostrarlos. |
| `PluginEditor.cpp` | Deshabilita destinos NAM y Factory sin mapa CLO en el navegador Sound Clone; vuelve a comprobar los clics y actualiza los destinos cuando cambia la revisión de metadatos. |
| `GP200SoundClone.cpp` | Los dos constructores Factory HOT exigen destino CLO conocido. Protege también a consumidores que no pasan por la interfaz. |
| `MidiConnection.cpp` | Guards anteriores al prepare de HOT1/HOT2; revalidación antes del commit; rechaza parámetros NAM P2–P14 y valores fuera de rango/no finitos; muestra procedencia NAM tras MOD_SYNC. |
| `CMakeLists.txt` | Versión 0.1.8, sin cambiar IDs del plugin ni el formato DAW. |

## Caché y concurrencia

`GP200ModSync` mantiene el lock recursivo previo. La carga prepara un mapa temporal y solo publica perfiles si la validación termina correctamente. Los JSON inválidos no eliminan la protección NAM previa. Un reemplazo válido del archivo sustituye el mapa completo, incluidos los NAM retirados.

El refresco MS11 importa primero cualquier archivo local pendiente, incluso si es la primera llamada de la sesión. Después prepara CAB/CLO en un mapa temporal y añade los NAM anteriores: la ausencia de un registro CLO no identifica la ausencia de NAM. Si el dispositivo anuncia CLO en un destino NAM, se rechaza el mapa y se informa del conflicto.

Un fallo de escritura conserva el snapshot en RAM. La misma copia de disco no lo sustituye en el siguiente getter. Un archivo nuevo sí permite recuperar/reemplazar metadatos. La caché guarda `nam_metadata_source: generated-manifest-not-verified-by-ms11`; el SHA importado no se presenta como lectura del dispositivo.

Los guardados derivados de otras operaciones conservan las entradas NAM. `recordFactoryAmpOverride` vuelve a leer los cambios pendientes y no convierte una entrada NAM en CLO al completar una transferencia anterior.

## Protecciones y alcance

- Sound Clone no puede iniciar prepare para un NAM identificado, stock o destino sin CLO conocido.
- Los constructores HOT1/HOT2 aplican la misma política.
- Antes del commit se vuelve a consultar el mapa; un destino convertido en NAM durante el staging cancela el commit. Los chunks ya enviados son staging: no se solicita escritura Factory.
- HOT1 conserva el guard previo de AMP no seleccionado y preset vivo. No se extiende esa condición a HOT2 más allá del comportamiento anterior.
- Los diez destinos SnapTone normales y User IR/Factory CAB no pasan por este nuevo guard de Factory AMP.
- Si se suministra un JSON de otro firmware, MS11 no puede resolver todos los casos. Es obligatorio usar el JSON de la generación instalada. La protección del VST depende de esa correspondencia; no se afirma autenticación física.

## Resultados y límites

Los logs de `docs/nam2_validation` registran regresiones previas, pruebas NAM2, recepción y catálogo. Las nuevas pruebas cubren los 71 destinos AMP, cinco NAM simultáneamente en metadatos, valores de controles y paquetes MIDI, arranque con importación, refresco, proceso nuevo, errores de guardado, metadatos malformados, colisiones y cambios durante HOT1/HOT2.

Las pruebas compilan código de producción con dobles de JUCE. El archivo `tests/nam2_support/JuceHeader.h` es exclusivamente un doble de prueba; no es una copia de JUCE y no se añade al target del producto. Los métodos de transporte se extraen al ejecutar el test, sin mantener una segunda implementación de sus guards.

No se ha enlazado VST3/Standalone con JUCE ni hecho una prueba Windows, DAW, GUI, USB o audio físico. Tampoco se ha añadido protocolo NAM2 a firmware: `GP200ModSyncProtocol.h` conserva el decoder MS11 real y no acepta un formato de paquete inventado.

## Comprobación física propuesta

1. Instalar/usar un BIN Unified 1.3 y copiar su JSON exacto.
2. Conmutar entre AMP stock, CLO y cada NAM; comprobar nombres, dos controles NAM y cinco CLO.
3. Editar Input/Output, comprobar en GP200 y hacer cambios desde la pedalera hacia el VST.
4. Desconectar/reconectar y reiniciar el plugin; confirmar conservación del perfil y de los valores guardados del preset.
5. Verificar que el navegador Sound Clone deshabilita NAM y que HOT1/HOT2 siguen funcionando en un destino CLO libre, conservando el NAM.
6. Verificar selección/captura de Scenes, Store y CHAIN con los mismos presets. No presentar un silencio Scene+CHAIN como corregido por esta entrega.
7. Repetir en V180 y V182 y probar DAW y Standalone.

## Siguiente trabajo

Banco genérico WAH/PRE/DIST/EQ; después una extensión acordada de protocolo para identificar NAM y el mapa completo desde el equipo. Mantener NAM2 y CLO5 separados, conservar el kernel M7OPT y el resto de decisiones Unified 1.3.
