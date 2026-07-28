/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed under GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "EffectBlockComponent.h"
#include "PluginProcessor.h"
#include "../libgp200/MidiConnection.h"
#include "../libgp200/GP200Preset.h"
#include "TunerDisplayComponent.h"
#include "ToneMatch/ToneMatchPanel.h"

#include <array>
#include <cstdint>
#include <deque>
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
	
	enum class CompareSnapshot
{
    A = 0,
    B = 1
};

    void saveCurrentPresetToProject ();
    void recallSavedPresetToGP200 ();
	
	void openPrstFileChooser ();
    void importPrstFile (const juce::File& file);
    void openExportPrstFileChooser ();
    void exportCurrentPresetToPrst (const juce::File& file);
    void openIRFileChooser ();
    void importIRFile (const juce::File& file);
    void openSoundCloneWindow ();
    void importSoundCloneFile (const juce::File& file, int globalSlot);
	void syncUserIRSlotBoxFromCabEffectId(juce::uint32 effectId);
	
	void selectCompareSnapshot (CompareSnapshot snapshot);
void updateCompareSnapshotButtons ();
int getSelectedCompareSnapshotIndex () const;
juce::String getSelectedCompareSnapshotLabel () const;

    void startFullPresetRestoreFromSnapshot ();
    void buildFullPresetRestoreSteps (const gp200::GP200Preset& preset, const juce::MemoryBlock& presetData);
    void processFullPresetRestoreStep ();
    void finishFullPresetRestore ();

    void storeCurrentPresetToGP200 ();

    void sendPatchVolumeFromSlider ();
    void sendPatchPanFromSlider ();
    void sendPatchTempoFromSlider ();
	void handleTapTempo ();

    void syncPresetNameEditorFromCurrentPreset ();

    void toggleTuner ();
    void updateTunerButtonText ();
    void toggleAllBlocksOff ();
    bool captureCurrentBlockEnabledStates (BlockEnabledStates& states);
    bool applyBlockEnabledStates (const BlockEnabledStates& states);
    void updateAllBlocksOffButtonText ();

    void syncPatchVolumeSliderFromPresetData (const juce::MemoryBlock& presetData,
                                              const juce::String& presetDataSignature);

    void loadPreviousBank ();
    void loadNextBank ();
    void loadPreviousPreset ();
    void loadNextPreset ();
    void loadPresetRelative (int delta);

    void updateEffectBlocksUI ();
    void rebuildEffectBlocks (const gp200::GP200Preset& preset, const juce::String& newSignature);
    void layoutEffectBlocks ();
    void selectEffectBlock (int blockIndex);
    void scheduleEditorHeightUpdate ();
    void updateEditorHeight ();
    void updateEffectChainRibbon (const gp200::GP200Preset& preset);
    void applyInterfaceTypography ();
    void clearInterfaceTypography ();

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
	static juce::String midiNoteToName (int midiNote);

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
    gp200::MidiConnection& midiConnection;
    std::unique_ptr<juce::LookAndFeel_V4> interfaceLookAndFeel;
    std::unique_ptr<juce::LookAndFeel_V4> patchSettingsLookAndFeel;
double lastInitialPresetRequestMs{0.0};

    std::vector<PresetRestoreStep> presetRestoreSteps;
    int presetRestoreStepIndex{0};
    bool presetRestoreInProgress{false};

    juce::MemoryBlock presetRestoreSnapshotData;
    int presetRestoreSlot{-1};
    juce::String presetRestoreName;

    juce::TextButton previousBankButton{"BANK -"};
    juce::TextButton previousPresetButton{"<"};
    juce::TextButton nextPresetButton{">"};
    juce::TextButton nextBankButton{"BANK +"};
	juce::TextButton compareAButton{"A"};
juce::TextButton compareBButton{"B"};
juce::TextButton savePresetButton{"Save to DAW"};
juce::TextButton recallPresetButton{"Recall from DAW"};
juce::TextButton storePresetButton{"Store to GP-200"};
juce::TextButton importPrstButton{"Import PRST"};
juce::TextButton exportPrstButton{"Export PRST"};
juce::TextButton allBlocksOffButton{"FX OFF"};
std::unique_ptr<juce::FileChooser> prstFileChooser;
std::unique_ptr<juce::FileChooser> exportPrstFileChooser;
    juce::TextButton importIRButton{"Import IR"};
    juce::ComboBox userIRSlotBox;
    std::unique_ptr<juce::FileChooser> irFileChooser;
    juce::TextButton soundCloneButton{"Sound Clone"};

CompareSnapshot selectedCompareSnapshot{
    CompareSnapshot::A
};


    class EffectChainRibbonComponent final : public juce::Component
    {
      public:
        struct Item
        {
            int blockIndex{-1};
            juce::String blockName;
            bool enabled{false};
            juce::Colour colour;
        };

        void setItems (std::vector<Item> newItems);
        void setSelectedBlockIndex (int blockIndex);
        void setBlockEnabled (int blockIndex, bool enabled);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent& event) override;

        std::function<void (int blockIndex)> onBlockSelected;
        std::function<void (int blockIndex, int targetPosition)> onBlockReordered;

      private:
        juce::Rectangle<int> getTileBounds (int itemIndex) const;
        int getItemIndexAt (juce::Point<int> position) const;
        int getTargetPositionAtX (int x) const;

        std::vector<Item> items;
        int selectedBlockIndex{-1};
        int pressedItemIndex{-1};
        int dragTargetPosition{-1};
        bool dragging{false};
        juce::Point<int> mouseDownPosition;
    };

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
	juce::TextButton tapTempoButton{"TAP"};
    juce::TextEditor presetNameEditor;
    juce::TextButton tunerButton{"Tuner OFF"};
	TunerDisplayComponent tunerDisplay;
    bool tunerIsOn{false};
	
	double lastTapTimeMs{0.0};
    double tapFlashUntilMs{0.0};
    std::deque<double> tapTempoIntervals;

    BlockEnabledStates savedBlockEnabledStates{};
    bool hasSavedBlockEnabledStates{false};
    bool allBlocksAreTemporarilyOff{false};
    int savedBlockEnabledSlot{-1};

    EffectChainRibbonComponent effectChainRibbon;
    juce::Viewport effectsViewport;
    juce::Component effectsContent;
    DropIndicatorComponent dropIndicator;
    std::vector<std::unique_ptr<EffectBlockComponent>> effectBlocks;

    // Structure signature controls component reconstruction; data signature
    // controls value refreshes. Keeping them separate prevents UI flicker.
    juce::String effectBlocksSignature;
    juce::String effectBlocksDataSignature;

    // Structural changes can arrive as several partial live-preset revisions.
    // Keep the currently painted chain until the incoming structure has been
    // quiet for a short period, then rebuild once. This avoids visible
    // intermediate states when changing presets, effects or User IR models.
    gp200::GP200Preset pendingStructuralPreset;
    juce::String pendingStructuralSignature;
    juce::String pendingStructuralDataSignature;
    double pendingStructuralLastChangeMs{0.0};
    bool pendingStructuralRefresh{false};

    juce::String presetNameEditorSignature;
    juce::String patchVolumeSourceSignature;

    // Optimistic guards prevent delayed live dumps from briefly restoring the
    // previous patch-setting value after a local slider edit.
    int pendingPatchVolumeValue{50};
    int pendingPatchPanValue{0};
    int pendingPatchTempoValue{120};
    double patchVolumeLocalEditUntilMs{0.0};
    double patchPanLocalEditUntilMs{0.0};
    double patchTempoLocalEditUntilMs{0.0};

    juce::String effectsStatusText{"Effects: waiting for preset data"};

    gp200::GP200Preset offlinePreset;
    std::uint64_t offlinePresetRevision{0};
    bool offlinePresetDirty{false};
    bool lastConnectionIndicatorState{false};
    int selectedEffectBlockIndex{-1};
    bool editorHeightUpdatePending{false};
	
	juce::TextButton toneMatchButton{"Tone Match"};
std::unique_ptr<ToneMatchPanel> toneMatchPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
