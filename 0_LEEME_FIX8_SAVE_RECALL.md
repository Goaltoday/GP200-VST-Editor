# FIX8 — Save/Recall from DAW: Series/Parallel y P

Corrección limitada al guardado y recuperación de snapshots del DAW, sobre las fuentes FIX7. El firmware no cambia y no se incluye una actualización de firmware.

## Causa y solución

El snapshot guardaba el bloque del preset, pero el modo Series/Parallel ampliado se consultaba por separado y no se añadía al estado del proyecto. Recall restauraba parámetros, orden y marcadores Send/Return, sin enviar ese modo.

Save conectado captura ahora el modo confirmado con el preset. Los snapshots A y B conservan cada uno su modo en el estado del DAW (XML versión 8). P va incluido en el byte ampliado de routing. Recall restaura el preset con la secuencia existente y envía el modo al final, 150 ms después de ordenar los bloques. Luego espera una lectura nueva del preset y del modo antes de permitir Store o un nuevo Save. El envío final comprueba que no ha cambiado el slot ni su generación.

Los valores ampliados 80–8B y 90–9B recuperan el modo y P exactos. Los valores nativos 00/01 recuperan Series/Parallel; esos valores no contienen P y el firmware puede conservar el P actual.

## Compatibilidad y compilación

Los proyectos anteriores se pueden abrir. Si un snapshot fue guardado antes de FIX8, no contiene el modo: Recall avisa y restaura los datos que sí existen. Pon el routing deseado y pulsa Save de nuevo con FIX8 para actualizar ese snapshot. No se deduce el modo a partir de offsets desconocidos del preset.

Este ZIP contiene fuentes, no un VST3 precompilado. Configura JUCE y utiliza `build_release.cmd` con CMake/Visual Studio según el proyecto. Se conservan el identificador, nombre y versión CMake de FIX7 para limitar el parche a este problema. Cierra el host antes de sustituir el VST3 compilado. Sigue usando tu firmware actual compatible con FIX7.

`FIX7_to_FIX8_SAVE_RECALL.patch` contiene únicamente los seis archivos de producción modificados. Los documentos FIX7 incluidos son históricos; para esta entrega prevalece este documento.

## Prueba en tu equipo

1. Con el pedal conectado, selecciona Series, ajusta S/P/R y guarda A. Espera a que el estado esté confirmado antes de Save.
2. Selecciona Parallel con otra P válida y guarda B.
3. Guarda el proyecto del DAW, ciérralo y vuelve a abrirlo.
4. Con un preset distinto en el pedal, pulsa Recall A y después Recall B. Espera la confirmación en cada caso; comprueba Series/Parallel, P y Send/Return en la interfaz y el sonido del pedal.
5. Tras confirmar, usa Store en un slot de prueba y comprueba el resultado al cambiar y volver a ese preset.

## Validación y límites

71 pruebas de Save/Recall y persistencia A/B, 302 de conexión/lectura/guardado, 728 de paquetes de modo, 512 de lectura de modo y 192192 de movimientos de routing pasan. Ejecuta `python3 tests/run_tests.py` (Python 3 y g++ con C++20).

Las pruebas extraen métodos reales de producción, con transporte, codec y XML JUCE simulados. La reproducción de parámetros que no cambia se simula hasta el reorder final. No equivalen a compilar el plugin completo ni a probar USB/audio/DAW con la GP-200 real. Esa validación física queda pendiente.

No cambia el firmware, DSP, asignación de expresión, IR/HOT, algoritmo existente de restauración de parámetros ni formato PRST.
