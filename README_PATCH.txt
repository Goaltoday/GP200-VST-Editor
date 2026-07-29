GP200 automatic preset-name scan and cache refresh patch
Base: cumulative preset-slot browser patch on source_latest_12

Replace:
- CMakeLists.txt
- source/GP200Plugin/PluginEditor.cpp
- source/GP200Plugin/PluginEditor.h
- source/libgp200/MidiConnection.cpp
- source/libgp200/MidiConnection.h
- source/libgp200/GP200PresetNameScanner.cpp
- source/libgp200/GP200PresetNameScanner.h

Behaviour:
- Automatic low-priority scan starts 900 ms after the initial live preset is ready.
- One SysEx request remains in flight at a time; normal preset/IR/CLO operations have priority.
- Clicking the slot opens the menu immediately using cached/partial names.
- Store/rename and incoming live preset names update the affected cached slot directly.
- An interrupted scan restarts later and skips names already cached.
