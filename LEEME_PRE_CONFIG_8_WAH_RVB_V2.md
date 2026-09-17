# PRE CONFIG 8 — WAH/RVB V2

Esta versión amplía PRE CONFIG 8 sin cambiar la arquitectura del VST.

## Familias disponibles

- MOD: receta MOD→PRE ya usada por PRE BANK.
- WAH: nueva receta WAH→PRE. Se reconstruyó usando Hammy, que existe en WAH y PRE. La transformación aplicada a Hammy WAH reproduce byte a byte el package Hammy PRE del BIN estable `0da4482a...`.
- DLY: experimental, igual que en PRE CONFIG 8 V1.
- RVB: experimental. Se aplican las traducciones estáticas candidatas de estado, arena alta y callbacks obtenidas del firmware V1.8.0.

El HTML sigue filtrando por capacidad física de cada uno de los 8 huecos PRE. Un efecto que no cabe no aparece en ese hueco.

## VST

No ha sido necesario cambiar la lógica C++ respecto a PRE CONFIG 8 V1. El VST ya resuelve dinámicamente:

`PRE target ID -> source_effect_id + source_module`

usando `Documents\\GP200\\GP200_PRE_BANK.json`.

Por ello WAH y RVB obtienen nombre y layout de parámetros desde la base normal del VST. El tratamiento especial Time/Sync sigue aplicándose solamente cuando `source_module` es `DLY`.

## Estado de validación

- WAH/Hammy: validación binaria exacta de la receta, falta validar en hardware los otros cinco WAH.
- RVB: receta estática experimental; falta validación física.
- Mantener firmware oficial de recuperación y probar inicialmente con PRE apagado.
