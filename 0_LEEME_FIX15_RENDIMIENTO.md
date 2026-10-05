# FIX15 — rendimiento de CHAIN/MIDI

Base: FIX14. Fuentes completas; requiere compilar con JUCE.

Routing comprueba revisiones antes de decodificar. BLEND reutiliza la decodificación hasta cambiar la revisión, el slot o la conexión. Las consultas de pantalla omiten el cálculo de permiso de guardar; guardar/exportar conservan sus validaciones. La lista de efectos copia el preset después de comprobar cambios. La recepción SysEx prepara el diagnóstico fuera del bloqueo compartido y copia el bloque de datos conservando su contenido.

No se modifican mensajes, temporizadores, confirmaciones, audio ni formato de presets. No se añaden controles ALIGN/Ø B.

Pruebas: `python3 tests/run_tests.py` y `python3 tests/test_receive.py`. Usan métodos de producción con JUCE/puertos simulados. Compilación completa y prueba en hardware pendientes. Ver `LEEME_R5.md` del paquete principal.
