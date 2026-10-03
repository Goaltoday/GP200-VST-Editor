# VST FIX12 CHAIN BLEND correcciones

Aplicar sobre la base GP200-VST-Editor-Experimental-firmware (2)(4), o sobre esa misma base con la superposición FIX11 CHAIN BLEND. Este FIX12 no identifica el FIX12 SEND/RETURN histórico del proyecto. Copiar source y tests respetando rutas y recompilar con JUCE/CMake o el workflow habitual. Se entregan fuentes, no un VST3 compilado.

Cambios solicitados:
- BLEND está en la parte superior derecha del panel de routing. Se reserva una franja de 28 píxeles para que no se solape con SPLIT, ramas o bloques; los marcadores y las zonas de arrastre siguen alineados. Se mantiene el slider por encima del fondo del panel.
- CHAIN pasa a la posición anterior de Import IR y viceversa.
- Se elimina NEW BLEND del GUI. La primera edición del slider inicializa la firma y aplica la posición que el usuario eligió. Cargar o dibujar un preset no lo convierte. Si VOL era el blend antiguo, al editar el slider se devuelve VOL a 100; no cambia su asignación EXP. No se hace STORE automático.
- Se mantiene el último gesto del slider mientras se confirma la escritura. Se permite seguir arrastrando durante la lectura; los valores siguientes se envían tras terminar la confirmación anterior. Cambiar de preset, modo, routing o iniciar Recall cancela el gesto pendiente.
- Al desactivar un preset cargado directamente en CHAIN, sin información del modo nativo anterior, el retorno por defecto es Parallel (0). La opción anterior Series (1) podía enviar la señal al FX Loop externo sin retorno conectado. Si el modo nativo anterior se observó en la misma sesión, se sigue recuperando ese modo.

Requiere firmware FIX34: FIX33 enganchaba el setter de parámetros nativo 0x7174/0x778C, pero el mensaje MIDI del VST recorre otro handler y valida la metadata de VOL. FIX34 intercepta esos dos campos en la recepción MIDI real antes de esa validación. La notificación de vuelta y el dump live siguen confirmando el estado real; una modificación de caché local no basta.

Save/Recall conserva la firma y posición independientes y borra la firma al restaurar un snapshot antiguo. Para persistir en el pedal, esperar la confirmación y usar STORE. La curva de mezcla y los efectos no cambian: en el centro ambas ramas tienen ganancia 1, no potencia constante.

Validación local: 371 comprobaciones de CHAIN/BLEND con cuerpos reales de transporte, restore y sincronización GUI; regresiones de conexión, snapshots A/B y DAW, MIDI, PRE y drag. Puertos, codec y componentes JUCE son dobles de prueba. Falta compilación completa con JUCE y prueba del conjunto en GP200 y host DAW.
