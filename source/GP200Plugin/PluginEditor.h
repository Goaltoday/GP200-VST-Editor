#pragma once

#include "EffectBlockComponent.h"
#include "PluginProcessor.h"
#include "../libgp200/MidiConnection.h"

#include <array>
#include <memory>
#include <vector>

class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor () override;

    void paint (juce::Graphics&) override;
    void resized () override;
    void timerCallback () override;

  private:
    using BlockEnabledStates = std::array<bool, gp200::effectBlockCount>;

    void saveCurrentPresetToProject ();
    void recallSavedPresetToGP200 ();

    void startFullPresetRestoreFromSnapshot ();
    void buildFullPresetRestoreSteps (const gp200::GP200Preset& preset, const juce::MemoryBlock& presetData);
    void processFullPresetRestoreStep ();
    void finishFullPresetRestore ();

    void storeCurrentPresetToGP200 ();

    void sendPatchVolumeFromSlider ();
    void sendPatchPanFromSlider ();
    void sendPatchTempoFromSlider ();

    void syncPresetNameEditorFromCurrentPreset ();

    void toggleTuner ();
    void updateTunerButtonText ();
    void toggleAllBlocksOff ();
    bool captureCurrentBlockEnabledStates (BlockEnabledStates& states);
    bool applyBlockEnabledStates (const BlockEnabledStates& states);
    void updateAllBlocksOffButtonText ();

    void syncPatchVolumeSliderFromPresetData (const juce::MemoryBlock& presetData,
                                              const juce::String& presetDataSignature);

    void loadPreviousPreset ();
    void loadNextPreset ();
    void loadPresetRelative (int delta);

    void updateEffectBlocksUI ();
    void rebuildEffectBlocks (const gp200::GP200Preset& preset, const juce::String& newSignature);
    void layoutEffectBlocks ();

    int getDropPositionForContentY (int contentY) const;
    int getDropLineYForPosition (int dropPosition) const;
    void updateDragDropIndicator (int contentY);
    void hideDragDropIndicator ();
    void moveEffectBlockToPosition (int blockIndex, int targetPosition);

    void drawInfoBox (juce::Graphics& g,
                      juce::Rectangle<int> bounds,
                      const juce::String& title,
                      const juce::String& value) const;

    void drawStatusPill (juce::Graphics& g, juce::Rectangle<int> bounds) const;

    juce::String getCurrentPresetCompactText () const;
    juce::String getSavedPresetCompactText () const;

    static int wrapPresetSlot (int slot);
    static juce::String formatSlotCompact (int slot);
    static juce::String formatPresetCompact (int slot, const juce::String& presetName);
    static bool isUsefulPresetName (const juce::String& presetName);

    enum class PresetRestoreStepType
    {
        PatchVolume,
        PatchTempo,
        EffectChange,
        ParamChange,
        ToggleEffect,
        ReorderEffects
    };

    struct PresetRestoreStep
    {
        PresetRestoreStepType type{PresetRestoreStepType::ParamChange};

        int blockIndex{-1};
        int paramIndex{-1};
        juce::uint32 effectId{0};
        float value{0.0f};
        bool shouldBeOn{false};

        gp200::RoutingOrder routingOrder{};
        int fxLoopSend{4};
        int fxLoopReturn{4};
    };

    static constexpr int idleTimerHz = 20;
    static constexpr int restoreTimerHz = 100;

    AudioPluginAudioProcessor& processorRef;
    gp200::MidiConnection midiConnection;

    std::vector<PresetRestoreStep> presetRestoreSteps;
    int presetRestoreStepIndex{0};
    bool presetRestoreInProgress{false};

    juce::MemoryBlock presetRestoreSnapshotData;
    int presetRestoreSlot{-1};
    juce::String presetRestoreName;

    juce::TextButton previousPresetButton{"<"};
	juce::TextButton nextPresetButton{">"};
	juce::TextButton savePresetButton{"Save to DAW"};
	juce::TextButton recallPresetButton{"Recall from DAW"};
	juce::TextButton storePresetButton{"Store to GP-200"};
	juce::TextButton allBlocksOffButton{"FX OFF"};

    class DropIndicatorComponent final : public juce::Component
    {
      public:
        void paint (juce::Graphics& g) override
        {
            const auto line = getLocalBounds ().toFloat ().reduced (8.0f, 1.0f);

            g.setColour (juce::Colour (0xffffa42a));
            g.fillRoundedRectangle (line, 2.0f);

            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawRoundedRectangle (line, 2.0f, 1.0f);
        }
    };

    juce::Slider patchVolumeSlider;
    juce::Slider panSlider;
    juce::Slider tempoSlider;
    juce::TextEditor presetNameEditor;
    juce::TextButton tunerButton{"Tuner OFF"};
    bool tunerIsOn{false};

    BlockEnabledStates savedBlockEnabledStates{};
    bool hasSavedBlockEnabledStates{false};
    bool allBlocksAreTemporarilyOff{false};
    int savedBlockEnabledSlot{-1};

    juce::Viewport effectsViewport;
    juce::Component effectsContent;
    DropIndicatorComponent dropIndicator;
    std::vector<std::unique_ptr<EffectBlockComponent>> effectBlocks;

    juce::String effectBlocksSignature;
    juce::String presetNameEditorSignature;
    juce::String patchVolumeSourceSignature;
    juce::String effectsStatusText{"Effects: waiting for preset data"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
