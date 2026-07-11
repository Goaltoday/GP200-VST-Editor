#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../libgp200/GP200Constants.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor ()
    : AudioProcessor (BusesProperties ()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                          .withInput ("Input", juce::AudioChannelSet::stereo (), true)
#endif
                          .withOutput ("Output", juce::AudioChannelSet::stereo (), true)
#endif
      )
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor () {}

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
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources () {}

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
        setGP200PresetSnapshotState (slot, presetName, presetData);
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

void AudioPluginAudioProcessor::setGP200PresetSnapshotState (int slot,
                                                             const juce::String& presetName,
                                                             const juce::MemoryBlock& presetData)
{
    const juce::ScopedLock lock (stateLock);

    savedGP200PresetSnapshotSlot = slot;

    if (isUsefulPresetName (presetName))
        savedGP200PresetSnapshotName = presetName.trim ();
    else
        savedGP200PresetSnapshotName = "unknown";

    savedGP200PresetData = presetData;
    ++savedPresetRevision;
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

int AudioPluginAudioProcessor::getSavedGP200PresetSnapshotSlot () const
{
    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetSnapshotSlot;
}

juce::String AudioPluginAudioProcessor::getSavedGP200PresetSnapshotName () const
{
    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetSnapshotName;
}

juce::String AudioPluginAudioProcessor::getSavedGP200PresetSnapshotSlotText () const
{
    const juce::ScopedLock lock (stateLock);

    if (savedGP200PresetSnapshotSlot < 0)
        return "Saved preset in DAW: unknown";

    return "Saved preset in DAW: " + formatGP200Slot (savedGP200PresetSnapshotSlot);
}

int AudioPluginAudioProcessor::getSavedGP200PresetDataSize () const
{
    const juce::ScopedLock lock (stateLock);

    return static_cast<int> (savedGP200PresetData.getSize ());
}

bool AudioPluginAudioProcessor::hasSavedGP200PresetData () const
{
    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetData.getSize () > 0;
}

juce::MemoryBlock AudioPluginAudioProcessor::getSavedGP200PresetDataCopy () const
{
    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetData;
}

juce::String AudioPluginAudioProcessor::getSavedGP200PresetDataStatusText () const
{
    const juce::ScopedLock lock (stateLock);

    if (savedGP200PresetData.getSize () == 0)
        return "Saved preset in DAW: not saved";

    return "Saved preset in DAW: " + formatGP200Slot (savedGP200PresetSnapshotSlot) + "  " +
           savedGP200PresetSnapshotName + "  " +
           juce::String (static_cast<int> (savedGP200PresetData.getSize ())) + " bytes";
}

std::uint64_t AudioPluginAudioProcessor::getSavedPresetRevision () const
{
    const juce::ScopedLock lock (stateLock);
    return savedPresetRevision;
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
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("GP200StudioState");

    {
        const juce::ScopedLock lock (stateLock);

        xml->setAttribute ("version", 4);

        xml->setAttribute ("slotReferenceSlot", savedGP200Slot);
        xml->setAttribute ("slotReferenceName", savedGP200PresetName);

        xml->setAttribute ("presetSnapshotSlot", savedGP200PresetSnapshotSlot);
        xml->setAttribute ("presetSnapshotName", savedGP200PresetSnapshotName);
        xml->setAttribute ("presetSnapshotDataBase64", savedGP200PresetData.toBase64Encoding ());
    }

    copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    if (!xml->hasTagName ("GP200StudioState"))
        return;

    const juce::ScopedLock lock (stateLock);

    savedGP200Slot = xml->getIntAttribute ("slotReferenceSlot", xml->getIntAttribute ("gp200Slot", -1));

    savedGP200PresetName =
        xml->getStringAttribute ("slotReferenceName", xml->getStringAttribute ("gp200PresetName", "unknown"));

    savedGP200PresetSnapshotSlot =
        xml->getIntAttribute ("presetSnapshotSlot", xml->getIntAttribute ("gp200Slot", -1));

    savedGP200PresetSnapshotName = xml->getStringAttribute (
        "presetSnapshotName", xml->getStringAttribute ("gp200PresetName", "unknown"));

    savedGP200PresetData.setSize (0);

    auto presetDataBase64 = xml->getStringAttribute ("presetSnapshotDataBase64", {});

    if (presetDataBase64.isEmpty ())
        presetDataBase64 = xml->getStringAttribute ("gp200PresetDataBase64", {});

    if (presetDataBase64.isNotEmpty ())
        savedGP200PresetData.fromBase64Encoding (presetDataBase64);

    ++savedPresetRevision;
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter ()
{
    return new AudioPluginAudioProcessor ();
}