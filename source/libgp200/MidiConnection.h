/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed under GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "GP200Preset.h"
#include "GP200ModSyncProtocol.h"
#include "GP200IR.h"
#include "GP200SoundClone.h"
#include "GP200PresetNameScanner.h"

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace gp200
{
class MidiConnection final : private juce::MidiInputCallback, private juce::Timer
{
  public:
    MidiConnection ();
    ~MidiConnection () override;

    bool connectToGP200 ();
    void disconnect ();

    bool isConnected () const;

    bool requestCurrentPresetFromGP200 ();
    struct RoutingModeSnapshot { int mode{-1}; int slot{-1}; std::uint64_t revision{0}; };
    RoutingModeSnapshot getRoutingModeSnapshot () const;
    struct RoutingStateSnapshot { bool connected{false}; int slot{-1}; bool live{false}; RoutingModeSnapshot mode; juce::MemoryBlock data; std::uint64_t presetRevision{0}, liveRevision{0}; bool canSave{false}, modeFresh{false}; };
    RoutingStateSnapshot getRoutingStateSnapshot () const;
    bool requestRoutingModeFromGP200 ();

    bool requestAssignmentNamesFromGP200 ();
    void processStartupHandshake ();
    void processPendingLivePresetRefresh ();

    juce::String getAssignmentNamesStatusText () const;
    juce::String getUserIRDisplayName (int zeroBasedIndex) const;
    juce::String getSnapToneDisplayName (int zeroBasedIndex) const;
    bool renameSnapToneOnGP200 (int zeroBasedIndex, const juce::String& newName);
    bool renameUserIROnGP200 (int zeroBasedIndex, const juce::String& newName);

    bool sendPresetChange (int slot);

    void startPresetNameScan (int prioritySlot);
    void cancelPresetNameScan ();
    void processPresetNameScan ();
    bool isPresetNameScanRunning () const;
    float getPresetNameScanProgress () const;
    std::uint64_t getPresetNameScanRevision () const;
    juce::String getPresetSlotName (int slot) const;
    bool hasPresetSlotName (int slot) const;
    bool hasCompletePresetNameCache () const;
    void updatePresetNameCache (int slot, const juce::String& name);
    void invalidatePresetNameCacheSlot (int slot);

    bool startIRUpload (const juce::File& wavFile, int zeroBasedUserIRSlot);
    bool startFactoryCabUpload (const juce::File& wavFile, int zeroBasedFactoryCabIndex);
    bool startFactoryAmpUpload (const juce::File& cloFile,
                                int zeroBasedFactoryAmpIndex,
                                const juce::String& requestedDisplayName);
    bool startFactoryAmpRename (int zeroBasedFactoryAmpIndex,
                                const juce::String& requestedDisplayName);
    void processIRUpload ();
    bool isIRUploadInProgress () const;
    juce::String getIRUploadStatusText () const;

    bool startSoundCloneUpload (const juce::File& cloFile, int globalSlot);
    void processSoundCloneUpload ();
    bool isSoundCloneUploadInProgress () const;
    juce::String getSoundCloneUploadStatusText () const;

    bool sendPatchVolume (int value);
    bool sendPatchPan (int pan);
    bool sendPatchTempoBpm (int bpm);

    bool renameCurrentPresetOnGP200 (const juce::String& newName);
    bool sendTunerOnOff (bool shouldBeOn);

    bool sendEffectOnOff (int blockIndex, bool shouldBeOn);
    bool sendEffectChange (int blockIndex, juce::uint32 effectId);
    bool sendAutoCabMatch (bool shouldBeEnabled);
    bool sendParamChange (int blockIndex, int paramIndex, juce::uint32 effectId, float value);
    bool sendSeriesParallel (bool parallel);
    bool sendRoutingModeValue (juce::uint8 value);
    bool sendFlexibleRouting (const RoutingOrder& order, int send, int boundary, int ret, bool parallel, int expectedSlot = -1);
    bool canSaveCurrentPreset () const;
    bool isRoutingTransactionBusy () const;
    juce::String getRoutingTransactionStatus () const;
    struct RoutingRequestSnapshot { bool active{false}; int slot{-1}; RoutingOrder order{}; int send{0}, boundary{0}, ret{0}, mode{1}; };
    RoutingRequestSnapshot getRoutingRequestSnapshot () const;
    bool sendReorderEffects (const RoutingOrder& routingOrder, int fxLoopSend, int fxLoopReturn);
    bool storeCurrentPresetToGP200 ();

    void updateCurrentPresetEffectEnabled (int blockIndex, bool enabled);
    void updateCurrentPresetEffectId (int blockIndex, juce::uint32 effectId);
    void updateCurrentPresetEffectParam (int blockIndex, int paramIndex, float value);
    void updateCurrentPresetRoutingOrder (const RoutingOrder& routingOrder);

    juce::String getStatusText () const;
    juce::String getLastMessageText () const;

    int getCurrentSlot () const;
    juce::String getCurrentSlotText () const;

    juce::String getCurrentPresetName () const;
    juce::String getCurrentPresetNameText () const;

    juce::String getCurrentPresetDumpStatusText () const;
    int getCurrentPresetDumpSize () const;
    bool hasLiveCurrentPresetData () const;
    juce::MemoryBlock getCurrentPresetDumpDataCopy () const;

    std::uint64_t getPresetRevision () const;
    std::uint64_t getLivePresetRevision () const;
    std::uint64_t getAssignmentNamesRevision () const;

    void adoptCurrentPresetSnapshot (int slot,
                                     const juce::String& presetName,
                                     const juce::MemoryBlock& presetData);

    bool requestPresetNameForCurrentSlotIfNeeded ();

    // Suspend automatic preset/state reads while Recall from DAW or PRST import
    // is actively rebuilding the GP-200 edit buffer.
    void beginPresetRestoreTransaction ();
    void endPresetRestoreTransaction (int expectedRoutingMode = -1);
    bool sendPresetRestoreRoutingMode (int expectedSlot, int mode);

  private:
    void timerCallback () override;
    void processRoutingTransaction ();
    void failRoutingTransaction (const juce::String& reason);
    void processPresetReadRecovery ();
    int routingStage{0}; // 1 order, 2 final mode, 3 mode reply, 4 preset, 5 final query, 6 recovery
    int routingSlot{-1}, routingSend{0}, routingBoundary{0}, routingReturn{0}, routingValue{1};
    RoutingOrder routingOrder{};
    std::uint64_t routingGeneration{0}, slotGeneration{0}, routingModeBaseline{0}, routingLiveBaseline{0}, presetModeBaseline{0};
    double routingNextMs{0}, routingDeadlineMs{0}, routingQueryMs{0};
    juce::String routingTransactionStatus{"SPR: idle"};
    double presetReadStartedMs{0}, presetReadResumeMs{0}, modePollMs{0};
    int presetReadRetries{0};
    bool presetReadIsLive{false};
    bool isFactoryAmpDestinationInactiveLocked (int zeroBasedFactoryAmpIndex) const;
    bool processModSyncStartup (double nowMs);
    bool handleModSyncResponse (const juce::uint8* data, int size);
    void finishModSyncFailure (const juce::String& reason);
    void applyModSyncSnapshot ();
    juce::String modSyncStatus{"MOD_SYNC: waiting for startup"};
    bool modSyncAttempted{false}; // Lifetime of this instance; NOT reset on disconnect/editor reopen.
    bool modSyncActive{false};
    bool modSyncWaiting{false};
    int modSyncPage{0}, modSyncRetries{0};
    double modSyncSentMs{0};
    std::uint32_t modSyncNonce{0};
    std::array<modsync::Page,modsync::pageCount> modSyncPages{};

    struct AssignmentNameQuery
    {
        int section{-1};
        int page{-1};
        int block{-1};
    };

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    void handleIncomingSysEx (const juce::MidiMessage& message);
    void parseGP200SysEx (const juce::uint8* data, int size);
    bool handleSoundCloneUploadAck (const juce::uint8* data, int size);
    void completeSoundCloneUpload ();

    enum class StartupHandshakePhase
    {
        Idle,
        WaitingForIdentity,
        WaitingBeforeStateDump,
        WaitingForStateDump,
        WaitingForCurrentPreset,
        Ready
    };

    bool sendIdentityQueryUnlocked ();
    bool sendEnterEditorModeUnlocked ();
    bool sendStateDumpRequestUnlocked ();
    void collectStateDumpChunk (const juce::uint8* data, int size);

    bool sendNextAssignmentNameQuery ();
    bool sendAssignmentNameQuery (int section, int page, int block);
    void handleAssignmentNameResponse (const juce::uint8* data, int size);

    bool sendNextPresetNameScanRequestUnlocked ();
    bool sendReadRequestForSlot (int slot);
    bool sendLiveReadRequestForSlot (int slot);
    void scheduleLivePresetRefresh ();

    void resetPresetDumpCaptureForSlot (int slot);
    void collectPresetReadChunk (const juce::uint8* data, int size);

    static int getChunkOffset (const juce::uint8* data, int size);

    static std::vector<juce::uint8> buildReadRequest (int slot);
    static std::vector<juce::uint8> buildLiveReadRequest (int slot);
    static std::vector<juce::uint8> buildPresetChange (int slot);
    static std::vector<juce::uint8> buildEffectChange (int blockIndex, juce::uint32 effectId);
    static std::vector<juce::uint8> buildAutoCabMatch (bool shouldBeEnabled);
    static std::vector<juce::uint8>
    buildParamChange (int blockIndex, int paramIndex, juce::uint32 effectId, float value);
    static std::vector<juce::uint8> buildPatchSetting (int target, int value);
    static std::vector<juce::uint8>
    buildReorderEffects (const RoutingOrder& routingOrder, int fxLoopSend, int fxLoopReturn);
    static std::vector<juce::uint8> buildIdentityQuery ();
    static std::vector<juce::uint8> buildEnterEditorMode ();
    static std::vector<juce::uint8> buildStateDumpRequest ();
    static std::vector<juce::uint8> buildAssignmentNameQuery (int section, int page, int block);
    static std::vector<juce::uint8> buildRenameSnapTone (int globalSlot, const juce::String& newName);
    static std::vector<juce::uint8> buildRenameUserIR (int zeroBasedIndex, const juce::String& newName);
    static std::vector<juce::uint8> buildStorePresetCommit (int slot, const juce::String& presetName);

    static std::vector<juce::uint8> nibbleEncode (const juce::uint8* data, int size);
    static std::vector<juce::uint8> nibbleDecode (const juce::uint8* data, int size);
    static std::array<juce::uint8, 2> encodeDisplayValue (float value);

    static juce::String parseAssignmentNameResponse (const juce::uint8* data, int size);
    static juce::String cleanAssignmentName (const juce::String& name);
    static juce::String sanitizePresetNameForStore (const juce::String& presetName);

    static int getEffectOnOffCCForBlockIndex (int blockIndex);

    static juce::MemoryBlock assemblePresetReadChunks (const std::vector<std::vector<juce::uint8>>& chunks);

    static juce::String extractPresetNameFromReadChunk (const juce::uint8* data, int size);
    static juce::String extractPresetNameFromDecodedPresetData (const std::vector<juce::uint8>& decoded);

    mutable juce::CriticalSection stateLock;

    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    GP200PresetNameScanner presetNameScanner;

    juce::String statusText{"Not connected"};
    juce::String lastMessageText{"No MIDI messages received yet"};

    int currentSlot{-1};
    juce::String currentPresetName{"unknown"};

    StartupHandshakePhase startupHandshakePhase{StartupHandshakePhase::Idle};
    double startupHandshakeNextActionMs{0.0};
    double startupHandshakePhaseStartedMs{0.0};
    std::vector<std::vector<juce::uint8>> stateDumpChunks;
    bool startupAssignmentNamesRequested{false};

    bool presetNameRequestPending{false};
    bool livePresetReadPending{false};
    bool currentStateRequestPending{false};
    bool currentStateRequestQueued{false};
    double currentStateRequestSentMs{0.0};
    bool liveRefreshPending{false};
    bool presetRestoreTransactionActive{false};
    std::uint64_t presetRestoreSlotGeneration{0};
    double liveRefreshDueMs{0.0};
    int lastRequestedNameSlot{-1};

    int presetDumpSlot{-1};
    std::vector<std::vector<juce::uint8>> presetReadChunks;
    juce::MemoryBlock currentPresetDecodedData;
    bool currentPresetDataIsLive{false};
    juce::String currentPresetDumpStatusText{"Current full preset data: not captured"};

    std::vector<AssignmentNameQuery> pendingAssignmentNameQueries;
    AssignmentNameQuery currentAssignmentNameQuery;
    bool assignmentNameRequestInProgress{false};

    std::array<juce::String, userIRCount> userIRNames{};
    std::array<juce::String, snapToneCount> snapToneNames{};
    juce::String assignmentNamesStatusText{"Assignment names: not requested"};

    RoutingModeSnapshot routingModeSnapshot;
    std::uint64_t livePresetRevision{0};
    std::uint64_t presetRevision{0};
    std::uint64_t assignmentNamesRevision{0};

    enum class IRUploadPhase { Idle, WaitingAfterPrepare, SendingChunks, WaitingBeforeCommit, WaitingAfterCommit };
    GP200IRUpload irUpload;
    IRUploadPhase irUploadPhase{IRUploadPhase::Idle};
    int irUploadChunkIndex{0};
    int hot1FactoryAmpUploadIndex{-1};
    juce::String hot1FactoryAmpSourceFile;
    bool hot2FactoryAmpRename{false};
    double irUploadNextActionMs{0.0};
    juce::String irUploadStatusText{"IR upload: idle"};
    juce::String irUploadLabel{"User IR"};

    enum class SoundCloneUploadPhase { Idle, WaitingAfterPrepare, SendingChunks, WaitingForAck };
    GP200SoundCloneUpload soundCloneUpload;
    SoundCloneUploadPhase soundCloneUploadPhase{SoundCloneUploadPhase::Idle};
    int soundCloneUploadChunkIndex{0};
    double soundCloneUploadNextActionMs{0.0};
    juce::String soundCloneUploadStatusText{"Sound Clone upload: idle"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiConnection)
};
} // namespace gp200
