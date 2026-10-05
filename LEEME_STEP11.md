# STEP11 — Sincronización MIDI al iniciar el VST

Versión experimental: validada localmente; pendiente de compilación VST3 Windows y prueba en GP-200.

## Bases

- VST experimental proporcionado: SHA-256 `088603c85085c2cad386b6d17005dcc424520683a2f9722557555345f9f965f0`.
- HTML STEP10: SHA-256 `0f40e1e93fb233e6f8431191fc3bf8c9592386bebc2d18fb65dbb1ee80c5ed31`.
- Firmware objetivo: GP-200 V1.8.0.

## Instalación

Copiar la carpeta `source` del ZIP sobre la raíz del proyecto experimental, conservando las rutas. Son seis archivos modificados y un header nuevo:

- source/GP200Plugin/PluginEditor.cpp
- source/GP200Plugin/PluginProcessor.cpp
- source/libgp200/MidiConnection.cpp
- source/libgp200/MidiConnection.h
- source/libgp200/GP200ModSync.cpp
- source/libgp200/GP200ModSync.h
- source/libgp200/GP200ModSyncProtocol.h (nuevo)

No hace falta cambiar CMakeLists.txt: el header nuevo se incluye desde MidiConnection.h.
Compilar el VST3 con el procedimiento Windows habitual. No se incluye un binario compilado.

Abrir el HTML STEP11 entregado por separado, cargar el BIN compatible, seleccionar los mods y las sustituciones deseadas y generar el ZIP. El handler MIDI se añade siempre al generar con STEP11. Los mods IR2048, Factory SAFE y DSP conservan su selección independiente y su política aditiva. Instalar el BIN y reiniciar la pedalera antes de iniciar la nueva instancia del VST.

## Funcionamiento

El gestor MIDI intenta la primera conexión automáticamente. El handshake y las lecturas iniciales avanzan en su temporizador, incluso sin abrir la ventana. Tras identidad, modo editor, State Dump y lectura del preset activo, consulta las capacidades y 18 páginas de nombres/perfiles. Después continúa con User IR/SnapTone.

Una sola sincronización por instancia al conseguir conexión. No se repite al reabrir la ventana, cambiar de preset ni realizar una sustitución hot. Si se interrumpe después de comenzar o cambia el firmware, crear una instancia nueva para consultar de nuevo.

El VST conserva la caché durante la recepción. Solo aplica la fotografía completa; elimina también los overrides AMP que han vuelto a stock. Guarda automáticamente `Documentos/GP200/GP200_MOD_SYNC.json`. El JSON incluido en el ZIP del firmware es un respaldo opcional: con ambos componentes STEP11 no hay que copiarlo manualmente.

Timeout de 800 ms y hasta dos reintentos por página. Sin respuesta válida se conserva la caché anterior, que no se considera verificada para la pedalera conectada. Una consulta iniciada se cancela si empieza una transferencia IR/CLO o restauración de preset. Los uploads hot mantienen su protocolo.

## Firmware

La extensión reutiliza la consulta de nombres User IR (registro 0x1009, transporte 0x11, 28 bytes), con índice reservado 0x4D53 y firma MS11. El handler stock descarta ese índice por estar fuera de 0–19. No se introduce un opcode de escritura.

El callback en file offset 0x556C5 se redirige a una cave de 534 bytes en 0x2A0E09 (dirección de ejecución 0x8027F190). Se comprueban los bytes originales o la instalación propia antes de escribir. No se solapa con el runtime SAFE ni con DSP Load. El ensamblador se incluye dentro del HTML como texto para futuras revisiones.

Respuesta de 168 bytes decodificados: firma, versión, nonce de consulta, página, capacidades, conteos y checksum. Página 0: capacidades; 1–9: CAB; 10–18: AMP. Cada página de datos contiene hasta ocho registros de estado, número de parámetros y nombre de 16 bytes. Los nombres CAB se obtienen mediante el lector del catálogo del firmware y se limitan a 12 caracteres.

Los AMP se leen del backing RAM que emplea SAFE: A5, VTSI 1024, layout y CRC válidos permiten anunciar CLO5. Sin SAFE instalado se anuncian AMP stock. Un slot A5 inválido impide aceptar la fotografía completa, en lugar de inventar un perfil. Las 71 correspondencias índice/Effect ID coinciden con la tabla SAFE de la base.

El HTML también valida longitud y CRC antes de convertir CLO; valida los slots del snapshot; deriva IR1024/2048 de los parches reales y muestra los nombres de AMP precargados. No modifica el código de upload SAFE ni el startup SAFE existentes.

## Validación realizada

- Emulación ARM del handler: las 19 páginas, comprobación CRC, fallback stock, páginas fuera de rango, preservación de registros y alineación de pila.
- Emulación del dispatcher original 0x1CF58: alcanza el callback modificado y produce la respuesta MS11.
- Guardia de escritura en emulación: el handler escribe únicamente su pila; no escribe flash ni los slots.
- Parser C++ compilado y ejecutado con las respuestas de la emulación ARM: rechaza nonce/página incorrectos, checksum alterado, nibbles inválidos, offsets y truncamientos; UndefinedBehaviorSanitizer sin errores.
- HTML: sintaxis JavaScript, ocho combinaciones de mods, checksum FRMW, reaplicación idempotente y rechazo de CLO/slot/cave inválidos.
- MidiConnection.cpp y GP200ModSync.cpp: comprobación de sintaxis con GCC y headers JUCE 8.0.8.
- PluginEditor.cpp y PluginProcessor.cpp: la comprobación GCC presenta los mismos 15 diagnósticos de ToneMatch en la base y en la versión modificada. No se han alterado esos componentes ajenos al cambio. Esto no equivale a una compilación VST3 Windows satisfactoria.

## Prueba pendiente en hardware

1. Generar un BIN STEP11 con un CAB renombrado y un AMP CLO precargado, instalarlo y reiniciar.
2. Iniciar el VST3 recompilado. Verificar nombre CAB, nombre AMP y sus cinco parámetros.
3. Cerrar y abrir solo la ventana: no debe repetir MOD_SYNC.
4. Comprobar una sustitución hot como en la base validada.
5. Con firmware anterior sin STEP11, comprobar que el timeout permite continuar el arranque conservando la caché.

No se ha flasheado una pedalera desde este entorno.
