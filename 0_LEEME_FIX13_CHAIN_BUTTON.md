# GP200 FIX35 y VST FIX13 CHAIN BUTTON

La recepción MIDI de SPR traducía los valores 0/1 a 0x80|END / 0x90|END cuando el modo anterior era ampliado. Por eso desactivar CHAIN desde el VST podía restaurarlo y dejar la confirmación pendiente. El selector físico usa otra entrada y no hacía esa traducción.

FIX35 acepta 0/1 como modos nativos exactos. Conserva los modos ampliados y los setters internos. Cambio respecto a FIX34: instrucción condicional en 0x8028556A de 0DD3 a 07D3, destino 0x8028557C, más checksum FRMW. Sitio de archivo V180 0x2A71E3; V182 0x2A7873. El resto de los bytes coincide con FIX34.

VST FIX13 CHAIN BUTTON conserva las correcciones GUI/BLEND de FIX12 CHAIN BLEND CORRECCIONES. El botón CHAIN envía una sola escritura del modo final, sin modo Series temporal ni reordenamiento. Se comprueba que el routing coincide con el dump live antes de enviar y se espera confirmación fresca de modo, dump live y modo final. Las operaciones de mover bloques siguen su transacción completa. No se reenvía CHAIN automáticamente para forzar una confirmación. Ante timeout se recupera el estado del dispositivo.

## Instalación

Elegir el BIN V180 para GP200 1.8.0 o V182 para 1.8.2. Ambos son completos; no usar en otras versiones/modelos. El HTML dual permite generar desde stock y bases compatibles, preservando los mods reconocidos. Recompilar el VST con la superposición FIX13 sobre la base GP200-VST-Editor-Experimental-firmware (2)(4), FIX11 CHAIN BLEND o FIX12 CHAIN BLEND CORRECCIONES. Se entregan fuentes, no un VST3 compilado. Esta rama FIX13 no identifica variantes históricas con el mismo número.

## Validación

VST: 353 regresiones de conexión, incluidas 48 transiciones alternadas con modos iniciales 0/1/0x85/0x95 y retorno nativo 0/1, rechazo de doble envío, rechazo de orden distinto y recuperación tras modo incorrecto. Además 371 CHAIN/BLEND, 6128 drag, 71 snapshots, 728 paquetes, 512 readback, 192192 routing y 74 PRE. Se ejecutan cuerpos reales con puertos/codec/JUCE simulados; falta build completo JUCE/DAW.

Firmware: 65536 combinaciones old/wanted por perfil ejecutando el receptor Thumb emitido. La continuación nativa está simulada: confirma el valor aceptado, no acredita USB, audio ni persistencia. HTML: handlers reales con DOM/exportación XML simulados; genera los BIN entregados, conserva tap/reset y mods, rechaza corrupción y escrituras parciales, reimporta stock/FIX31/FIX33/FIX34 y su propia salida. FIX31 se prueba en V180.

## Prueba física pendiente

Con ambos componentes actualizados, cargar un preset guardado en CHAIN y alternar OFF/ON varias veces. Comprobar que OFF permanece en el pedal, el botón vuelve a responder y sigue el audio. Repetir con un preset sin CHAIN y cambiando desde el pedal. Comprobar BLEND 0/17/50/100, VOL/EXP, Save/Recall y STORE/reinicio. No se ha probado físicamente FIX35.
