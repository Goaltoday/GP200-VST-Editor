# Banco PRE seleccionable — versión experimental

Esta edición parte de `GP200-VST-Editor-Experimental-firmware (7)` y conserva sus cambios anteriores.

## Mapa PRE

| Opción original de PRE | Efecto instalado |
|---|---|
| 14 Boost | Jet |
| Boost | C-Chorus |
| OD9 | G-Chorus |
| P-Boost | S-Phase |
| Yellow OD | Pure (20–500 ms) |

Los cinco IDs siguen perteneciendo al bloque PRE. El editor aplica nombres, descripciones y parámetros por bloque, por lo que las copias originales de DIST no cambian.

`O-Trem` vuelve a ser el efecto original de MOD. La antigua presentación `Pure 2` se ha eliminado. El Pure original del bloque DLY tampoco se modifica.

## Uso

1. Genera el firmware con `GP200_UNIFIED_STEP11_PRE_BANK_V1.html`.
2. Compila/instala este VST.
3. Carga el firmware y elige los nuevos efectos normalmente en la lista PRE.
4. Empieza la prueba con PRE, MOD y DLY apagados; activa únicamente PRE y comprueba cada modelo antes de guardar el preset.

Es firmware experimental: conserva el BIN oficial y el procedimiento de recuperación.
