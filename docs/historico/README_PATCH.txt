GP200 preset slot search field

Base: automatic preset-name scanning/cache update patch on source_latest_12.

Replace:
  CMakeLists.txt
  source/GP200Plugin/PluginEditor.cpp
  source/GP200Plugin/PluginEditor.h

Add:
  source/GP200Plugin/PresetSlotSearchPopup.cpp
  source/GP200Plugin/PresetSlotSearchPopup.h

Behaviour:
- Clicking the current slot opens an in-editor preset browser.
- Search filters by preset name or compact slot code (for example 41-C).
- Single-clicking a result loads that slot.
- The list updates while the background name scan continues.
- Escape or X closes the browser.
