# Pure 2 experimental en MOD

Esta variante parte de `GP200-VST-Editor-Experimental-firmware (5).zip` y solo
adapta el ID MOD `0x04000021`, anteriormente `O-Trem`, para el firmware V7
Second Pure.

Cambios:

- `O-Trem` pasa a mostrarse como `Pure 2`.
- Usa los cinco controles de Pure: Mix, Time, Feedback, Sync y Trail.
- Time queda limitado a 20-500 ms, acorde con la arena de RAM disponible.

No se añade el ID `0x0B000000` al catálogo MOD. La prueba anterior demostró que
el hardware rechaza IDs DLY en ese bloque.

Este VST debe utilizarse únicamente después de instalar el mod de firmware
`Segundo Pure en MOD`. Sin dicho firmware, `Pure 2` seguirá ejecutando el
O-Trem original aunque el editor muestre los controles nuevos.
