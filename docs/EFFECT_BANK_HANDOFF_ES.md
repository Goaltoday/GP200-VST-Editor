# Handoff — VST 0.1.9 / banco genérico

Fecha: 17/10/2026. Base de código: entrega NAM2 0.1.8 de este chat, confirmada por el usuario como funcionando. El firmware y el kernel M7OPT no se modifican. La autoridad del contrato JSON se extrajo de `sources/UNIFIED_1.3/inner_base.html`, funciones `exInstall`/`exSnapshot` y catálogo `EX_DATA` del consolidado 1.3.

## Contrato importado

Archivo: Documentos/GP200/GP200_EFFECT_BANK.json. Esquema `gp200-effect-bank/v1`; producto GP-200; firmware V180/V182; `firmware_sha256`; array `slots`, máximo 80. El indicador histórico `vst_generic_support:false` describe el VST anterior y no impide la lectura en esta versión.

Cada asignación incluye `destination_module`, `destination_effect_id`, `source_module`, `source_effect_id`, `display_name`, `profile` y `parameters`. Cada parámetro aporta `index`, `name`, `minimum`, `maximum`, `step`, `default` y flags nativos. Se admiten índices 0–14 y hasta 15 parámetros, finitos y coherentes. Los IDs 0 son válidos (COMP) y no significan ausencia de asignación. DIST se normaliza a DST.

Se validan destinos/fuentes frente al catálogo del VST, claves duplicadas, versiones, hash y rangos. Tamaño de archivo limitado a 1 MiB. Solo se publica el nuevo banco si termina toda la validación. Un archivo parcial o inválido mantiene el banco válido anterior, con aviso. Cuando existe hash/versión en el manifiesto MOD_SYNC se comprueba que coincida; no es una autenticación del firmware físico por MIDI.

## Modelo en memoria

`GP200EffectBankEntry` contiene identificación por módulo/ID, nombres propios, tabla de parámetros y la correspondencia con los índices del catálogo anterior para opciones discretas. Se construye mediante shared_ptr y se publica como const; no se mueve después de fijar los punteros de nombres/tablas.

`GP200ModSync` mantiene un mapa de clave `(módulo, ID)`. Los getters no hacen I/O. `findParamsForEffect(id,module)` mantiene una referencia de vida thread_local al entry devuelto; como en la API previa, el puntero se utiliza inmediatamente por la UI y no debe almacenarse como un handle indefinido entre consultas. Los lectores que necesiten conservar una versión deben retener el shared_ptr de `findEffectBankEntry`.

La sincronización PRE antigua y WAH local quedan subordinadas a un banco genérico activo, incluso en destinos omitidos del array. Esto evita resucitar una asignación antigua al restaurar un hueco stock. El banco genérico no se escribe desde MIDI: persiste en su archivo original.

## Integración

- `GP200ModSync.h/.cpp`: lectura/validación, claves por bloque, ownership, prioridad, estado y sondeo acotado.
- `GP200EffectParamDatabase.h/.cpp`: perfiles exactos del JSON y sobrecargas por módulo para opciones discretas.
- `GP200EffectDatabase.cpp`: nombres/descripciones por bloque y destino Micro Boost en DST, ausente en el listado anterior aunque está en el catálogo de firmware.
- `EffectBlockComponent.cpp`: da prioridad al nombre genérico frente al resolver stock, controles con índices/rangos importados, opciones por bloque y Time/Sync para instancias reubicadas. Reconoce Time A/Time B y sus sincronías A/B; no confunde Time R% con un tiempo en milisegundos. Conserva la modalidad mostrada por cada control Time y solo solicita reconstrucción si esa modalidad cambia; un readback Sync idéntico no reconstruye controles en cada refresco.
- `GP200Preset.cpp`: resumen y detalle de parámetros conscientes del bloque.
- `MidiConnection.cpp`: inicialización de defaults por módulo al seleccionar efecto; conserva el ID de destino en el transporte.
- `PluginEditor.cpp`: llama al sondeo de metadatos y muestra el estado de importación. La revisión de metadatos provoca la reconstrucción ya prevista de controles.
- `GP200SoundClone.cpp` y guards de MidiConnection: fuerzan una revisión explícita en las operaciones de escritura porque los getters ya no revisan disco.

La serialización MIDI de cambios de efecto y parámetro y el callback de audio se conservan byte por byte (hash de sus cuerpos) respecto a 0.1.8. Se mantienen IDs del plugin y formato de estado DAW. No se añade transferencia SC10, DSP Load remoto ni protocolo MIDI genérico.

## Rendimiento y límites de evidencia

La ruta de audio (`processBlock`, tuner y captura Tone Match) no se modifica ni consulta el banco. El firmware, su carga DSP y su SRC/FIFO permanecen fuera de esta actualización.

El trabajo añadido se ejecuta en inicialización o hilo de control/interfaz: validación de archivos cambiados y búsqueda en memoria. Se reutiliza el timer del editor; como máximo se revisan cambios una vez por segundo. No se añade timer de consultas MIDI ni se envían datos por importar los JSON.

La revisión anterior comprobaba repetidamente los archivos desde los getters. Se retira esa E/S de todas las consultas pasivas. Las operaciones explícitas de importación/refresco MIDI y escritura sí pueden revisar archivos. Los locks son los del subsistema de metadatos y no se han introducido en audio.

Los tests cuentan stat/read/parse en 100.000 iteraciones (400.000 consultas): cero E/S/parseo en la ruta caliente. El benchmark host compara el mismo tipo de consultas con 0.1.8: el baseline aún no entiende el banco genérico y devuelve contenido stock; la comparación acredita el ahorro de revisiones de disco, no equivalencia de funcionalidad ni una cifra de rendimiento de Windows/DAW. Los tiempos dependen del host; no extrapolarlos a porcentajes DSP de GP200.

## Pruebas

`tests/run_effect_bank_tests.py` compila ModSync y ambas bases de datos reales con dobles de JUCE/archivos/JSON. Extrae los métodos reales de nombres de la UI, detección Time/Sync, inicialización de defaults y envío MIDI. No se sustituye la lógica comprobada por una segunda implementación.

Cobertura:

- 80 destinos (WAH5, PRE31, DST38, EQ6).
- 130 fuentes con alguna asignación permitida en el catálogo 1.3; el catálogo contiene 140 en total, diez sin uso en esas elecciones.
- ID compartido en PRE y DST con algoritmos distintos y alias DIST/DST.
- Parámetros de cada fixture: nombres, índices, rangos, pasos, defaults y opciones discretas; Time/Feedback con orden adaptado.
- Pure 2.000 ms; detección de sincronías de delays y A/B de Dual Echo.
- Prioridad de nombres frente a un resolver stock y persistencia en un proceso nuevo.
- Defaults y mensajes con ID físico en los cuatro bloques, sin emitir MIDI al importar o inicializar solo metadatos.
- Sustitución/retirada, JSON inválido, duplicados, límites, colisiones de hash/versión y prioridad frente al refresco PRE antiguo.
- Ausencia de E/S y parseo en getters; sondeo limitado; hashes de callback/serializadores.
- Regresiones NAM2/HOT1/HOT2, Scenes, CHAIN, routing, snapshots y recepción previas.

Fixtures: parámetros extraídos del catálogo incluido en 1.3; no son configuraciones para instalar como si pertenecieran a un BIN real. Los logs y RESULTS.json especifican qué se ejecutó. No hay enlace JUCE, render de la GUI ni validación Windows/DAW/USB física de 0.1.9 en este entorno.

## Continuar

Empezar por README_EFFECT_BANK_ES.md. Usar ambos JSON de la misma generación. La siguiente fase, si se solicita, es ampliar de forma coordinada el protocolo del firmware y el VST para identificar NAM y el banco genérico directamente desde el equipo. No anunciar esa identificación automática como existente.
