# Notas de la refactorización v1

## Alcance

Esta entrega conserva el comportamiento observable del proyecto recibido. No sustituye el
protocolo MIDI ni rediseña toda la interfaz. El objetivo es corregir problemas reales y crear
una base más segura para refactorizaciones posteriores.

## Cambios aplicados

1. `GP200EffectDescription` usa `effectId`, no el nombre visible. Esto evita colisiones entre
   efectos llamados igual en módulos diferentes.
2. Los 305 mapas de parámetros apuntan a 133 diseños compartidos. Se conserva cada ID y su
   número de parámetros.
3. Los tipos de parámetro son `GP200ParamKind`, eliminando comparaciones de texto y el typo
   histórico `combox`.
4. Los tamaños y offsets compartidos están en `GP200Constants.h`.
5. `MidiConnection` reutiliza `MidiDeviceScanner`, elimina includes duplicados, protege el
   estado compartido y publica revisiones de preset y nombres asignados.
6. `PluginEditor` deja de producir firmas Base64 en cada ciclo. Usa las revisiones y reduce el
   sondeo normal a 20 Hz.
7. El snapshot restaurado solo se adopta localmente al terminar la secuencia de restauración.
8. Se eliminan helpers y miembros sin uso, y se normaliza el formato del código.

## Cambios deliberadamente aplazados

- Extraer el protocolo SysEx a un codificador/decodificador independiente.
- Mover la conexión MIDI fuera de la vida de la ventana del editor.
- Crear un motor de restauración con ACK, timeout, reintentos y cancelación.
- Actualizar los once bloques de manera incremental en vez de reconstruirlos.
- Añadir etiquetas completas para todos los parámetros `choice`.
- Generar los catálogos desde CSV/JSON como fuente única.

Esos cambios son mayores y deben hacerse uno por uno, con pruebas de hardware después de
cada commit.

## Validación incluida

Ejecutar desde la raíz:

```cmd
python tools\validate_catalogs.py
```

Comprueba IDs duplicados o desordenados, referencias inexistentes, tamaños incorrectos de
layouts y restos de tipos de parámetro basados en cadenas.

## Validación necesaria en Windows

1. Compilar Debug.
2. Abrir el VST3 en Studio One.
3. Conectar el GP-200.
4. Probar lectura de preset y nombre.
5. Probar cambio de efecto, parámetros y ON/OFF.
6. Probar reordenamiento.
7. Verificar nombres User IR y SnapTone.
8. Probar Save Preset, Recall Preset y Store preset.

No mezclar esta validación con otros cambios funcionales.
