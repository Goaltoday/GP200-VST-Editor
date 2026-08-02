/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed unde r GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

#include <cstdint>

#include "TunerEngine.h"
#include "../libgp200/MidiConnection.h"
#include "../libgp200/GP200Preset.h"

#include "ToneMatch/ToneMatchCapture.h"
#include "ToneMatch/ToneAnalysis.h"
#include "ToneMatch/ToneMatchComparison.h"
#include "ToneMatch/SolverV1.h"

class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
  public:
    static constexpr int compareSnapshotCount = 2;

    AudioPluginAudioProcessor ();
    ~AudioPluginAudioProcessor () override;

    gp200::MidiConnection& getMidiConnection() noexcept;
    void ensureGP200Connection();

    gp200::GP200Preset& getOfflinePreset() noexcept;
    std::uint64_t& getOfflinePresetRevision() noexcept;
    bool& getOfflinePresetDirty() noexcept;
    int& getOfflinePatchVolume() noexcept;
    int& getOfflinePatchPan() noexcept;
    int& getOfflinePatchTempo() noexcept;
    void notifyOfflineStateChanged();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources () override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor () override;
    bool hasEditor () const override;

    const juce::String getName () const override;

    bool acceptsMidi () const override;
    bool producesMidi () const override;
    bool isMidiEffect () const override;
    double getTailLengthSeconds () const override;

    int getNumPrograms () override;
    int getCurrentProgram () override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
	
	void setTunerEnabled(bool shouldBeEnabled) noexcept;
bool isTunerEnabled() const noexcept;

TunerResult getTunerResult() const noexcept;

    void setGP200SessionState (int slot, const juce::String& presetName);
    void setGP200SessionState (int slot, const juce::String& presetName, const juce::MemoryBlock& presetData);

    void setGP200SlotReferenceState (int slot, const juce::String& presetName);
    void setGP200PresetSnapshotState (int snapshotIndex,
                                  int slot,
                                  const juce::String& presetName,
                                  const juce::MemoryBlock& presetData);

    int getSavedGP200Slot () const;
    juce::String getSavedGP200PresetName () const;
    juce::String getSavedGP200SlotText () const;

    int getSavedGP200PresetSnapshotSlot (int snapshotIndex) const;
juce::String getSavedGP200PresetSnapshotName (int snapshotIndex) const;
juce::String getSavedGP200PresetSnapshotSlotText (int snapshotIndex) const;

int getSavedGP200PresetDataSize (int snapshotIndex) const;
bool hasSavedGP200PresetData (int snapshotIndex) const;
juce::MemoryBlock getSavedGP200PresetDataCopy (int snapshotIndex) const;
juce::String getSavedGP200PresetDataStatusText (int snapshotIndex) const;
std::uint64_t getSavedPresetRevision (int snapshotIndex) const;

    static juce::String formatGP200Slot (int slot);
	
	bool startToneMatchCapture (tonematch::CaptureRole role);
tonematch::ToneCaptureData stopToneMatchCapture();
void clearToneMatchCapture();

bool isToneMatchCapturing() const noexcept;
tonematch::CaptureState getToneMatchCaptureState() const noexcept;
tonematch::CaptureRole getToneMatchCaptureRole() const noexcept;
double getToneMatchCapturedDurationSeconds() const noexcept;
float getToneMatchCapturePeakLinear() const noexcept;

bool hasToneMatchCapture (tonematch::CaptureRole role) const;
tonematch::ToneCaptureData getToneMatchCaptureCopy (
    tonematch::CaptureRole role) const;
void storeToneMatchCapture (tonematch::ToneCaptureData capture);
void clearToneMatchCapture (tonematch::CaptureRole role);

bool analyseToneMatchCapture (tonematch::CaptureRole role);
bool hasToneMatchProfile (tonematch::CaptureRole role) const;
tonematch::ToneAnalysisProfile getToneMatchProfileCopy (
    tonematch::CaptureRole role) const;
	
	bool compareToneMatchProfiles();

bool hasToneMatchComparison() const;

tonematch::ToneMatchComparisonResult
getToneMatchComparisonCopy() const;

void clearToneMatchComparison();

bool generateToneMatchIR();
bool hasToneMatchResult() const;
tonematch::ToneMatchResult getToneMatchResultCopy() const;
void clearToneMatchResult();
bool saveToneMatchIRToFile (
    const juce::File& file,
    juce::String& errorMessage) const;

  private:
    static bool isUsefulPresetName (const juce::String& presetName);
	
	gp200::MidiConnection midiConnection;

    gp200::GP200Preset offlinePreset;
    std::uint64_t offlinePresetRevision{1};
    bool offlinePresetDirty{false};
    int offlinePatchVolume{50};
    int offlinePatchPan{0};
    int offlinePatchTempo{120};

	TunerEngine tunerEngine;
	tonematch::ToneMatchCapture toneMatchCapture;
	
	    struct GP200PresetSnapshot
    {
        int slot{-1};
        juce::String name{"unknown"};
        juce::MemoryBlock data;
        std::uint64_t revision{0};
    };
	
	

    static bool isValidSnapshotIndex (int snapshotIndex);

    mutable juce::CriticalSection stateLock;

    int savedGP200Slot{-1};
    juce::String savedGP200PresetName{"unknown"};

    std::array<GP200PresetSnapshot, compareSnapshotCount> savedGP200PresetSnapshots;
	
	mutable juce::CriticalSection toneMatchDataLock;

tonematch::ToneCaptureData sourceToneCapture;
tonematch::ToneCaptureData targetToneCapture;

tonematch::ToneAnalysisProfile sourceToneProfile;
tonematch::ToneAnalysisProfile targetToneProfile;
tonematch::ToneMatchComparisonResult toneMatchComparison;
tonematch::ToneMatchResult toneMatchResult;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
