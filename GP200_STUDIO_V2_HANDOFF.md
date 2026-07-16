# GP200 Studio V2 — Fichero de traspaso entre chats

**Fecha de este estado:** 16 de julio de 2026  
**Código fuente de referencia:** `source_lates_4.zip`  
**SHA-256:** `2f1cb248098422354ae67d9beb74e5542006b0b2203d517cb121f41ec005cacb`

> Este documento debe acompañar siempre a `source_lates_4.zip`.  
> En un chat nuevo, indicar expresamente: **“Toma `source_lates_4.zip` como única fuente de verdad y lee este fichero antes de proponer cambios.”**

---

## 1. Contexto y objetivo del proyecto

Estamos desarrollando **GP200 Studio V2**, un plugin VST3 basado en JUCE para controlar un multiefectos **Valeton GP-200** mediante USB MIDI.

Funciones principales ya integradas:

- Conexión USB MIDI con el GP-200.
- Recepción y edición del preset vivo.
- Cambio de preset y banco.
- Edición de bloques de efectos.
- Guardado y recuperación de presets desde el DAW.
- Importación de presets `.PRST`.
- Importación y selección de IR de usuario.
- Tone Match con captura SOURCE/TARGET y generación de IR.
- Afinador integrado y sincronizado con el afinador hardware.
- Snapshots A/B.
- Controles de volumen, panorama y tempo del patch.
- Tap tempo.
- Interfaz compacta de utilidades.

---

## 2. Regla de trabajo obligatoria

Antes de modificar arquitectura o rehacer una función:

1. Revisar primero `source_lates_4.zip`.
2. Mantener separada la información confirmada de las hipótesis.
3. No sustituir decisiones ya tomadas sin explicar claramente la razón.
4. No regenerar archivos desde versiones antiguas.
5. Aplicar cambios mínimos y localizados.
6. No tocar código no relacionado con la petición.
7. Cuando se entreguen archivos, indicar las rutas exactas que deben sustituirse.
8. No introducir varias alternativas cuando el usuario ya ha elegido una dirección.

---

## 3. Estructura relevante del código

```text
source/
├─ GP200Plugin/
│  ├─ PluginProcessor.h/.cpp
│  ├─ PluginEditor.h/.cpp
│  ├─ EffectBlockComponent.h/.cpp
│  ├─ TunerEngine.h/.cpp
│  ├─ TunerDisplayComponent.h/.cpp
│  └─ ToneMatch/
│     ├─ ToneMatchTypes.h
│     ├─ ToneMatchCapture.h/.cpp
│     ├─ ToneAnalysis.h/.cpp
│     ├─ ToneMatchComparison.h/.cpp
│     ├─ ToneMatchCurveComponent.h/.cpp
│     ├─ ToneMatchPanel.h/.cpp
│     ├─ IToneMatchSolver.h
│     └─ SolverV1.h/.cpp
└─ libgp200/
   ├─ MidiConnection.h/.cpp
   ├─ MidiDeviceScanner.h/.cpp
   ├─ GP200Preset.h/.cpp
   ├─ GP200IR.h/.cpp
   ├─ GP200EffectDatabase.h/.cpp
   └─ GP200EffectParamDatabase.h/.cpp
```

---

# 4. Estado confirmado de la conexión MIDI

## 4.1 `MidiConnection` pertenece al processor

La conexión MIDI ya no pertenece al editor. Se mantiene viva mientras exista la instancia del plugin:

```cpp
gp200::MidiConnection& getMidiConnection() noexcept;
void ensureGP200Connection();
```

En `PluginProcessor.cpp`, `ensureGP200Connection()`:

- conecta solo si todavía no está conectado;
- solicita nombres de asignaciones únicamente en una conexión nueva;
- solicita el preset actual solo si todavía no existe un dump recibido.

Esto evita perder el estado local al cerrar y reabrir la ventana del plugin.

## 4.2 No reconectar desde `PluginEditor`

El editor obtiene una referencia mediante:

```cpp
midiConnection (p.getMidiConnection())
```

No debe volver a llamar directamente a:

```cpp
midiConnection.connectToGP200();
```

La apertura debe usar:

```cpp
processorRef.ensureGP200Connection();
```

## 4.3 Petición inicial robusta

El timer reintenta la petición mientras:

```cpp
midiConnection.isConnected()
&& midiConnection.getCurrentPresetDumpSize() == 0
```

El código actual usa aproximadamente **200 ms** entre reintentos.

---

# 5. Navegación de presets y bancos

## 5.1 Botones existentes

La tarjeta de preset contiene:

```text
BANK -   <   SLOT   NOMBRE   >   BANK +
```

En `PluginEditor.h`:

```cpp
juce::TextButton previousBankButton{"BANK -"};
juce::TextButton previousPresetButton{"<"};
juce::TextButton nextPresetButton{">"};
juce::TextButton nextBankButton{"BANK +"};
```

## 5.2 Comportamiento elegido

No se usan los CC MIDI de BANK del manual para la GUI.

Se reutiliza:

```cpp
midiConnection.sendPresetChange(targetSlot);
```

La navegación por banco suma o resta cuatro slots:

```cpp
void loadPreviousBank() { loadPresetRelative(-4); }
void loadNextBank()     { loadPresetRelative(4);  }
```

Esto conserva la letra actual:

```text
43-A → BANK + → 44-A
43-C → BANK - → 42-C
```

### Motivo de esta decisión

Es determinista y no depende del modo global `Initial/Wait` del GP-200.

Los CC confirmados por el manual existen, pero se reservan para una posible emulación literal de los footswitches:

```text
CC22 BANK - Initial
CC23 BANK + Initial
CC26 BANK - Wait
CC27 BANK + Wait
CC28 confirmación BANK Wait
```

---

# 6. Estado actual de la tarjeta de preset

Archivo:

```text
source/GP200Plugin/PluginEditor.cpp
```

## 6.1 Diseño confirmado

- Se eliminó el texto `CURRENT PRESET`.
- Slot completamente blanco.
- Nombre editable mediante `TextEditor`.
- No existe línea vertical entre slot y nombre.
- Sí permanece la línea horizontal entre preset vivo y snapshots.
- La tarjeta mide actualmente:

```cpp
const juce::Rectangle<int> currentPresetBox
{
    18,
    52,
    400,
    124
};
```

## 6.2 Slot

El slot se dibuja manualmente:

```cpp
g.setFont (juce::Font (24.0f, juce::Font::bold));
g.setColour (textColour);

g.drawText (
    slotText,
    currentPresetBox.getX() + 104,
    currentPresetBox.getY() + 18,
    56,
    42,
    juce::Justification::centred
);
```

## 6.3 Nombre del preset

Configuración actual:

```cpp
auto presetFont = juce::Font (16.0f, juce::Font::bold);
presetFont.setHorizontalScale (0.88f);
presetNameEditor.setFont (presetFont);
```

Bounds actuales:

```cpp
presetNameEditor.setBounds (176, 70, 132, 42);
```

## 6.4 Cómo ajustar tipografía

Slot:

```cpp
auto slotFont = juce::Font (24.0f, juce::Font::bold);
slotFont.setHorizontalScale (0.92f);
g.setFont (slotFont);
```

Nombre:

```cpp
auto presetFont = juce::Font (16.0f, juce::Font::bold);
presetFont.setHorizontalScale (0.88f);
presetNameEditor.setFont (presetFont);
```

Referencias:

```text
1.00 = ancho normal
0.95 = ligeramente estrecha
0.90 = estrecha
0.85 = bastante comprimida
```

No introducir una fuente externa sin una decisión explícita de distribución/licencia.

---

# 7. Afinador

## 7.1 Estado funcional

El afinador del plugin es preciso y está integrado con el comando hardware del GP-200.

Al activarlo:

- aparece `tunerDisplay`;
- se ocultan completamente:
  - `userIRSlotBox`;
  - `importIRButton`;
  - `toneMatchButton`;
- el display se lleva al frente.

Código relevante en `toggleTuner()`:

```cpp
const bool showIRControls = !tunerIsOn;

userIRSlotBox.setVisible(showIRControls);
importIRButton.setVisible(showIRControls);
toneMatchButton.setVisible(showIRControls);

if (tunerIsOn)
    tunerDisplay.toFront(false);
```

Bounds actuales:

```cpp
tunerDisplay.setBounds (
    418,
    189,
    getWidth() - 448,
    32);
```

No volver a dejar visible el fragmento de `User IR` bajo el afinador.

---

# 8. Tone Match — decisiones confirmadas

## 8.1 Objetivo

Generar una IR que represente literalmente:

```text
RAW = TARGET − SOURCE
```

## 8.2 Decisiones descartadas

No reintroducir sin aprobación explícita:

- suavizado global de la RAW;
- `matchAmount` inferior al 100 %;
- normalización a 0 dB;
- calibración automática del nivel global;
- límites arbitrarios de corte o realce;
- ponderación de la RAW por confianza;
- fade automático a 0 dB;
- segundo pase o refinamiento residual;
- compensaciones específicas del GP-200.

## 8.3 Análisis espectral

`ToneAnalysis.h` usa:

```cpp
int fftOrder{14};
```

Por tanto:

```text
FFT = 16384 muestras
```

Opciones actuales confirmadas:

```cpp
double overlapRatio{0.75};
double silenceThresholdDb{-55.0};
float clippingThreshold{0.999f};
double minimumFrequencyHz{30.0};
double maximumFrequencyHz{20000.0};
int robustGroupCount{11};
double confidenceReferenceDeviationDb{3.0};
```

El análisis usa Welch robusto / median-of-means.

## 8.4 Comparación canónica única

`ToneMatchComparison` es ahora la única fuente de la RAW.

Rango actual:

```cpp
double minimumFrequencyHz{40.0};
double maximumFrequencyHz{18000.0};
int outputPointCount{512};
```

El resultado contiene:

```cpp
frequencyHz
rawCorrectionDb
levelAlignedCorrectionDb
confidence
```

Aunque existen campos antiguos de nivel alineado, la decisión funcional actual es RAW literal.

## 8.5 Solver

`SolverV1` ya no recibe SOURCE y TARGET. Recibe:

```cpp
const ToneMatchComparisonResult& comparison
```

y copia directamente:

```cpp
result.frequencyHz = comparison.frequencyHz;
result.rawCorrectionDb = comparison.rawCorrectionDb;
result.smoothedCorrectionDb = comparison.rawCorrectionDb;
result.confidence = comparison.confidence;
```

No vuelve a calcular `TARGET - SOURCE`.

## 8.6 Síntesis de IR

Estado confirmado:

- sample rate: 44,1 kHz;
- longitud: 1024 muestras;
- minimum-phase;
- sin smoothing;
- sin límites;
- fuera de la curva RAW mantiene el valor del extremo;
- no se añadió el filtro pasa-bajos discutido posteriormente.

El filtrado HPF/LPF se deja al cargador de IR del GP-200.

## 8.7 Ganancia práctica de salida

En `SolverV1.cpp`:

```cpp
constexpr double outputGainDb = 12.0;
```

La IR recibe +12 dB de ganancia global práctica.

Esto no cambia la forma tonal, pero puede acercar el nivel a IR comerciales.

## 8.8 Cálculo del error

Para que los +12 dB no aparezcan como error tonal:

```cpp
const auto achievedForErrorDb =
    achievedDb - outputGainDb;

const auto residual =
    raw - achievedForErrorDb;
```

No eliminar esta corrección mientras exista el `outputGainDb`.

## 8.9 Riesgo conocido

La ganancia fija de +12 dB puede crear picos superiores a 0 dBFS en alguna IR.

El solver avisa:

```text
Generated IR peak is above 0 dBFS; export as 32-bit float
```

No se implementó todavía un limitador automático de pico porque alteraría el nivel elegido.

---

# 9. Tone Match — interfaz actual

Archivo:

```text
source/GP200Plugin/ToneMatch/ToneMatchPanel.cpp
```

La zona de capturas fue compactada para ampliar la gráfica:

```cpp
SOURCE: y=82,  alto=78
TARGET: y=170, alto=78
graphTop = 288
graphBottom = getHeight() - 72
```

La gráfica gana espacio vertical respecto a la versión anterior.

No volver a usar cajas de captura de 118 px salvo que se rediseñe toda la ventana.

---

# 10. Capturas recomendadas para Tone Match

Decisiones prácticas:

- misma DI reamplificada en SOURCE y TARGET;
- idealmente el mismo tramo exacto;
- duración recomendada: 20–30 segundos;
- unos 25 segundos es una buena referencia;
- evitar silencios largos;
- incluir acordes, palm mutes, notas individuales y distintas dinámicas.

Se eligió DI de guitarra representativa frente a ruido blanco/rosa para cadenas no lineales.

---

# 11. Filtros de la IR

Se estudió hacer que la IR filtrara completamente por encima del máximo de comparación.

Decisión final:

- **no incorporar ese pasa-bajos en el WAV**;
- usar los filtros HPF/LPF disponibles en el cargador IR del GP-200;
- mantener Tone Match centrado en reproducir la RAW.

No añadir una caída a `-120 dB` sin nueva aprobación.

---

# 12. Snapshots A/B

La tarjeta contiene botones A/B y muestra el preset guardado de comparación.

La línea inferior sigue separada mediante:

```cpp
g.drawHorizontalLine(...)
```

La eliminación solicitada fue únicamente la línea vertical entre slot y nombre.

---

# 13. Estado de botones y textos

- Los controles de banco muestran `BANK -` y `BANK +`.
- Las flechas muestran `<` y `>`.
- El botón antes llamado `FX OFF` fue renombrado visualmente hacia `BYPASS` según el ajuste del usuario.
- El estado visual puede indicarse por color sin necesidad de alternar textos largos.

Antes de tocar nombres de miembros, buscar el texto real en `source_lates_4.zip`; no asumir nombres antiguos.

---

# 14. Commits/hitos alcanzados

Mensajes de commit recomendados y usados durante el trabajo:

```text
Preserve live GP-200 state across editor reopen
Improve Tone Match analysis and layout
Enlarge Tone Match comparison graph
Add preset bank navigation controls
```

Los cambios posteriores de estilo de la tarjeta del preset pueden agruparse como:

```text
Refine preset navigation panel layout
```

Comando recomendado:

```bash
git add source/GP200Plugin/PluginEditor.cpp
git add source/GP200Plugin/PluginEditor.h
git commit -m "Refine preset navigation panel layout"
```

Comprobar siempre:

```bash
git status
```

---

# 15. Funcionalidades que no deben romperse

Cada cambio nuevo debe preservar:

- reconexión MIDI robusta;
- estado vivo al cerrar/reabrir GUI;
- recepción de cambios físicos del GP-200;
- navegación preset anterior/siguiente;
- navegación banco anterior/siguiente;
- cambio de banco conservando A/B/C/D;
- snapshots A/B;
- edición del nombre;
- Save to DAW;
- Recall from DAW;
- Store to GP-200;
- Import PRST;
- User IR;
- Import IR;
- Tone Match;
- afinador;
- ocultación de controles IR durante afinación;
- Tap Tempo;
- edición de volumen/pan/BPM;
- bloques de efectos;
- RAW única en Tone Match;
- FFT 16384;
- IR 44,1 kHz / 1024 muestras / minimum-phase;
- ganancia +12 dB;
- error descontando +12 dB.

---

# 16. Cambios descartados o abandonados

No reabrir estas líneas sin una petición expresa:

- importación múltiple de presets: prueba abandonada porque solo cargaba uno y luego el botón no reaccionaba;
- segundo pase de Tone Match;
- suavizado fuerte;
- normalización de la IR;
- límites de ±dB;
- filtro pasa-bajos embebido en la IR;
- dependencia de CC BANK Initial/Wait para los botones de GUI.

---

# 17. Hipótesis y trabajo pendiente

Estas ideas no están implementadas y deben tratarse como hipótesis:

- mejorar diagnósticos de confianza del Tone Match sin alterar la RAW;
- medir por separado error absoluto y error de forma;
- evaluar un control de pico seguro para la ganancia +12 dB;
- auditar y limpiar campos antiguos de `levelAlignedCorrectionDb`;
- continuar la ingeniería inversa de carga NAM del editor oficial;
- revisar licencias y atribuciones antes de publicación pública;
- futuras mejoras estéticas globales del GUI;
- posible refinamiento del estilo tipográfico y espacios de la tarjeta de preset.

---

# 18. Archivos y fuentes auxiliares del proyecto

Además del código, el proyecto dispone de:

```text
gp200editor-master (1).zip
Valeton(1).zip
1768360642746.GP-200_Online Manual_EN_Firmware V1.8.0 (1).pdf
downloadfile-21.pdf
source_lates_4.zip
```

Usos:

- `source_lates_4.zip`: código actual y fuente de verdad.
- manual GP-200 firmware 1.8.0: comandos, funciones y MIDI documentado.
- `Valeton(1).zip`: editor oficial para análisis de binarios/procesos.
- `gp200editor-master (1).zip`: proyecto original de referencia.
- `downloadfile-21.pdf`: documentación técnica adicional.

---

# 19. Protocolo para comenzar un chat nuevo

Mensaje recomendado para pegar:

> Estamos desarrollando GP200 Studio V2, un plugin VST3 JUCE para controlar el Valeton GP-200 por USB MIDI. Usa `source_lates_4.zip` como única fuente de verdad y lee `GP200_STUDIO_V2_HANDOFF.md` antes de responder. Mantén separada la información confirmada de las hipótesis. No propongas cambios de arquitectura sin revisar primero las decisiones del fichero. Preserva todas las funcionalidades enumeradas y aplica cambios mínimos.

Después indicar una única tarea concreta.

---

# 20. Verificación de este paquete

Este documento fue generado tras inspeccionar el contenido real de:

```text
source_lates_4.zip
```

Se confirmó dentro del archivo:

- `ToneAnalysis::Options::fftOrder = 14`;
- solver usando `ToneMatchComparisonResult`;
- +12 dB en `SolverV1`;
- error descontando +12 dB;
- botones `BANK -` / `BANK +`;
- fuente del nombre a 16 px, bold, scale 0.88;
- slot blanco de 24 px;
- eliminación de línea vertical entre slot y nombre;
- afinador ocultando controles IR;
- Tone Match Panel compacto.

**No se ha compilado el proyecto durante la creación de este fichero de traspaso.**  
El código se ha inspeccionado, pero la compilación y prueba funcional final deben realizarse en el entorno de desarrollo del usuario.
