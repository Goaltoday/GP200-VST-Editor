/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed under GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../libgp200/GP200Constants.h"
#include "../libgp200/GP200EffectDatabase.h"
#include "../libgp200/GP200EffectParamDatabase.h"


namespace
{
gp200::GP200Preset makeDefaultOfflinePreset()
{
    gp200::GP200Preset preset;
    preset.isValid = true;
    preset.patchName = "Offline preset";
    preset.fxLoopSend = 4;
    preset.fxLoopReturn = 4;

    static constexpr const char* modules[] = {
        "PRE", "WAH", "DST", "AMP", "NR", "CAB",
        "EQ", "MOD", "DLY", "RVB", "VOL"
    };

    static constexpr int defaultRoutingOrder[] = {
        10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
    };

    for (int blockIndex = 0; blockIndex < static_cast<int> (gp200::effectBlockCount); ++blockIndex)
    {
        const auto index = static_cast<std::size_t> (blockIndex);
        preset.routingOrder[index] = defaultRoutingOrder[blockIndex];

        auto& slot = preset.effects[index];
        slot.blockIndex = blockIndex;
        slot.slotIndex = blockIndex;
        slot.enabled = false;
        slot.params.fill (0.0f);

        const auto effects = gp200::GP200EffectDatabase::getEffectsForModule (modules[blockIndex]);
        if (!effects.empty ())
            slot.effectId = effects.front ().effectId;

        if (const auto* paramSet = gp200::GP200EffectParamDatabase::findParamsForEffect (slot.effectId))
        {
            for (int i = 0; i < paramSet->count; ++i)
            {
                const auto& param = paramSet->params[i];
                if (param.idx >= 0 && param.idx < static_cast<int> (slot.params.size ()))
                    slot.params[static_cast<std::size_t> (param.idx)] = param.defaultValue;
            }
        }
    }

    return preset;
}

constexpr juce::uint32 offlineSnapshotMagic = 0x4f503247u;
constexpr int offlineSnapshotVersion = 2;

juce::MemoryBlock serialiseOfflineState (const gp200::GP200Preset& preset,
                                         int patchVolume,
                                         int patchPan,
                                         int patchTempo)
{
    juce::MemoryBlock result;
    juce::MemoryOutputStream out (result, false);
    out.writeInt (static_cast<int> (offlineSnapshotMagic));
    out.writeInt (offlineSnapshotVersion);
    out.writeString (preset.patchName);
    out.writeString (preset.author);
    out.writeInt (static_cast<int> (preset.prstRawSource.getSize ()));
    if (preset.prstRawSource.getSize () > 0)
        out.write (preset.prstRawSource.getData (), preset.prstRawSource.getSize ());
    out.writeInt (preset.fxLoopSend);
    out.writeInt (preset.fxLoopReturn);
    out.writeInt (patchVolume);
    out.writeInt (patchPan);
    out.writeInt (patchTempo);
    for (const auto value : preset.routingOrder)
        out.writeInt (value);
    for (const auto& effect : preset.effects)
    {
        out.writeInt (effect.blockIndex);
        out.writeInt (effect.slotIndex);
        out.writeBool (effect.enabled);
        out.writeInt (static_cast<int> (effect.effectId));
        for (const auto value : effect.params)
            out.writeFloat (value);
    }
    return result;
}

bool deserialiseOfflineState (const juce::MemoryBlock& data,
                              gp200::GP200Preset& preset,
                              int& patchVolume,
                              int& patchPan,
                              int& patchTempo)
{
    if (data.getSize () < 8)
        return false;

    juce::MemoryInputStream in (data, false);
    if (static_cast<juce::uint32> (in.readInt ()) != offlineSnapshotMagic)
        return false;

    const auto version = in.readInt ();
    if (version != 1 && version != offlineSnapshotVersion)
        return false;

    gp200::GP200Preset decoded;
    decoded.isValid = true;
    decoded.patchName = in.readString ();
    decoded.author = in.readString ();

    if (version >= 2)
    {
        const auto rawSize = in.readInt ();
        if (rawSize < 0 || static_cast<juce::int64> (rawSize) > in.getNumBytesRemaining ())
            return false;
        if (rawSize > 0)
        {
            decoded.prstRawSource.setSize (static_cast<std::size_t> (rawSize), false);
            if (in.read (decoded.prstRawSource.getData (), rawSize) != rawSize)
                return false;
        }
    }

    decoded.fxLoopSend = in.readInt ();
    decoded.fxLoopReturn = in.readInt ();
    patchVolume = juce::jlimit (0, 100, in.readInt ());
    patchPan = juce::jlimit (-100, 100, in.readInt ());
    patchTempo = juce::jlimit (40, 250, in.readInt ());

    for (auto& value : decoded.routingOrder)
        value = in.readInt ();
    for (auto& effect : decoded.effects)
    {
        effect.blockIndex = in.readInt ();
        effect.slotIndex = in.readInt ();
        effect.enabled = in.readBool ();
        effect.effectId = static_cast<juce::uint32> (in.readInt ());
        for (auto& value : effect.params)
            value = in.readFloat ();
    }

    if (decoded.patchName.isEmpty ())
        decoded.patchName = "Offline preset";

    preset = std::move (decoded);
    return true;
}
}

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor ()
    : AudioProcessor (BusesProperties ()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                          .withInput ("Input", juce::AudioChannelSet::stereo (), true)
#endif
                          .withOutput ("Output", juce::AudioChannelSet::stereo (), true)
#endif
      ),
      offlinePreset (makeDefaultOfflinePreset ())
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor ()
{
    midiConnection.disconnect();
}

gp200::MidiConnection&
AudioPluginAudioProcessor::getMidiConnection() noexcept
{
    return midiConnection;
}

gp200::GP200Preset& AudioPluginAudioProcessor::getOfflinePreset() noexcept
{
    return offlinePreset;
}

std::uint64_t& AudioPluginAudioProcessor::getOfflinePresetRevision() noexcept
{
    return offlinePresetRevision;
}

bool& AudioPluginAudioProcessor::getOfflinePresetDirty() noexcept
{
    return offlinePresetDirty;
}

int& AudioPluginAudioProcessor::getOfflinePatchVolume() noexcept
{
    return offlinePatchVolume;
}

int& AudioPluginAudioProcessor::getOfflinePatchPan() noexcept
{
    return offlinePatchPan;
}

int& AudioPluginAudioProcessor::getOfflinePatchTempo() noexcept
{
    return offlinePatchTempo;
}

void AudioPluginAudioProcessor::notifyOfflineStateChanged()
{
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails {}
                           .withNonParameterStateChanged (true));
}

void AudioPluginAudioProcessor::ensureGP200Connection()
{
    const auto wasAlreadyConnected =
        midiConnection.isConnected();

    if (!wasAlreadyConnected)
    {
        if (!midiConnection.connectToGP200())
            return;

        midiConnection.requestAssignmentNamesFromGP200();
    }

    // Solo solicita el preset cuando todavía no existe uno en memoria.
    // Al reabrir la ventana, los cambios locales se conservan porque el
    // dump ya existe y no se reemplaza.
    if (midiConnection.getCurrentPresetDumpSize() == 0)
        midiConnection.requestCurrentPresetFromGP200();
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName () const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi () const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi () const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect () const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds () const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms ()
{
    return 1;
}

int AudioPluginAudioProcessor::getCurrentProgram ()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (
    double sampleRate,
    int samplesPerBlock)
{
    tunerEngine.prepare (sampleRate);

    toneMatchCapture.prepare (
        sampleRate,
        samplesPerBlock,
        getTotalNumInputChannels());
}

void AudioPluginAudioProcessor::releaseResources ()
{
    toneMatchCapture.releaseResources();
}

void AudioPluginAudioProcessor::setTunerEnabled (
    bool shouldBeEnabled) noexcept
{
    tunerEngine.setEnabled (shouldBeEnabled);
}

bool AudioPluginAudioProcessor::isTunerEnabled () const noexcept
{
    return tunerEngine.isEnabled ();
}

TunerResult AudioPluginAudioProcessor::getTunerResult () const noexcept
{
    return tunerEngine.getResult ();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet () != juce::AudioChannelSet::mono () &&
        layouts.getMainOutputChannelSet () != juce::AudioChannelSet::stereo ())
        return false;

#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet () != layouts.getMainInputChannelSet ())
        return false;
#endif

    return true;
#endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
	
	if (toneMatchCapture.isCapturing())
    toneMatchCapture.pushAudioBlock (buffer);

    tunerEngine.process (buffer);

    const auto totalNumInputChannels = getTotalNumInputChannels ();
    const auto totalNumOutputChannels = getTotalNumOutputChannels ();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples ());
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor () const
{
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor ()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
bool AudioPluginAudioProcessor::isUsefulPresetName (const juce::String& presetName)
{
    const auto name = presetName.trim ();

    return name.isNotEmpty () && name != "unknown" && name != "requesting...";
}

bool AudioPluginAudioProcessor::isValidSnapshotIndex (int snapshotIndex)
{
    return snapshotIndex >= 0 &&
           snapshotIndex < compareSnapshotCount;
}

//==============================================================================
void AudioPluginAudioProcessor::setGP200SessionState (int slot, const juce::String& presetName)
{
    setGP200SlotReferenceState (slot, presetName);
}

void AudioPluginAudioProcessor::setGP200SessionState (int slot,
                                                      const juce::String& presetName,
                                                      const juce::MemoryBlock& presetData)
{
    setGP200SlotReferenceState (slot, presetName);

    if (presetData.getSize () > 0)
        setGP200PresetSnapshotState (0, slot, presetName, presetData);
}

void AudioPluginAudioProcessor::setGP200SlotReferenceState (int slot, const juce::String& presetName)
{
    const juce::ScopedLock lock (stateLock);

    savedGP200Slot = slot;

    if (isUsefulPresetName (presetName))
        savedGP200PresetName = presetName.trim ();
    else
        savedGP200PresetName = "unknown";
}

void AudioPluginAudioProcessor::setGP200PresetSnapshotState (
    int snapshotIndex,
    int slot,
    const juce::String& presetName,
    const juce::MemoryBlock& presetData)
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return;

    const juce::ScopedLock lock (stateLock);

    auto& snapshot =
        savedGP200PresetSnapshots[static_cast<std::size_t> (snapshotIndex)];

    snapshot.slot = slot;

    if (isUsefulPresetName (presetName))
        snapshot.name = presetName.trim ();
    else
        snapshot.name = "unknown";

    snapshot.data = presetData;
    ++snapshot.revision;
}

int AudioPluginAudioProcessor::getSavedGP200Slot () const
{
    const juce::ScopedLock lock (stateLock);

    return savedGP200Slot;
}

juce::String AudioPluginAudioProcessor::getSavedGP200PresetName () const
{
    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetName;
}

juce::String AudioPluginAudioProcessor::getSavedGP200SlotText () const
{
    const juce::ScopedLock lock (stateLock);

    if (savedGP200Slot < 0)
        return "Saved slot in DAW: unknown";

    return "Saved slot in DAW: " + formatGP200Slot (savedGP200Slot);
}

int AudioPluginAudioProcessor::getSavedGP200PresetSnapshotSlot (
    int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return -1;

    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetSnapshots[
        static_cast<std::size_t> (snapshotIndex)].slot;
}

juce::String AudioPluginAudioProcessor::getSavedGP200PresetSnapshotName (
    int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return "unknown";

    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetSnapshots[
        static_cast<std::size_t> (snapshotIndex)].name;
}

juce::String AudioPluginAudioProcessor::getSavedGP200PresetSnapshotSlotText (
    int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return "Saved preset in DAW: unknown";

    const juce::ScopedLock lock (stateLock);

    const auto& snapshot =
        savedGP200PresetSnapshots[static_cast<std::size_t> (snapshotIndex)];

    if (snapshot.slot < 0)
        return "Saved preset in DAW: unknown";

    return "Saved preset in DAW: " + formatGP200Slot (snapshot.slot);
}

int AudioPluginAudioProcessor::getSavedGP200PresetDataSize (
    int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return 0;

    const juce::ScopedLock lock (stateLock);

    return static_cast<int> (
        savedGP200PresetSnapshots[
            static_cast<std::size_t> (snapshotIndex)].data.getSize ());
}

bool AudioPluginAudioProcessor::hasSavedGP200PresetData (
    int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return false;

    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetSnapshots[
        static_cast<std::size_t> (snapshotIndex)].data.getSize () > 0;
}

juce::MemoryBlock AudioPluginAudioProcessor::getSavedGP200PresetDataCopy (
    int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return {};

    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetSnapshots[
        static_cast<std::size_t> (snapshotIndex)].data;
}


juce::String AudioPluginAudioProcessor::getSavedGP200PresetDataStatusText (
    int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return "Saved preset in DAW: invalid snapshot";

    const juce::ScopedLock lock (stateLock);

    const auto& snapshot =
        savedGP200PresetSnapshots[static_cast<std::size_t> (snapshotIndex)];

    if (snapshot.data.getSize () == 0)
        return "Saved preset in DAW: not saved";

    return "Saved preset in DAW: " +
           formatGP200Slot (snapshot.slot) + "  " +
           snapshot.name + "  " +
           juce::String (
               static_cast<int> (snapshot.data.getSize ())) +
           " bytes";
}

std::uint64_t AudioPluginAudioProcessor::getSavedPresetRevision (
    int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return 0;

    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetSnapshots[
        static_cast<std::size_t> (snapshotIndex)].revision;
}

juce::String AudioPluginAudioProcessor::formatGP200Slot (int slot)
{
    if (slot < 0)
        return "unknown";

    const int bank = slot / gp200::presetsPerBank + 1;
    const int slotInBank = slot % gp200::presetsPerBank;

    const juce::String slotLetter = juce::String ("ABCD").substring (slotInBank, slotInBank + 1);

    return juce::String (slot) + "  (" + juce::String (bank).paddedLeft ('0', 2) + "-" + slotLetter + ")";
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (
    juce::MemoryBlock& destData)
{
    auto xml =
        std::make_unique<juce::XmlElement> ("GP200StudioState");

    {
        const juce::ScopedLock lock (stateLock);

        xml->setAttribute ("version", 6);

        xml->setAttribute ("slotReferenceSlot", savedGP200Slot);
        xml->setAttribute ("slotReferenceName",
                           savedGP200PresetName);

        const auto& snapshotA = savedGP200PresetSnapshots[0];
        const auto& snapshotB = savedGP200PresetSnapshots[1];

        xml->setAttribute ("snapshotASlot", snapshotA.slot);
        xml->setAttribute ("snapshotAName", snapshotA.name);
        xml->setAttribute (
            "snapshotADataBase64",
            snapshotA.data.toBase64Encoding ());

        xml->setAttribute ("snapshotBSlot", snapshotB.slot);
        xml->setAttribute ("snapshotBName", snapshotB.name);
        xml->setAttribute (
            "snapshotBDataBase64",
            snapshotB.data.toBase64Encoding ());

        const auto offlineData = serialiseOfflineState (
            offlinePreset,
            offlinePatchVolume,
            offlinePatchPan,
            offlinePatchTempo);
        xml->setAttribute ("offlinePresetDataBase64",
                           offlineData.toBase64Encoding ());
        xml->setAttribute ("offlinePresetDirty", offlinePresetDirty);
    }

    copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (
    const void* data,
    int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    if (!xml->hasTagName ("GP200StudioState"))
        return;

    const juce::ScopedLock lock (stateLock);

    savedGP200Slot = xml->getIntAttribute (
        "slotReferenceSlot",
        xml->getIntAttribute ("gp200Slot", -1));

    savedGP200PresetName = xml->getStringAttribute (
        "slotReferenceName",
        xml->getStringAttribute (
            "gp200PresetName",
            "unknown"));

    for (auto& snapshot : savedGP200PresetSnapshots)
    {
        snapshot.slot = -1;
        snapshot.name = "unknown";
        snapshot.data.setSize (0);
        ++snapshot.revision;
    }

    const auto offlineDataBase64 =
        xml->getStringAttribute ("offlinePresetDataBase64", {});

    if (offlineDataBase64.isNotEmpty ())
    {
        juce::MemoryBlock offlineData;
        if (offlineData.fromBase64Encoding (offlineDataBase64))
        {
            gp200::GP200Preset restoredPreset;
            int restoredVolume = 50;
            int restoredPan = 0;
            int restoredTempo = 120;

            if (deserialiseOfflineState (offlineData,
                                         restoredPreset,
                                         restoredVolume,
                                         restoredPan,
                                         restoredTempo))
            {
                offlinePreset = std::move (restoredPreset);
                offlinePatchVolume = restoredVolume;
                offlinePatchPan = restoredPan;
                offlinePatchTempo = restoredTempo;
                offlinePresetDirty = xml->getBoolAttribute ("offlinePresetDirty", false);
                ++offlinePresetRevision;
            }
        }
    }

    const auto hasNewSnapshotFormat =
        xml->hasAttribute ("snapshotASlot") ||
        xml->hasAttribute ("snapshotAName") ||
        xml->hasAttribute ("snapshotADataBase64") ||
        xml->hasAttribute ("snapshotBSlot") ||
        xml->hasAttribute ("snapshotBName") ||
        xml->hasAttribute ("snapshotBDataBase64");

    if (hasNewSnapshotFormat)
    {
        auto& snapshotA = savedGP200PresetSnapshots[0];
        auto& snapshotB = savedGP200PresetSnapshots[1];

        snapshotA.slot =
            xml->getIntAttribute ("snapshotASlot", -1);

        snapshotA.name =
            xml->getStringAttribute (
                "snapshotAName",
                "unknown");

        const auto snapshotADataBase64 =
            xml->getStringAttribute (
                "snapshotADataBase64",
                {});

        if (snapshotADataBase64.isNotEmpty ())
        {
            snapshotA.data.fromBase64Encoding (
                snapshotADataBase64);
        }

        snapshotB.slot =
            xml->getIntAttribute ("snapshotBSlot", -1);

        snapshotB.name =
            xml->getStringAttribute (
                "snapshotBName",
                "unknown");

        const auto snapshotBDataBase64 =
            xml->getStringAttribute (
                "snapshotBDataBase64",
                {});

        if (snapshotBDataBase64.isNotEmpty ())
        {
            snapshotB.data.fromBase64Encoding (
                snapshotBDataBase64);
        }

        return;
    }

    // Compatibilidad con proyectos que guardaban un único snapshot.
    auto& snapshotA = savedGP200PresetSnapshots[0];

    snapshotA.slot = xml->getIntAttribute (
        "presetSnapshotSlot",
        xml->getIntAttribute ("gp200Slot", -1));

    snapshotA.name = xml->getStringAttribute (
        "presetSnapshotName",
        xml->getStringAttribute (
            "gp200PresetName",
            "unknown"));

    auto oldPresetDataBase64 =
        xml->getStringAttribute (
            "presetSnapshotDataBase64",
            {});

    if (oldPresetDataBase64.isEmpty ())
    {
        oldPresetDataBase64 =
            xml->getStringAttribute (
                "gp200PresetDataBase64",
                {});
    }

    if (oldPresetDataBase64.isNotEmpty ())
    {
        snapshotA.data.fromBase64Encoding (
            oldPresetDataBase64);
    }
}

bool AudioPluginAudioProcessor::startToneMatchCapture (
    tonematch::CaptureRole role)
{
    return toneMatchCapture.start (role);
}

tonematch::ToneCaptureData
AudioPluginAudioProcessor::stopToneMatchCapture()
{
    auto capture = toneMatchCapture.stop();

    if (capture.isValid())
        storeToneMatchCapture (capture);

    return capture;
}

void AudioPluginAudioProcessor::clearToneMatchCapture()
{
    toneMatchCapture.clear();
}

bool AudioPluginAudioProcessor::isToneMatchCapturing() const noexcept
{
    return toneMatchCapture.isCapturing();
}

tonematch::CaptureState
AudioPluginAudioProcessor::getToneMatchCaptureState() const noexcept
{
    return toneMatchCapture.getState();
}

tonematch::CaptureRole
AudioPluginAudioProcessor::getToneMatchCaptureRole() const noexcept
{
    return toneMatchCapture.getCurrentRole();
}


double
AudioPluginAudioProcessor::getToneMatchCapturedDurationSeconds() const noexcept
{
    return toneMatchCapture.getCapturedDurationSeconds();
}

float
AudioPluginAudioProcessor::getToneMatchCapturePeakLinear() const noexcept
{
    return toneMatchCapture.getCurrentPeakLinear();
}

void AudioPluginAudioProcessor::storeToneMatchCapture (
    tonematch::ToneCaptureData capture)
{
    const juce::ScopedLock lock (toneMatchDataLock);

    if (capture.role == tonematch::CaptureRole::source)
    {
        sourceToneCapture = std::move (capture);
        sourceToneProfile = {};
    }
    else
    {
        targetToneCapture = std::move (capture);
        targetToneProfile = {};
    }
	toneMatchComparison = {};
    toneMatchResult = {};
	
}

bool AudioPluginAudioProcessor::hasToneMatchCapture (
    tonematch::CaptureRole role) const
{
    const juce::ScopedLock lock (toneMatchDataLock);

    return role == tonematch::CaptureRole::source
        ? sourceToneCapture.isValid()
        : targetToneCapture.isValid();
}

tonematch::ToneCaptureData
AudioPluginAudioProcessor::getToneMatchCaptureCopy (
    tonematch::CaptureRole role) const
{
    const juce::ScopedLock lock (toneMatchDataLock);

    return role == tonematch::CaptureRole::source
        ? sourceToneCapture
        : targetToneCapture;
}

void AudioPluginAudioProcessor::clearToneMatchCapture (
    tonematch::CaptureRole role)
{
    const juce::ScopedLock lock (toneMatchDataLock);

    if (role == tonematch::CaptureRole::source)
    {
        sourceToneCapture = {};
        sourceToneProfile = {};
    }
    else
    {
        targetToneCapture = {};
        targetToneProfile = {};
    }
	toneMatchComparison = {};
    toneMatchResult = {};
}

bool AudioPluginAudioProcessor::analyseToneMatchCapture (
    tonematch::CaptureRole role)
{
    tonematch::ToneCaptureData captureCopy;

    {
        const juce::ScopedLock lock (toneMatchDataLock);

        captureCopy =
            role == tonematch::CaptureRole::source
                ? sourceToneCapture
                : targetToneCapture;
    }

    if (!captureCopy.isValid())
        return false;

    tonematch::ToneAnalysis analyser;
    auto profile = analyser.analyse (captureCopy);

    if (!profile.isValid())
        return false;

   {
    const juce::ScopedLock lock (toneMatchDataLock);

    if (role == tonematch::CaptureRole::source)
        sourceToneProfile = std::move (profile);
    else
        targetToneProfile = std::move (profile);

    // Cualquier perfil nuevo invalida la comparación anterior.
    toneMatchComparison = {};
}

    return true;
}

bool AudioPluginAudioProcessor::hasToneMatchProfile (
    tonematch::CaptureRole role) const
{
    const juce::ScopedLock lock (toneMatchDataLock);

    return role == tonematch::CaptureRole::source
        ? sourceToneProfile.isValid()
        : targetToneProfile.isValid();
}

tonematch::ToneAnalysisProfile
AudioPluginAudioProcessor::getToneMatchProfileCopy (
    tonematch::CaptureRole role) const
{
    const juce::ScopedLock lock (toneMatchDataLock);

    return role == tonematch::CaptureRole::source
        ? sourceToneProfile
        : targetToneProfile;
}

bool AudioPluginAudioProcessor::compareToneMatchProfiles()
{
    tonematch::ToneAnalysisProfile sourceCopy;
    tonematch::ToneAnalysisProfile targetCopy;

    {
        const juce::ScopedLock lock (toneMatchDataLock);

        sourceCopy = sourceToneProfile;
        targetCopy = targetToneProfile;
    }

    if (!sourceCopy.isValid()
        || !targetCopy.isValid())
    {
        return false;
    }

    tonematch::ToneMatchComparison comparisonEngine;

    auto comparison =
        comparisonEngine.compare (
            sourceCopy,
            targetCopy);

    if (!comparison.isValid())
        return false;

    {
        const juce::ScopedLock lock (toneMatchDataLock);

        toneMatchComparison =
            std::move (comparison);

        toneMatchResult = {};
    }

    return true;
}

bool AudioPluginAudioProcessor::
    hasToneMatchComparison() const
{
    const juce::ScopedLock lock (toneMatchDataLock);

    return toneMatchComparison.isValid();
}

tonematch::ToneMatchComparisonResult
AudioPluginAudioProcessor::
    getToneMatchComparisonCopy() const
{
    const juce::ScopedLock lock (toneMatchDataLock);

    return toneMatchComparison;
}

void AudioPluginAudioProcessor::
    clearToneMatchComparison()
{
    const juce::ScopedLock lock (toneMatchDataLock);

    toneMatchComparison = {};
    toneMatchResult = {};
}

bool AudioPluginAudioProcessor::generateToneMatchIR()
{
    tonematch::ToneMatchComparisonResult comparisonCopy;

    {
        const juce::ScopedLock lock (toneMatchDataLock);
        comparisonCopy = toneMatchComparison;
    }

    if (!comparisonCopy.isValid())
        return false;

    tonematch::SolverV1 solver;
    tonematch::ToneMatchOptions options;

    auto result = solver.solve (
        comparisonCopy,
        options);

    if (!result.hasValidImpulseResponse())
        return false;

    {
        const juce::ScopedLock lock (toneMatchDataLock);
        toneMatchResult = std::move (result);
    }

    return true;
}

bool AudioPluginAudioProcessor::hasToneMatchResult() const
{
    const juce::ScopedLock lock (toneMatchDataLock);
    return toneMatchResult.hasValidImpulseResponse();
}

tonematch::ToneMatchResult
AudioPluginAudioProcessor::getToneMatchResultCopy() const
{
    const juce::ScopedLock lock (toneMatchDataLock);
    return toneMatchResult;
}

void AudioPluginAudioProcessor::clearToneMatchResult()
{
    const juce::ScopedLock lock (toneMatchDataLock);
    toneMatchResult = {};
}

bool AudioPluginAudioProcessor::saveToneMatchIRToFile (
    const juce::File& file,
    juce::String& errorMessage) const
{
    const auto result = getToneMatchResultCopy();

    if (!result.hasValidImpulseResponse())
    {
        errorMessage = "No generated IR is available";
        return false;
    }

    file.deleteFile();

    auto outputStream = file.createOutputStream();

    if (outputStream == nullptr || !outputStream->openedOk())
    {
        errorMessage = "Could not create the output WAV file";
        return false;
    }

    juce::WavAudioFormat wavFormat;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (
            outputStream.release(),
            result.impulseResponseSampleRate,
            1,
            32,
            {},
            0));

    if (writer == nullptr)
    {
        errorMessage = "Could not create the WAV writer";
        return false;
    }

    if (!writer->writeFromAudioSampleBuffer (
            result.impulseResponse,
            0,
            result.impulseResponse.getNumSamples()))
    {
        errorMessage = "Could not write the IR audio data";
        return false;
    }

    return true;
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter ()
{
    return new AudioPluginAudioProcessor ();
}