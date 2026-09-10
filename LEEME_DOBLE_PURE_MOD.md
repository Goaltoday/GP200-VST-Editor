# Prueba experimental: Pure simultáneo en MOD y DLY

Esta variante parte directamente de `GP200-VST-Editor-Experimental-firmware (5).zip`.
Conserva sus cambios y añade una sola entrada al catálogo del editor:

```cpp
{0X0B000000u, "MOD"}
```

No modifica el firmware, el protocolo MIDI, el mapa de parámetros ni el
algoritmo de `Pure`. El objetivo es averiguar si el firmware permite dos
instancias del mismo Effect ID en los bloques físicos MOD (7) y DLY (8).

## Compilación

Usar `build_release_hot1_clean.cmd`, que es el procedimiento específico que ya
incluye esta versión. El VST3 resultante aparecerá en la ruta indicada por ese
script.

## Prueba escalonada

1. Usar un preset prescindible y no guardarlo todavía.
2. Apagar MOD y DLY.
3. Seleccionar `Pure` en MOD.
4. Activar solamente MOD y comprobar que la GP-200 sigue respondiendo.
5. Apagar MOD y comprobar `Pure` normalmente en DLY.
6. Con ambos apagados, configurar tiempos muy distintos, por ejemplo:
   - MOD/Pure: Delay 150 ms, Feedback 0, Mix 50.
   - DLY/Pure: Delay 600 ms, Feedback 0, Mix 50.
7. Activar MOD. Si permanece estable, activar después DLY.
8. Tocar una nota corta y escuchar si aparecen dos repeticiones diferenciadas.

## Interpretación

- Dos tiempos y colas distintos: instancias independientes.
- Los dos adoptan el mismo tiempo o cola: estado interno compartido.
- MOD queda en bypass o vuelve a otro efecto: validación de familia en firmware.
- Bloqueo al seleccionar o activar: inicialización no reentrante, colisión de
  buffers o recursos insuficientes. Reiniciar y no guardar el preset.

Esta prueba solo habilita `Pure`. No presupone que otros delays sean seguros.
