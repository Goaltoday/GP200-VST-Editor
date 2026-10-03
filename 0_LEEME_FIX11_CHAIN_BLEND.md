# VST FIX11 — CHAIN / BLEND independiente, firmware FIX33

Basado en el ZIP GP200-VST-Editor-Experimental-firmware (2)(4) adjuntado. Este paquete contiene los archivos fuente modificados con sus rutas. Copiarlos sobre esa versión y recompilar con su flujo habitual (JUCE + CMake / workflow Windows). No es un VST3 compilado: JUCE no está disponible en este entorno.

Botón CHAIN: activo solo para 0x80..0x8b, sin confundir Parallel original (0). Activar conserva S/P/R; desactivar recupera el modo nativo observado antes de activar, o Series si el preset ya se cargó en CHAIN y no existe ese dato. Las respuestas de Series temporal durante la transacción no cambian ese recuerdo. Se conserva lectura/Recall de modos extendidos antiguos, sin convertir presets al cargarlos.

VOL siempre mantiene su nombre/control. Slider separado BLEND con SOLO A, POS, CENTRO, SOLO B. Visible en CHAIN; solo editable si la firma identifica BLEND independiente. NEW BLEND realiza una activación explícita y migra como el pedal: copia el VOL guardado si estaba funcionando como mezcla y restaura VOL a 100, o usa blend 50 sin tocar VOL cuando no era mezcla. No hace STORE automático. En presets antiguos sin activar NEW BLEND se conserva la función histórica de VOL como mezcla.

El slider envía campos 14 y 13 del bloque físico VOL (10), espera un nuevo dump live y confirma el valor real. Save/STORE quedan bloqueados mientras espera. Mantiene los controles visibles durante la lectura y conserva la posición que el usuario haya solicitado dentro del mismo preset. Al cambiar de slot/modo o comenzar Recall/routing, cancela las ediciones pendientes.

Save/Recall A/B y estado del DAW conservan el snapshot completo. Recall ahora incluye valor/firma independientes, al final de los pases de parámetros conocidos; un snapshot antiguo borra la firma anterior para no heredar BLEND de otro preset. El envío normal de VOL no se sustituye. Exportar/importar PRST conserva los 15 campos ya presentes en el codec.

Requiere firmware FIX33 para recepción de BLEND reservado. No se cambian efectos PRE, tonos, audio del plugin ni curva de mezcla. Para persistir en el pedal usar STORE tras confirmación.

Pruebas: regresiones de conexión, snapshots A/B/DAW, paquetes MIDI, PRE, routing y arrastre; 366 comprobaciones adicionales con cuerpos reales de restore/transporte y doubles de JUCE/codec/puertos; receptor Thumb en ambas versiones. Falta compilación completa y prueba física del VST con el pedal. Las pruebas locales no demuestran persistencia Flash ni funcionamiento del host DAW real.
