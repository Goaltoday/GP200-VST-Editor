# GP200 NAM HOT 1.6 + VST 0.1.13 — Factory AMP stock

La carga NAM a Factory AMP stock queda implementada y habilitada en esta versión experimental. Parte del paquete 1.5 adjuntado por el usuario. Mantiene nombres persistentes y recuperación de pesos de 1.4.1, confirmada previamente en pedalera. La ampliación 1.6 está pendiente de confirmación física.

## Qué permite

Puedes enviar un .nam compatible a un Factory AMP que todavía sea stock, sin haber preparado ese destino como NAM en el generador. El firmware conserva su ID y enlaza un descriptor NAM de dos controles al motor compartido. El modelo y el nombre se recuperan al reiniciar.

Debe existir **al menos un NAM incorporado al BIN**: aporta el motor compartido. Los demás destinos pueden seguir stock hasta recibir el modelo por MIDI. Con cero NAM en el generador no se instala el receptor ni el motor NAM.

El nuevo receptor conserva los envíos NH01/NH02 para NAM preparados. El VST nuevo requiere NH03 antes de enviar pesos: si detecta firmware anterior, pide actualizar sin enviar el modelo. Los clientes antiguos no pueden convertir un AMP stock ni reemplazar una conversión dinámica NH03.

## Instalación y prueba

1. Abre GP200_Unified_M7OPT_1.6_180_182.html y elige el perfil correcto, V180 o V182. Usa tu misma base compatible anterior a NAM y conserva tus selecciones de mods. Incorpora al menos un NAM como antes.
2. Genera e instala el BIN nuevo. No reimportes un BIN final con NAM como base del HTML: regenera desde la base anterior, igual que en las versiones previas.
3. Recompila las fuentes completas del VST 0.1.13 incluidas. No se entrega un VST3 Windows compilado.
4. Importa el GP200_MOD_SYNC.json generado con este BIN en el VST, para partir de sus nombres y perfiles correctos. Es necesario disponer de metadatos válidos del firmware.
5. Selecciona un AMP distinto en la pedalera. En el importador elige un Factory AMP que todavía sea stock y un archivo .nam compatible. Ajusta el nombre, hasta 12 caracteres ASCII, y pulsa Import.
6. Espera «flash verified: model and name saved». Selecciona el destino: deben aparecer el nombre nuevo y los controles Input/Output. Para la comparación inicial puedes poner ambos en 50, su valor neutro.
7. Apaga y enciende la pedalera. Comprueba nombre y sonido antes de volver a importar nada. Cambia a otro AMP y regresa al destino.

El bypass del destino no basta: debe estar fuera de la selección AMP durante la transferencia. Se comprueba en la consulta, antes del paquete final en el VST y antes de borrar en el firmware.

El VST guarda el perfil NAM2 en su caché local solo tras el ACK. No vuelvas a importar el JSON inicial después de cada reinicio: ese archivo describe la generación inicial, no las conversiones HOT posteriores. La sincronización automática completa de NAM por MS11 continúa pendiente; otro equipo necesita también los metadatos actualizados o volver a realizar una importación confirmada.

## Capacidad y convivencia

Hay 18 almacenes AMP18 de 8 KiB: uno contiene el receptor y quedan 17 para NAM/SnapTone Factory en conjunto. Cada NAM preparado consume uno; cada nuevo AMP convertido consume otro. La sustitución de un NAM ya asignado reutiliza su almacén, incluso con todos ocupados.

La consulta busca espacio sin escribir. El envío incluye el almacén esperado; si cambia de propietario entre consulta y envío, se rechaza antes de borrar. Un destino ocupado por SnapTone/CLO no se convierte automáticamente. Tampoco se utilizan sus almacenes. Sin espacio libre, la operación termina sin enviar/escribir otro modelo.

## Persistencia y fallos

Los pesos conservan su disposición de 1.4.1. NH03 añade versión 2 de los metadatos con CRC propio para índice, ID y tipo de asignación, además del CRC de pesos y del nombre. El marcador se publica al final.

El arranque guarda los descriptores originales, lee los registros persistentes y enlaza solo conversiones válidas, con ID/motor/destino correctos. Un registro incompleto, con identidad/pesos/nombre corruptos, conserva el AMP stock. El código ejecuta el inicializador nativo previo de arranque y mantiene la restauración SnapTone existente.

Al seleccionar un NAM dinámico, la lectura deja intacto su descriptor en RAM. Un fallo de lectura revierte el registro y ejecuta el cargador/init del AMP stock. Si una sustitución falla y aún se puede verificar el modelo anterior, se conserva ese modelo; de lo contrario se retira la asignación dinámica y el ACK indica el retorno a stock. El VST limpia su perfil NAM2 cuando recibe ese último ACK válido.

La ausencia de ACK sigue significando estado no confirmado: no hay reintento automático ni promesa de conservar el modelo anterior ante pérdida de alimentación. En caso de avería/corrupción posterior a un envío confirmado, la caché local puede requerir actualización; MS11 todavía no descubre estas conversiones por sí mismo.

## Rendimiento

No cambia el kernel NAM ni el procesamiento por bloque. Las conversiones nuevas comparten el cuerpo de un motor existente y la misma región AMP de ejecución; no ejecutan modelos adicionales en paralelo. El receptor ocupa la página ya reservada y los descriptores usan bytes sobrantes de los almacenes, sin un nuevo buffer DSP.

Se añaden trabajo y lecturas al arranque, transferencia e inicialización de modelo. NAM puede consumir más DSP que el AMP stock sustituido. Los tiempos físicos de flash y posibles interrupciones audibles durante la carga requieren medición en pedalera.

## Fuentes y pruebas

El botón «Fuentes y pruebas» del HTML descarga las fuentes completas del firmware y las pruebas. El ZIP principal incluye el VST y los informes de validación. Las pruebas ejecutan el receptor ARM compilado y los cargadores/init/procesadores nativos; la flash física y los extremos USB se emulan. No equivalen a una validación de audio en el equipo.

Para reconstruir: build_unified_profiles.py V180/V182, build_v13.py, nam_hot/build_runtime.py <zig>, build_v14.py, package_v14.py y build_dual_v14.py. Se conservan estos nombres históricos de scripts; producen la versión 1.6.

Las dependencias de prueba son Unicorn, Capstone, pyelftools y Zig/Clang para ARM; @xmldom/xmldom y linkedom para las pruebas JS. test_v14.cjs genera los BIN sintéticos de prueba (0/1/3/5 NAM). Los tests ARM usan la variante de cinco NAM. Nunca flashes esos BIN sintéticos.
