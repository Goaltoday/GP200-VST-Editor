# VOL -> PRE alias R1

Experimental test build paired with `GP200_UNIFIED_STEP11_VOL_PRE_ALIAS_R1.html`.

Firmware change: only the VOL catalog offset in the package catalog header is changed from `0x40C` to PRE `0x44`. The block index remains VOL (10). No PRE package is copied and no RAM/callback relocation is performed.

VST change: the VOL selector deliberately lists the PRE catalog and resolves dynamic PRE-bank names/parameter layouts for VOL as well. Use this VST only with the alias test firmware.

First physical test: keep PRE OFF, select COMP in VOL, then enable VOL. If stable, change parameters/bypass. Only afterwards test PRE and VOL simultaneously with different PRE algorithms. Do not save important presets until coexistence is understood.
