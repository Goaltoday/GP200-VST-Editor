REVERSION DEL NAVEGADOR DE SLOTS

Base restaurada: source_latest_12.zip

Sustituir:
- CMakeLists.txt
- source/GP200Plugin/PluginEditor.cpp
- source/GP200Plugin/PluginEditor.h
- source/libgp200/MidiConnection.cpp
- source/libgp200/MidiConnection.h

Eliminar si existen:
- source/GP200Plugin/PresetSlotBrowser.cpp
- source/GP200Plugin/PresetSlotBrowser.h
- source/libgp200/GP200PresetNameScanner.cpp
- source/libgp200/GP200PresetNameScanner.h

Después borrar completamente la carpeta build y recompilar para evitar que queden objetos de la versión defectuosa.
