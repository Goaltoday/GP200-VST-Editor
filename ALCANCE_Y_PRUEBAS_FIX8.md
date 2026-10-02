# Alcance comprobado respecto a FIX7

Archivos de producción modificados:

- `source/GP200Plugin/PluginEditor.cpp`
- `source/GP200Plugin/PluginEditor.h`
- `source/GP200Plugin/PluginProcessor.cpp`
- `source/GP200Plugin/PluginProcessor.h`
- `source/libgp200/MidiConnection.cpp`
- `source/libgp200/MidiConnection.h`

Todos los demás archivos originales fuera de tests son idénticos antes de añadir esta documentación y el parche. No se eliminan archivos originales. Los tests añadidos comprueban la persistencia del modo y la confirmación tras Recall.

Resultados: 71 Save/Recall, 302 conexión, 728 paquetes, 512 readback, 192192 movimientos.
