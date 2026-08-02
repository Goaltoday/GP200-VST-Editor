AUTO CAB - Source Latest 15

Archivos modificados:
- source/GP200Plugin/PluginEditor.h
- source/GP200Plugin/PluginEditor.cpp
- source/libgp200/MidiConnection.h
- source/libgp200/MidiConnection.cpp

Comportamiento:
- Nuevo botón AUTO CAB entre FX OFF y User IR.
- Activado por defecto.
- Al conectar, envía una vez AUTO CAB = ON para sincronizar el ajuste global.
- Al desactivarlo, envía AUTO CAB = OFF y el CAB queda fijo al cambiar AMP.
- Al reconectar, reaplica el estado visible del botón.
