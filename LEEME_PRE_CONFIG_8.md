# PRE CONFIG 8 — prueba experimental

Esta versión acompaña a `GP200_UNIFIED_STEP11_PRE_CONFIG_8_V1.html`.

El HTML usa los 8 huecos PRE duplicados de mayor capacidad: Penesas, AC Boost, Hammy, FAT BB, Super OD, Blues OD, B-Boost y OD 9. Al activar el mod, restaura primero el PRE BANK fijo anterior y después aplica las 8 asignaciones elegidas.

El ZIP de firmware genera `GP200_PRE_BANK.json`. Copiarlo a `Documentos\GP200\GP200_PRE_BANK.json` junto al `GP200_MOD_SYNC.json`. El VST lee ese fichero para resolver nombre, parámetros y presentación DLY por el ID PRE guardado en el preset.

MOD: receta de relocalización validada localmente contra los cuatro packages ya confirmados (Jet, C-Chorus, G-Chorus y S-Phase), con salida byte a byte idéntica al PRE BANK conocido.

DLY: receta general derivada de la relocalización de Pure. Es experimental; Pure tuvo además ajustes específicos de memoria/tiempo en la rama confirmada, por lo que otros delays NO están confirmados en hardware. Probar con PRE/MOD/DLY apagados al seleccionar y conservar firmware oficial de recuperación.
