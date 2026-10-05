# Series / Parallel button change

This source update adds the Series / Parallel button in the utility bar, between Import IR and Tone Match. It sends the captured GP-200 SysEx route message; byte 42 is `0x00` for Parallel and `0x01` for Series. No preset, effect order, or other existing action is changed.

The source patch was checked byte-for-byte against both route messages in `series paralel.pcapng`.

The bundled `vst3/GP200 VST.vst3` is retained from the uploaded archive and was not rebuilt. Build the project to produce a VST3 containing this change. The source archive has an empty `external/JUCE` submodule in this environment, and CMake is unavailable here.
