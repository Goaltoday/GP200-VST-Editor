/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed u  nder GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "MidiConnection.h"
#include "GP200EffectParamDatabase.h"
#include "MidiDeviceScanner.h"
#include "GP200ModSync.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

namespace gp200
{
MidiConnection::MidiConnection () = default;

void MidiConnection::timerCallback ()
{
    // Advance an already-open connection only. Never discover or open ports here.
    if (!isConnected ())
    {
        stopTimer ();
        return;
    }
    const juce::ScopedLock lock (stateLock);
    if (irUploadPhase != IRUploadPhase::Idle || soundCloneUploadPhase != SoundCloneUploadPhase::Idle || presetRestoreTransactionActive)
    {
        if (modSyncActive) finishModSyncFailure ("interrupted by a transfer");
        return;
    }
    if (startupHandshakePhase == StartupHandshakePhase::Idle) requestCurrentPresetFromGP200 ();
    processStartupHandshake ();
    if (!modSyncActive)
    {
        processPendingLivePresetRefresh ();
        requestPresetNameForCurrentSlotIfNeeded ();
    }
}




MidiConnection::~MidiConnection ()
{
    stopTimer ();
    disconnect ();
}

bool MidiConnection::connectToGP200 ()
{
    disconnect ();

    juce::MidiDeviceInfo selectedInput;
    juce::MidiDeviceInfo selectedOutput;

    if (!MidiDeviceScanner::findFirstGP200Input (selectedInput) ||
        !MidiDeviceScanner::findFirstGP200Output (selectedOutput))
    {
        const juce::ScopedLock lock (stateLock);
        statusText = "GP-200 MIDI input/output not found";
        return false;
    }

    auto newInput = juce::MidiInput::openDevice (selectedInput.identifier, this);
    auto newOutput = juce::MidiOutput::openDevice (selectedOutput.identifier);

    if (newInput == nullptr || newOutput == nullptr)
    {
        const juce::ScopedLock lock (stateLock);
        statusText = "Could not open GP-200 MIDI ports";
        return false;
    }

    {
        const juce::ScopedLock lock (stateLock);
        midiInput = std::move (newInput);
        midiOutput = std::move (newOutput);
        statusText = "Connected to GP-200: IN=" + selectedInput.name + " OUT=" + selectedOutput.name;
        lastMessageText = "Listening for GP-200 messages...";
    }

    midiInput->start ();
    startTimer (40); // Only after both MIDI ports are open.
    return true;
}

void MidiConnection::disconnect ()
{
    stopTimer ();
    std::unique_ptr<juce::MidiInput> inputToStop;

    {
        const juce::ScopedLock lock (stateLock);
        inputToStop = std::move (midiInput);
        midiOutput.reset ();
    }

    if (inputToStop != nullptr)
        inputToStop->stop ();

    const juce::ScopedLock lock (stateLock);

    if (modSyncActive) finishModSyncFailure ("disconnected");
    presetNameScanner.cancel ();

    statusText = "Not connected";
    currentSlot = -1;
    currentPresetName = "unknown";

    startupHandshakePhase = StartupHandshakePhase::Idle;
    startupHandshakeNextActionMs = 0.0;
    startupHandshakePhaseStartedMs = 0.0;
    stateDumpChunks.clear ();
    startupAssignmentNamesRequested = false;

    presetNameRequestPending = false;
    livePresetReadPending = false;
    currentStateRequestPending = false;
    currentStateRequestQueued = false;
    currentStateRequestSentMs = 0.0;
    liveRefreshPending = false;
    liveRefreshDueMs = 0.0;
    lastRequestedNameSlot = -1;

    presetDumpSlot = -1;
    presetReadChunks.clear ();
    currentPresetDecodedData.setSize (0);
    currentPresetDataIsLive = false;
    currentPresetDumpStatusText = "Current full preset data: not captured";

    pendingAssignmentNameQueries.clear ();
    currentAssignmentNameQuery = {};
    assignmentNameRequestInProgress = false;

    for (auto& name : userIRNames)
        name.clear ();

    for (auto& name : snapToneNames)
        name.clear ();

    assignmentNamesStatusText = "Assignment names: not requested";
    ++presetRevision;
    ++assignmentNamesRevision;
}

bool MidiConnection::isConnected () const
{
    const juce::ScopedLock lock (stateLock);
    return midiInput != nullptr && midiOutput != nullptr;
}



bool MidiConnection::startIRUpload (const juce::File& wavFile, int zeroBasedUserIRSlot)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        irUploadStatusText = "IR upload failed: MIDI output not open";
        lastMessageText = irUploadStatusText;
        return false;
    }
    if (irUploadPhase != IRUploadPhase::Idle ||
        soundCloneUploadPhase != SoundCloneUploadPhase::Idle)
    {
        irUploadStatusText = "IR upload already in progress";
        lastMessageText = irUploadStatusText;
        return false;
    }

    GP200IRUpload prepared;
    const auto result = GP200IR::buildUpload (wavFile, zeroBasedUserIRSlot, prepared);
    if (result.failed ())
    {
        irUploadStatusText = "IR upload failed: " + result.getErrorMessage ();
        lastMessageText = irUploadStatusText;
        return false;
    }

    irUpload = std::move (prepared);
    irUploadLabel = "User IR " + juce::String (zeroBasedUserIRSlot + 1);
    irUploadChunkIndex = 0;
    midiOutput->sendMessageNow (irUpload.prepareMessage);
    irUploadPhase = IRUploadPhase::WaitingAfterPrepare;
    irUploadNextActionMs = juce::Time::getMillisecondCounterHiRes () + 200.0;
    irUploadStatusText = irUploadLabel + ": preparing";
    lastMessageText = irUploadStatusText;
    return true;
}

bool MidiConnection::startFactoryCabUpload (const juce::File& wavFile,
                                            int zeroBasedFactoryCabIndex)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        irUploadStatusText = "Factory CAB upload failed: MIDI output not open";
        lastMessageText = irUploadStatusText;
        return false;
    }
    if (irUploadPhase != IRUploadPhase::Idle ||
        soundCloneUploadPhase != SoundCloneUploadPhase::Idle)
    {
        irUploadStatusText = "Factory CAB upload unavailable: another transfer is in progress";
        lastMessageText = irUploadStatusText;
        return false;
    }

    GP200IRUpload prepared;
    const auto result = GP200IR::buildFactoryCabUpload (wavFile,
                                                        zeroBasedFactoryCabIndex,
                                                        prepared);
    if (result.failed ())
    {
        irUploadStatusText = "Factory CAB upload failed: " + result.getErrorMessage ();
        lastMessageText = irUploadStatusText;
        return false;
    }

    irUpload = std::move (prepared);
    irUploadLabel = "Factory CAB " + juce::String (zeroBasedFactoryCabIndex + 1);
    irUploadChunkIndex = 0;
    midiOutput->sendMessageNow (irUpload.prepareMessage);
    irUploadPhase = IRUploadPhase::WaitingAfterPrepare;
    irUploadNextActionMs = juce::Time::getMillisecondCounterHiRes () + 200.0;
    irUploadStatusText = irUploadLabel + ": preparing";
    lastMessageText = irUploadStatusText;
    return true;
}

bool MidiConnection::isFactoryAmpDestinationInactiveLocked (int zeroBasedFactoryAmpIndex) const
{
    if (!juce::isPositiveAndBelow (zeroBasedFactoryAmpIndex, 71)
        || !currentPresetDataIsLive
        || presetRestoreTransactionActive
        || currentPresetDecodedData.getSize ()
               < effectBlockStart + effectBlockCount * effectBlockSize)
        return false;

    const auto ampEffects = GP200EffectDatabase::getEffectsForModule ("AMP");
    juce::uint32 targetEffectId = 0;
    int factoryIndex = 0;
    for (const auto& effect : ampEffects)
    {
        if ((effect.effectId & 0xFF000000u) == 0x0F000000u)
            continue;
        if (factoryIndex++ == zeroBasedFactoryAmpIndex)
        {
            targetEffectId = effect.effectId;
            break;
        }
    }
    if (targetEffectId == 0)
        return false;

    const auto* data = static_cast<const juce::uint8*> (currentPresetDecodedData.getData ());
    for (std::size_t block = 0; block < effectBlockCount; ++block)
    {
        const auto offset = effectBlockStart + block * effectBlockSize + effectIdOffset;
        if (juce::ByteOrder::littleEndianInt (data + offset) == targetEffectId)
            return false;
    }
    return true;
}

bool MidiConnection::startFactoryAmpUpload (const juce::File& cloFile,
                                            int zeroBasedFactoryAmpIndex)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        irUploadStatusText = "Factory AMP upload failed: MIDI output not open";
        lastMessageText = irUploadStatusText;
        return false;
    }
    if (irUploadPhase != IRUploadPhase::Idle ||
        soundCloneUploadPhase != SoundCloneUploadPhase::Idle)
    {
        irUploadStatusText = "Factory AMP upload unavailable: another transfer is in progress";
        lastMessageText = irUploadStatusText;
        return false;
    }

    if (!isFactoryAmpDestinationInactiveLocked (zeroBasedFactoryAmpIndex))
    {
        irUploadStatusText = "HOT1: select a different AMP and wait for the live preset before uploading; bypass is not enough";
        lastMessageText = irUploadStatusText;
        return false;
    }

    GP200IRUpload prepared;
    const auto result = GP200SoundClone::buildFactoryAmpUpload (cloFile,
                                                                zeroBasedFactoryAmpIndex,
                                                                prepared);
    if (result.failed ())
    {
        irUploadStatusText = "Factory AMP upload failed: " + result.getErrorMessage ();
        lastMessageText = irUploadStatusText;
        return false;
    }

    if (!GP200SoundClone::factoryAmpUploadHasHot1Marker (prepared))
    {
        irUploadStatusText = "HOT1 build mismatch: the encoded activation marker is missing; clean-rebuild the VST with all HOT1 files";
        lastMessageText = irUploadStatusText;
        return false;
    }

    irUpload = std::move (prepared);
    hot1FactoryAmpUploadIndex = zeroBasedFactoryAmpIndex;
    hot1FactoryAmpSourceFile = cloFile.getFileName ();
    irUploadLabel = "Factory AMP " + juce::String (zeroBasedFactoryAmpIndex + 1);
    irUploadChunkIndex = 0;
    midiOutput->sendMessageNow (irUpload.prepareMessage);
    irUploadPhase = IRUploadPhase::WaitingAfterPrepare;

    // v1.3 robustness test: only Factory AMP/CLO gets relaxed timing.
    irUploadNextActionMs = juce::Time::getMillisecondCounterHiRes () + 250.0;
    irUploadStatusText = irUploadLabel + ": preparing";
    lastMessageText = irUploadStatusText;
    return true;
}

void MidiConnection::processIRUpload ()
{
    const juce::ScopedLock lock (stateLock);
    if (irUploadPhase == IRUploadPhase::Idle || midiOutput == nullptr)
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes ();
    if (now < irUploadNextActionMs)
        return;

    const bool isFactoryAmpUpload = irUploadLabel.startsWith ("Factory AMP ");
    const double chunkDelayMs      = isFactoryAmpUpload ? 40.0   : 30.0;
    const double preCommitDelayMs  = isFactoryAmpUpload ? 400.0  : 300.0;
    const double postCommitDelayMs = isFactoryAmpUpload ? 1200.0 : 1000.0;

    if (irUploadPhase == IRUploadPhase::WaitingAfterPrepare ||
        irUploadPhase == IRUploadPhase::SendingChunks)
    {
        if (irUploadChunkIndex < static_cast<int> (irUpload.chunks.size ()))
        {
            midiOutput->sendMessageNow (irUpload.chunks[static_cast<std::size_t> (irUploadChunkIndex)]);
            ++irUploadChunkIndex;
            irUploadPhase = IRUploadPhase::SendingChunks;
            irUploadNextActionMs = now + chunkDelayMs;
            irUploadStatusText = irUploadLabel + ": block " + juce::String (irUploadChunkIndex) + "/" +
                                 juce::String (static_cast<int> (irUpload.chunks.size ()));
            lastMessageText = irUploadStatusText;
            return;
        }

        irUploadPhase = IRUploadPhase::WaitingBeforeCommit;
        irUploadNextActionMs = now + preCommitDelayMs;
        irUploadStatusText = irUploadLabel + ": waiting before commit";
        return;
    }

    if (irUploadPhase == IRUploadPhase::WaitingBeforeCommit)
    {
        if (isFactoryAmpUpload
            && !isFactoryAmpDestinationInactiveLocked (hot1FactoryAmpUploadIndex))
        {
            irUploadPhase = IRUploadPhase::Idle;
            hot1FactoryAmpUploadIndex = -1;
            hot1FactoryAmpSourceFile.clear ();
            irUpload = {};
            irUploadStatusText = "HOT1 cancelled before commit: destination selected or live preset unavailable; no Factory AMP write requested";
            lastMessageText = irUploadStatusText;
            return;
        }

        midiOutput->sendMessageNow (irUpload.commitMessage);
        irUploadPhase = IRUploadPhase::WaitingAfterCommit;
        irUploadNextActionMs = now + postCommitDelayMs;
        irUploadStatusText = irUploadLabel + ": commit sent";
        lastMessageText = irUploadStatusText;
        return;
    }

    if (irUploadPhase == IRUploadPhase::WaitingAfterCommit)
    {
        const auto uploadedName = irUpload.displayName;
        const bool wasFactoryAmpUpload = irUploadLabel.startsWith ("Factory AMP ");
        const auto completedFactoryAmpIndex = hot1FactoryAmpUploadIndex;
        const auto completedFactoryAmpSourceFile = hot1FactoryAmpSourceFile;

        irUploadPhase = IRUploadPhase::Idle;
        hot1FactoryAmpUploadIndex = -1;
        hot1FactoryAmpSourceFile.clear ();
        irUploadStatusText = irUploadLabel + " completed: " + uploadedName;
        lastMessageText = irUploadStatusText;
        irUpload = {};

        // Avoid immediate extra MIDI traffic after the custom Factory AMP
        // flash/readback/hot-activation transaction.
        if (wasFactoryAmpUpload)
        {
            const auto ampEffects = GP200EffectDatabase::getEffectsForModule ("AMP");
            int factoryIndex = 0;
            for (const auto& effect : ampEffects)
            {
                if ((effect.effectId & 0xFF000000u) == 0x0F000000u)
                    continue;
                if (factoryIndex++ == completedFactoryAmpIndex)
                {
                    GP200ModSync::recordFactoryAmpOverride (effect.effectId, uploadedName, completedFactoryAmpSourceFile);
                    break;
                }
            }
            irUploadStatusText = irUploadLabel
                               + ": transfer sent (no activation ACK); select the destination AMP now";
            lastMessageText = irUploadStatusText;
            return;
        }

        pendingAssignmentNameQueries.clear ();

        for (auto& name : userIRNames)
            name.clear ();

        for (auto& name : snapToneNames)
            name.clear ();

        ++assignmentNamesRevision;

        constexpr int assignmentPageSize = 16;

        for (int block = 0; block < assignmentPageSize; ++block)
            pendingAssignmentNameQueries.push_back ({ 0, 0, block });

        for (int block = 0;
             block < static_cast<int> (userIRCount) - assignmentPageSize;
             ++block)
        {
            pendingAssignmentNameQueries.push_back ({ 0, 1, block });
        }

        for (int block = 0; block < static_cast<int> (snapToneCount); ++block)
            pendingAssignmentNameQueries.push_back ({ 1, 0, block });

        currentAssignmentNameQuery = {};
        assignmentNameRequestInProgress = false;
        assignmentNamesStatusText = "Assignment names: refreshing after " + irUploadLabel + " upload...";
        sendNextAssignmentNameQuery ();
    }
}

bool MidiConnection::isIRUploadInProgress () const
{
    const juce::ScopedLock lock (stateLock);
    return irUploadPhase != IRUploadPhase::Idle;
}

juce::String MidiConnection::getIRUploadStatusText () const
{
    const juce::ScopedLock lock (stateLock);
    return irUploadStatusText;
}


bool MidiConnection::startSoundCloneUpload (const juce::File& cloFile, int globalSlot)
{
    const juce::ScopedLock lock (stateLock);

    if (midiOutput == nullptr)
    {
        soundCloneUploadStatusText = "Sound Clone upload failed: MIDI output not open";
        lastMessageText = soundCloneUploadStatusText;
        return false;
    }

    if (soundCloneUploadPhase != SoundCloneUploadPhase::Idle ||
        irUploadPhase != IRUploadPhase::Idle)
    {
        soundCloneUploadStatusText = "Sound Clone upload unavailable: another transfer is in progress";
        lastMessageText = soundCloneUploadStatusText;
        return false;
    }

    GP200SoundCloneUpload prepared;
    const auto result = GP200SoundClone::buildUpload (cloFile, globalSlot, prepared);
    if (result.failed ())
    {
        soundCloneUploadStatusText = "Sound Clone upload failed: " + result.getErrorMessage ();
        lastMessageText = soundCloneUploadStatusText;
        return false;
    }

    soundCloneUpload = std::move (prepared);
    soundCloneUploadChunkIndex = 0;
    midiOutput->sendMessageNow (soundCloneUpload.prepareMessage);
    soundCloneUploadPhase = SoundCloneUploadPhase::WaitingAfterPrepare;
    soundCloneUploadNextActionMs = juce::Time::getMillisecondCounterHiRes () + 100.0;
    soundCloneUploadStatusText = "Sound Clone upload: preparing slot " + juce::String (globalSlot + 1);
    lastMessageText = soundCloneUploadStatusText;
    return true;
}

void MidiConnection::processSoundCloneUpload ()
{
    const juce::ScopedLock lock (stateLock);

    if (soundCloneUploadPhase == SoundCloneUploadPhase::Idle || midiOutput == nullptr)
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes ();
    if (now < soundCloneUploadNextActionMs)
        return;

    if (soundCloneUploadPhase == SoundCloneUploadPhase::WaitingAfterPrepare ||
        soundCloneUploadPhase == SoundCloneUploadPhase::SendingChunks)
    {
        if (soundCloneUploadChunkIndex < static_cast<int> (soundCloneUpload.chunks.size ()))
        {
            midiOutput->sendMessageNow (
                soundCloneUpload.chunks[static_cast<std::size_t> (soundCloneUploadChunkIndex)]);

            ++soundCloneUploadChunkIndex;
            soundCloneUploadStatusText =
                "Sound Clone upload: block " + juce::String (soundCloneUploadChunkIndex) + "/" +
                juce::String (static_cast<int> (soundCloneUpload.chunks.size ()));
            lastMessageText = soundCloneUploadStatusText;

            if (soundCloneUploadChunkIndex >= static_cast<int> (soundCloneUpload.chunks.size ()))
            {
                // Stock SnapTone route: no separate commit. The GP-200 sends
                // 0x12/0x0C after it has processed the final model chunk.
                soundCloneUploadPhase = SoundCloneUploadPhase::WaitingForAck;
                soundCloneUploadNextActionMs = now + 2000.0;
                soundCloneUploadStatusText = "Sound Clone upload: waiting for GP-200 confirmation";
                lastMessageText = soundCloneUploadStatusText;
            }
            else
            {
                soundCloneUploadPhase = SoundCloneUploadPhase::SendingChunks;
                soundCloneUploadNextActionMs = now + 30.0;
            }

            return;
        }

        soundCloneUploadPhase = SoundCloneUploadPhase::WaitingForAck;
        soundCloneUploadNextActionMs = now + 2000.0;
        soundCloneUploadStatusText = "Sound Clone upload: waiting for GP-200 confirmation";
        lastMessageText = soundCloneUploadStatusText;
        return;
    }

    if (soundCloneUploadPhase == SoundCloneUploadPhase::WaitingForAck)
    {
        soundCloneUploadStatusText = "Sound Clone upload failed: GP-200 confirmation timeout";
        lastMessageText = soundCloneUploadStatusText;
        soundCloneUploadPhase = SoundCloneUploadPhase::Idle;
        soundCloneUpload = {};
        soundCloneUploadChunkIndex = 0;
    }
}

bool MidiConnection::handleSoundCloneUploadAck (const juce::uint8* data, int size)
{
    if (soundCloneUploadPhase != SoundCloneUploadPhase::WaitingForAck || size < 38)
        return false;

    const bool matchesObservedAck =
        data[8]  == 0x12 && data[9]  == 0x0c &&
        data[13] == 0x01 && data[14] == 0x04 && data[15] == 0x01 &&
        data[18] == 0x08 && data[26] == 0x01;

    if (!matchesObservedAck)
        return false;

    const auto acknowledgedSlot = static_cast<int> (data[22]);
    if (acknowledgedSlot != soundCloneUpload.globalSlot)
        return false;

    completeSoundCloneUpload ();
    return true;
}

void MidiConnection::completeSoundCloneUpload ()
{
    const auto uploadedName = soundCloneUpload.displayName;
    const auto uploadedSlot = soundCloneUpload.globalSlot;

    soundCloneUploadPhase = SoundCloneUploadPhase::Idle;
    soundCloneUploadStatusText = "Sound Clone upload completed: " + uploadedName;
    lastMessageText = soundCloneUploadStatusText;
    soundCloneUpload = {};
    soundCloneUploadChunkIndex = 0;

    if (juce::isPositiveAndBelow (uploadedSlot, static_cast<int> (snapToneNames.size ())))
    {
        snapToneNames[static_cast<std::size_t> (uploadedSlot)] = uploadedName;
        ++assignmentNamesRevision;
    }

    pendingAssignmentNameQueries.clear ();
    constexpr int assignmentPageSize = 16;

    for (int block = 0; block < assignmentPageSize; ++block)
        pendingAssignmentNameQueries.push_back ({ 0, 0, block });

    for (int block = 0;
         block < static_cast<int> (userIRCount) - assignmentPageSize;
         ++block)
        pendingAssignmentNameQueries.push_back ({ 0, 1, block });

    for (int block = 0; block < static_cast<int> (snapToneCount); ++block)
        pendingAssignmentNameQueries.push_back ({ 1, 0, block });

    currentAssignmentNameQuery = {};
    assignmentNameRequestInProgress = false;
    assignmentNamesStatusText = "Assignment names: refreshing after Sound Clone upload...";
    sendNextAssignmentNameQuery ();
}

bool MidiConnection::isSoundCloneUploadInProgress () const
{
    const juce::ScopedLock lock (stateLock);
    return soundCloneUploadPhase != SoundCloneUploadPhase::Idle;
}

juce::String MidiConnection::getSoundCloneUploadStatusText () const
{
    const juce::ScopedLock lock (stateLock);
    return soundCloneUploadStatusText;
}

bool MidiConnection::requestCurrentPresetFromGP200 ()
{
    const juce::ScopedLock lock (stateLock);

    if (presetRestoreTransactionActive || modSyncActive)
        return false;

    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot query current preset: MIDI output not open";
        return false;
    }

    // On the first synchronisation after opening the MIDI ports, mirror the
    // handshake observed in the official GP-200 editor:
    // Identity -> Enter Editor Mode -> 100 ms -> five-chunk State Dump.
    // Once that initial handshake has completed, later editor re-openings only
    // need a fresh State Dump/current-preset read; Editor Mode is not resent.
    if (startupHandshakePhase == StartupHandshakePhase::Idle)
        return sendIdentityQueryUnlocked ();

    if (startupHandshakePhase == StartupHandshakePhase::Ready)
        return sendStateDumpRequestUnlocked ();

    // A startup/current-state transaction is already in progress. The editor
    // timer intentionally calls this method repeatedly until live data arrives;
    // do not overlap transactions.
    return false;
}

bool MidiConnection::sendIdentityQueryUnlocked ()
{
    if (midiOutput == nullptr)
        return false;

    const auto bytes = buildIdentityQuery ();
    const auto message = juce::MidiMessage::createSysExMessage (
        bytes.data () + 1, static_cast<int> (bytes.size () - 2));
    midiOutput->sendMessageNow (message);

    startupHandshakePhase = StartupHandshakePhase::WaitingForIdentity;
    startupHandshakePhaseStartedMs = juce::Time::getMillisecondCounterHiRes ();
    currentPresetDataIsLive = false;
    currentStateRequestPending = true;
    currentStateRequestSentMs = startupHandshakePhaseStartedMs;
    currentPresetDumpStatusText = "Current full preset data: identifying GP-200";
    lastMessageText = "GP-200 startup: identity query sent";
    return true;
}

bool MidiConnection::sendEnterEditorModeUnlocked ()
{
    if (midiOutput == nullptr)
        return false;

    const auto bytes = buildEnterEditorMode ();
    const auto message = juce::MidiMessage::createSysExMessage (
        bytes.data () + 1, static_cast<int> (bytes.size () - 2));
    midiOutput->sendMessageNow (message);

    const auto nowMs = juce::Time::getMillisecondCounterHiRes ();
    startupHandshakePhase = StartupHandshakePhase::WaitingBeforeStateDump;
    startupHandshakePhaseStartedMs = nowMs;
    startupHandshakeNextActionMs = nowMs + 100.0;
    currentPresetDumpStatusText = "Current full preset data: entering editor mode";
    lastMessageText = "GP-200 startup: editor mode requested";
    return true;
}

bool MidiConnection::sendStateDumpRequestUnlocked ()
{
    if (midiOutput == nullptr || presetRestoreTransactionActive)
        return false;

    // Preset-name reads and live preset reads both use 0x12/0x18. Do not start
    // a new current-state transaction while either one is in flight.
    if (presetNameScanner.hasPendingRequest () || presetDumpSlot >= 0 || livePresetReadPending)
    {
        currentStateRequestQueued = true;
        return false;
    }

    const auto bytes = buildStateDumpRequest ();
    const auto message = juce::MidiMessage::createSysExMessage (
        bytes.data () + 1, static_cast<int> (bytes.size () - 2));
    midiOutput->sendMessageNow (message);

    stateDumpChunks.clear ();
    currentPresetDataIsLive = false;
    currentStateRequestQueued = false;
    currentStateRequestPending = true;
    currentStateRequestSentMs = juce::Time::getMillisecondCounterHiRes ();
    startupHandshakePhase = StartupHandshakePhase::WaitingForStateDump;
    startupHandshakePhaseStartedMs = currentStateRequestSentMs;
    currentPresetDumpStatusText = "Current full preset data: receiving state dump 0/5 chunks";
    lastMessageText = "Requested five-chunk current state from GP-200";
    return true;
}

void MidiConnection::processStartupHandshake ()
{
    const juce::ScopedLock lock (stateLock);

    if (midiOutput == nullptr || presetRestoreTransactionActive)
        return;

    const auto nowMs = juce::Time::getMillisecondCounterHiRes ();
    constexpr double startupTimeoutMs = 1200.0;

    if (startupHandshakePhase == StartupHandshakePhase::WaitingBeforeStateDump
        && nowMs >= startupHandshakeNextActionMs)
    {
        sendStateDumpRequestUnlocked ();
        return;
    }

    // Keep the existing retry behaviour, but retry the correct handshake phase
    // rather than starting overlapping state/preset transactions.
    if (startupHandshakePhase == StartupHandshakePhase::WaitingForIdentity
        && nowMs - startupHandshakePhaseStartedMs >= startupTimeoutMs)
    {
        sendIdentityQueryUnlocked ();
        return;
    }

    if (startupHandshakePhase == StartupHandshakePhase::WaitingForStateDump
        && nowMs - startupHandshakePhaseStartedMs >= startupTimeoutMs)
    {
        sendStateDumpRequestUnlocked ();
        return;
    }

    // Auxiliary User IR / SnapTone names are deliberately postponed until the
    // seven-chunk active preset has completed. This keeps startup traffic in the
    // same order as the official editor without changing how those names are
    // queried or refreshed later.
    if (startupHandshakePhase == StartupHandshakePhase::Ready
        && !processModSyncStartup (nowMs))
        return;

    if (startupHandshakePhase == StartupHandshakePhase::Ready
        && !startupAssignmentNamesRequested)
    {
        startupAssignmentNamesRequested = true;
        requestAssignmentNamesFromGP200 ();
    }
}

void MidiConnection::finishModSyncFailure (const juce::String& reason)
{
    modSyncActive = false;
    modSyncWaiting = false;
    modSyncAttempted = true;
    modSyncPages = {};
    modSyncStatus = "MOD_SYNC: " + reason + "; cache not verified";
    lastMessageText = modSyncStatus;
}

bool MidiConnection::processModSyncStartup (double nowMs)
{
    if (!modSyncActive && modSyncAttempted) return true;
    if (!modSyncActive)
    {
        if (currentStateRequestPending || presetNameRequestPending || livePresetReadPending
            || presetDumpSlot >= 0 || presetNameScanner.hasPendingRequest () || assignmentNameRequestInProgress)
        {
            modSyncStatus = "MOD_SYNC waiting:";
            if (currentStateRequestPending) modSyncStatus += " state";
            if (presetNameRequestPending) modSyncStatus += " name";
            if (livePresetReadPending || presetDumpSlot >= 0) modSyncStatus += " preset";
            if (presetNameScanner.hasPendingRequest ()) modSyncStatus += " name scan";
            if (assignmentNameRequestInProgress) modSyncStatus += " assignments";
            return false;
        }
        modSyncAttempted = true;
        modSyncActive = true;
        modSyncPage = 0;
        modSyncRetries = 0;
        modSyncNonce = static_cast<std::uint32_t> (juce::Random::getSystemRandom ().nextInt ());
        modSyncPages = {};
    }
    if (modSyncWaiting)
    {
        if (nowMs - modSyncSentMs < 800.0) return false;
        if (modSyncRetries >= 2)
        {
            finishModSyncFailure ("MIDI query timed out");
            return true;
        }
        ++modSyncRetries;
        modSyncWaiting = false;
    }
    if (modSyncPage == modsync::pageCount)
    {
        applyModSyncSnapshot ();
        modSyncActive = false;
        modSyncPages = {};
        return true;
    }
    const auto bytes = modsync::request (modSyncPage, modSyncNonce);
    modSyncWaiting = true;
    modSyncSentMs = nowMs;
    modSyncStatus = "MOD_SYNC page " + juce::String (modSyncPage + 1) + "/19, retry " + juce::String (modSyncRetries);
    midiOutput->sendMessageNow (juce::MidiMessage::createSysExMessage (bytes.data ()+1, static_cast<int> (bytes.size ()-2)));
    return false;
}

bool MidiConnection::handleModSyncResponse (const juce::uint8* data, int size)
{
    modsync::Page page;
    const auto result = modsync::decode (data, size, modSyncPage, modSyncNonce, page);
    if (result == modsync::Decode::unrelated) return false;
    // Consume late/duplicate/malformed MS11 frames before all stock parsers.
    if (!modSyncActive || !modSyncWaiting) return true;
    if (irUploadPhase != IRUploadPhase::Idle || soundCloneUploadPhase != SoundCloneUploadPhase::Idle || presetRestoreTransactionActive)
    {
        finishModSyncFailure ("interrupted by a transfer");
        return true;
    }
    if (result != modsync::Decode::valid) return true; // Bounded timeout/retry handles corrupt frames.
    if (modSyncPage > 0 && page.bytes[19] != modSyncPages[0].bytes[19])
    {
        finishModSyncFailure ("capabilities changed during snapshot");
        return true;
    }
    modSyncPages[static_cast<std::size_t> (modSyncPage)] = page;
    ++modSyncPage;
    modSyncWaiting = false;
    modSyncRetries = 0;
    return true;
}

void MidiConnection::applyModSyncSnapshot ()
{
    juce::Array<juce::var> amps, cabs;
    for (int page = 1; page < modsync::pageCount; ++page)
    {
        const auto& bytes = modSyncPages[static_cast<std::size_t> (page)].bytes;
        const bool isAmp = page >= 10;
        for (int i = 0; i < bytes[18]; ++i)
        {
            const auto* rec = bytes.data () + 24 + i*18;
            if (isAmp && rec[0] == 0) continue;
            const auto index = (page - (isAmp ? 10 : 1))*8 + i;
            const auto id = isAmp ? modsync::ampIds[static_cast<std::size_t> (index)] : 0x0a000000u + static_cast<unsigned> (index);
            auto* entry = new juce::DynamicObject ();
            entry->setProperty ("effect_id", static_cast<juce::int64> (id));
            entry->setProperty ("display_name", juce::String::fromUTF8 (reinterpret_cast<const char*> (rec+2),16).trim ());
            entry->setProperty ("source_file", "");
            if (isAmp) { entry->setProperty ("parameter_profile", "CLO5"); entry->setProperty ("parameter_count", 5); amps.add (juce::var(entry)); }
            else cabs.add (juce::var(entry));
        }
    }
    // Complete replacement also removes AMP overrides that are now stock.
    auto* root = new juce::DynamicObject ();
    root->setProperty ("factory_amp_overrides", amps);
    root->setProperty ("factory_cab_overrides", cabs);
    const bool saved = GP200ModSync::replaceFromDevice (juce::var(root), (modSyncPages[0].bytes[19]&2) ? 2048 : 1024);
    modSyncStatus = saved ? "MOD_SYNC OK: 70 CAB, " + juce::String (amps.size ()) + " AMP overrides" : "MOD_SYNC applied; cache save failed";
    lastMessageText = modSyncStatus;
}

void MidiConnection::collectStateDumpChunk (const juce::uint8* data, int size)
{
    if (startupHandshakePhase != StartupHandshakePhase::WaitingForStateDump)
        return;

    const int offset = getChunkOffset (data, size);
    constexpr std::array<int, 5> expectedOffsets { 0, 185, 370, 555, 740 };

    if (std::find (expectedOffsets.begin (), expectedOffsets.end (), offset) == expectedOffsets.end ())
        return;

    for (const auto& existing : stateDumpChunks)
    {
        if (getChunkOffset (existing.data (), static_cast<int> (existing.size ())) == offset)
            return;
    }

    stateDumpChunks.emplace_back (data, data + size);
    currentPresetDumpStatusText = "Current full preset data: receiving state dump "
        + juce::String (static_cast<int> (stateDumpChunks.size ())) + "/5 chunks";

    if (stateDumpChunks.size () < expectedOffsets.size ())
        return;

    const auto assembled = assemblePresetReadChunks (stateDumpChunks);
    stateDumpChunks.clear ();

    if (assembled.getSize () < 10)
        return;

    const auto* decoded = static_cast<const juce::uint8*> (assembled.getData ());
    const int slot = static_cast<int> (decoded[8])
                   | (static_cast<int> (decoded[9]) << 8);

    if (slot < 0 || slot >= 256)
        return;

    currentStateRequestPending = false;
    currentStateRequestQueued = false;
    currentSlot = slot;

    juce::String name;
    if (assembled.getSize () >= 44)
    {
        std::vector<juce::uint8> decodedVector (decoded, decoded + assembled.getSize ());
        name = extractPresetNameFromDecodedPresetData (decodedVector);
    }

    if (name.isNotEmpty ())
    {
        currentPresetName = name;
        presetNameScanner.setCachedName (currentSlot, name);
    }
    else
    {
        currentPresetName = "requesting...";
    }

    // The state dump establishes the active slot. The existing seven-chunk
    // live read remains the source of the complete editable preset snapshot.
    presetNameRequestPending = true;
    livePresetReadPending = true;
    lastRequestedNameSlot = -1;
    startupHandshakePhase = StartupHandshakePhase::WaitingForCurrentPreset;
    startupHandshakePhaseStartedMs = juce::Time::getMillisecondCounterHiRes ();

    if (presetDumpSlot != slot)
        resetPresetDumpCaptureForSlot (slot);

    lastMessageText = "Current GP-200 slot from complete state dump: "
        + juce::String (slot) + " / " + currentPresetName;
}

void MidiConnection::processPendingLivePresetRefresh ()
{
    const juce::ScopedLock lock (stateLock);

    if (modSyncActive || !liveRefreshPending || midiOutput == nullptr || currentSlot < 0)
        return;

    const auto nowMs = juce::Time::getMillisecondCounterHiRes ();

    if (nowMs < liveRefreshDueMs)
        return;

    // Do not overlap a live seven-chunk read with either another preset read
    // or an already-in-flight preset-name scan request. Both protocols reply
    // with 0x12/0x18 messages and must remain strictly serialized.
    if (presetDumpSlot >= 0
        || presetNameScanner.hasPendingRequest ())
    {
        return;
    }

    liveRefreshPending = false;
    sendLiveReadRequestForSlot (currentSlot);
}

bool MidiConnection::requestAssignmentNamesFromGP200 ()
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        assignmentNamesStatusText = "Assignment names: MIDI output not open";
        lastMessageText = assignmentNamesStatusText;
        return false;
    }

    pendingAssignmentNameQueries.clear ();

    // Keep the current cache visible while fresh names are requested.
    // Each received response replaces only its corresponding entry.

    // Section 0: User IR 1-20. Page 0 contains 16 entries and page 1
    // contains the remaining entries.
    constexpr int assignmentPageSize = 16;

    for (int block = 0; block < assignmentPageSize; ++block)
        pendingAssignmentNameQueries.push_back ({0, 0, block});

    for (int block = 0; block < static_cast<int> (userIRCount) - assignmentPageSize; ++block)
    {
        pendingAssignmentNameQueries.push_back ({0, 1, block});
    }

    // Section 1: SnapTone 1-10.
    for (int block = 0; block < static_cast<int> (snapToneCount); ++block)
        pendingAssignmentNameQueries.push_back ({1, 0, block});

    currentAssignmentNameQuery = {};
    assignmentNameRequestInProgress = false;

    assignmentNamesStatusText = "Assignment names: requesting...";
    lastMessageText = assignmentNamesStatusText;

    return sendNextAssignmentNameQuery ();
}

juce::String MidiConnection::getAssignmentNamesStatusText () const
{
    const juce::ScopedLock lock (stateLock);
    return assignmentNamesStatusText;
}

juce::String MidiConnection::getUserIRDisplayName (int zeroBasedIndex) const
{
    const juce::ScopedLock lock (stateLock);
    if (zeroBasedIndex < 0 || zeroBasedIndex >= static_cast<int> (userIRNames.size ()))
        return {};

    const auto fallback = "User IR " + juce::String (zeroBasedIndex + 1);
    const auto name = userIRNames[static_cast<std::size_t> (zeroBasedIndex)].trim ();

    if (name.isEmpty ())
        return fallback;

    return fallback + " - " + name;
}

juce::String MidiConnection::getSnapToneDisplayName (int zeroBasedIndex) const
{
    const juce::ScopedLock lock (stateLock);
    if (zeroBasedIndex < 0 || zeroBasedIndex >= static_cast<int> (snapToneNames.size ()))
        return {};

    const auto fallback = "SnapTone " + juce::String (zeroBasedIndex + 1);
    const auto name = snapToneNames[static_cast<std::size_t> (zeroBasedIndex)].trim ();

    if (name.isEmpty ())
        return fallback;

    return fallback + " - " + name;
}

bool MidiConnection::renameSnapToneOnGP200 (int zeroBasedIndex, const juce::String& newName)
{
    const juce::ScopedLock lock (stateLock);

    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot rename SnapTone: MIDI output not open";
        return false;
    }

    if (zeroBasedIndex < 0 || zeroBasedIndex >= static_cast<int> (snapToneNames.size ()))
    {
        lastMessageText = "Cannot rename SnapTone: invalid slot";
        return false;
    }

    if (irUploadPhase != IRUploadPhase::Idle ||
        soundCloneUploadPhase != SoundCloneUploadPhase::Idle ||
        presetRestoreTransactionActive)
    {
        lastMessageText = "Cannot rename SnapTone: another transfer is in progress";
        return false;
    }

    auto safeName = newName.trim ().substring (0, 16);
    if (safeName.isEmpty ())
    {
        lastMessageText = "Cannot rename SnapTone: name is empty";
        return false;
    }

    for (int i = 0; i < safeName.length (); ++i)
    {
        const auto c = safeName[i];
        if (c < 32 || c > 126)
            safeName = safeName.replaceSection (i, 1, " ");
    }

    const auto bytes = buildRenameSnapTone (zeroBasedIndex, safeName);
    auto message = juce::MidiMessage::createSysExMessage (bytes.data () + 1,
                                                           static_cast<int> (bytes.size () - 2));
    midiOutput->sendMessageNow (message);

    snapToneNames[static_cast<std::size_t> (zeroBasedIndex)] = safeName;
    ++assignmentNamesRevision;

    const auto blockType = zeroBasedIndex < 5 ? "AMP" : "DIST";
    const auto localSlot = zeroBasedIndex < 5 ? zeroBasedIndex + 1 : zeroBasedIndex - 4;
    lastMessageText = "Renamed SnapTone " + juce::String (localSlot) + " (" + blockType + ") to: " + safeName;
    return true;
}

bool MidiConnection::renameUserIROnGP200 (int zeroBasedIndex, const juce::String& newName)
{
    const juce::ScopedLock lock (stateLock);

    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot rename User IR: MIDI output not open";
        return false;
    }

    if (zeroBasedIndex < 0 || zeroBasedIndex >= static_cast<int> (userIRNames.size ()))
    {
        lastMessageText = "Cannot rename User IR: invalid slot";
        return false;
    }

    if (irUploadPhase != IRUploadPhase::Idle ||
        soundCloneUploadPhase != SoundCloneUploadPhase::Idle ||
        presetRestoreTransactionActive)
    {
        lastMessageText = "Cannot rename User IR: another transfer is in progress";
        return false;
    }

    auto safeName = newName.trim ().substring (0, 16);
    if (safeName.isEmpty ())
    {
        lastMessageText = "Cannot rename User IR: name is empty";
        return false;
    }

    for (int i = 0; i < safeName.length (); ++i)
    {
        const auto c = safeName[i];
        if (c < 32 || c > 126)
            safeName = safeName.replaceSection (i, 1, " ");
    }

    const auto bytes = buildRenameUserIR (zeroBasedIndex, safeName);
    auto message = juce::MidiMessage::createSysExMessage (bytes.data () + 1,
                                                           static_cast<int> (bytes.size () - 2));
    midiOutput->sendMessageNow (message);

    userIRNames[static_cast<std::size_t> (zeroBasedIndex)] = safeName;
    ++assignmentNamesRevision;

    lastMessageText = "Renamed User IR " + juce::String (zeroBasedIndex + 1) + " to: " + safeName;
    return true;
}

bool MidiConnection::sendNextAssignmentNameQuery ()
{
    if (pendingAssignmentNameQueries.empty ())
    {
        assignmentNameRequestInProgress = false;
        currentAssignmentNameQuery = {};

        int userIRCount = 0;
        int snapToneCount = 0;

        for (const auto& name : userIRNames)
        {
            if (name.trim ().isNotEmpty ())
                ++userIRCount;
        }

        for (const auto& name : snapToneNames)
        {
            if (name.trim ().isNotEmpty ())
                ++snapToneCount;
        }

        assignmentNamesStatusText = "Assignment names: loaded User IR " + juce::String (userIRCount) + "/" +
                                    juce::String (static_cast<int> (userIRNames.size ())) + ", SnapTone " +
                                    juce::String (snapToneCount) + "/" +
                                    juce::String (static_cast<int> (snapToneNames.size ()));

        lastMessageText = assignmentNamesStatusText;
        return true;
    }

    currentAssignmentNameQuery = pendingAssignmentNameQueries.front ();
    pendingAssignmentNameQueries.erase (pendingAssignmentNameQueries.begin ());

    return sendAssignmentNameQuery (currentAssignmentNameQuery.section,
                                    currentAssignmentNameQuery.page,
                                    currentAssignmentNameQuery.block);
}

bool MidiConnection::sendAssignmentNameQuery (int section, int page, int block)
{
    if (midiOutput == nullptr)
    {
        assignmentNamesStatusText = "Assignment names: MIDI output not open";
        lastMessageText = assignmentNamesStatusText;
        return false;
    }

    const auto bytes = buildAssignmentNameQuery (section, page, block);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    assignmentNameRequestInProgress = true;

    const auto label = section == 0 ? "User IR " + juce::String (page * 16 + block + 1)
                                    : "SnapTone " + juce::String (block + 1);

    assignmentNamesStatusText = "Assignment names: requesting " + label;
    lastMessageText = assignmentNamesStatusText;

    return true;
}

bool MidiConnection::sendPatchVolume (int value)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot change Patch VOL: MIDI output not open";
        return false;
    }

    const auto safeValue = juce::jlimit (0, 100, value);

    constexpr int midiChannel = 1;
    constexpr int patchVolumeCC = 7;

    const auto message = juce::MidiMessage::controllerEvent (midiChannel, patchVolumeCC, safeValue);

    midiOutput->sendMessageNow (message);

    if (currentPresetDecodedData.getSize () > patchVolumeOffset)
    {
        auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());

        if (data != nullptr)
        {
            data[patchVolumeOffset] = static_cast<juce::uint8> (safeValue);
            ++presetRevision;
        }
    }

    lastMessageText = "Sent Patch VOL CC7 = " + juce::String (safeValue);

    return true;
}
bool MidiConnection::sendPatchPan (int pan)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot change Patch PAN: MIDI output not open";
        return false;
    }

    const auto safePan = juce::jlimit (-100, 100, pan);

    /*
        UI:
            -100 = L100
              0 = C
             100 = R100

        Device candidate:
            0..100 for center/right
            >127 for left side
    */
    const auto deviceValue = safePan >= 0 ? safePan : 256 + safePan;

    const auto bytes = buildPatchSetting (0x06, deviceValue);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    juce::String panText;

    if (safePan == 0)
        panText = "C";
    else if (safePan < 0)
        panText = "L" + juce::String (std::abs (safePan));
    else
        panText = "R" + juce::String (safePan);

    lastMessageText = "Sent Patch PAN = " + panText;

    return true;
}

bool MidiConnection::sendPatchTempoBpm (int bpm)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot change Patch Tempo: MIDI output not open";
        return false;
    }

    const auto safeBpm = juce::jlimit (40, 250, bpm);

    // Patch Setting target:
    // 0x00 = Patch VOL
    // 0x01 = Patch Tempo / BPM
    // 0x06 = Patch PAN
    //
    // Keep the visible BPM value as-is.
    // No -1 conversion here because the GP-200 display matches the plugin value.
    const auto bytes = buildPatchSetting (0x01, safeBpm);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    if (currentPresetDecodedData.getSize () > patchTempoOffset)
    {
        auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());

        if (data != nullptr)
        {
            data[patchTempoOffset] = static_cast<juce::uint8> (safeBpm);
            ++presetRevision;
        }
    }

    lastMessageText = "Sent Patch Tempo setting = " + juce::String (safeBpm) + " BPM";

    return true;
}

bool MidiConnection::sendTunerOnOff (bool shouldBeOn)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot toggle tuner: MIDI output not open";
        return false;
    }

    constexpr int midiChannel = 1;
    constexpr int tunerCC = 58;

    const auto value = shouldBeOn ? 127 : 0;

    const auto message = juce::MidiMessage::controllerEvent (midiChannel, tunerCC, value);

    midiOutput->sendMessageNow (message);

    lastMessageText = shouldBeOn ? "Sent TUNER ON CC58" : "Sent TUNER OFF CC58";

    return true;
}

bool MidiConnection::sendEffectOnOff (int blockIndex, bool shouldBeOn)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot toggle effect: MIDI output not open";
        return false;
    }

    // Special case: VOL does not have a standard MIDI CC ON/OFF command.
    // Keep all other blocks using the existing CC path, but toggle VOL by SysEx.
    if (blockIndex == 10)
    {
        const std::vector<juce::uint8> bytes{0xF0,
                                             0x21,
                                             0x25,
                                             0x7E,
                                             0x47,
                                             0x50,
                                             0x2D,
                                             0x32,
                                             0x12,
                                             0x10,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x04,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x01,
                                             0x05,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x04,
                                             0x00,
                                             0x00,
                                             0x00,
                                             0x0A,
                                             0x00,
                                             static_cast<juce::uint8> (shouldBeOn ? 0x01 : 0x00),
                                             0x09,
                                             0x0C,
                                             0x00,
                                             0x02,
                                             0xF7};

        auto message =
            juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

        midiOutput->sendMessageNow (message);

        updateCurrentPresetEffectEnabled (blockIndex, shouldBeOn);

        lastMessageText = "Sent VOL " + juce::String (shouldBeOn ? "ON" : "OFF") + " using SysEx";

        return true;
    }

    const auto ccNumber = getEffectOnOffCCForBlockIndex (blockIndex);

    if (ccNumber < 0)
    {
        lastMessageText = "Cannot toggle effect: unsupported block " + juce::String (blockIndex);

        return false;
    }

    constexpr int midiChannel = 1;
    const int value = shouldBeOn ? 127 : 0;

    const auto message = juce::MidiMessage::controllerEvent (midiChannel, ccNumber, value);

    midiOutput->sendMessageNow (message);

    updateCurrentPresetEffectEnabled (blockIndex, shouldBeOn);

    lastMessageText = "Sent effect " + juce::String (blockIndex) + (shouldBeOn ? " ON" : " OFF") +
                      " using CC" + juce::String (ccNumber);

    return true;
}

bool MidiConnection::sendEffectChange (int blockIndex, juce::uint32 effectId)
{
    const juce::ScopedLock lock (stateLock);
    if (blockIndex < 0 || blockIndex >= static_cast<int> (effectBlockCount))
    {
        lastMessageText = "Cannot change effect: invalid block index";
        return false;
    }

    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot change effect: MIDI output not open";
        return false;
    }

    const auto bytes = buildEffectChange (blockIndex, effectId);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    updateCurrentPresetEffectId (blockIndex, effectId);

    lastMessageText = "Sent effect change to GP-200: block " + juce::String (blockIndex) + " -> " +
                      juce::String::toHexString (static_cast<juce::int64> (effectId));

    return true;
}

bool MidiConnection::sendAutoCabMatch (bool shouldBeEnabled)
{
    const juce::ScopedLock lock (stateLock);

    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot change AUTO CAB: MIDI output not open";
        return false;
    }

    const auto bytes = buildAutoCabMatch (shouldBeEnabled);
    const auto message = juce::MidiMessage::createSysExMessage (
        bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);
    lastMessageText = shouldBeEnabled ? "AUTO CAB enabled" : "AUTO CAB disabled";
    return true;
}

bool MidiConnection::sendParamChange (int blockIndex, int paramIndex, juce::uint32 effectId, float value)
{
    const juce::ScopedLock lock (stateLock);
    if (blockIndex < 0 || blockIndex >= static_cast<int> (effectBlockCount))
    {
        lastMessageText = "Cannot change parameter: invalid block index";
        return false;
    }

    if (paramIndex < 0 || paramIndex >= static_cast<int> (effectParamCount))
    {
        lastMessageText = "Cannot change parameter: invalid parameter index";
        return false;
    }

    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot change parameter: MIDI output not open";
        return false;
    }

    const auto bytes = buildParamChange (blockIndex, paramIndex, effectId, value);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    updateCurrentPresetEffectParam (blockIndex, paramIndex, value);

    lastMessageText = "Sent parameter change to GP-200: block " + juce::String (blockIndex) + " param " +
                      juce::String (paramIndex) + " = " + juce::String (value, 2);

    return true;
}

bool MidiConnection::sendReorderEffects (const RoutingOrder& routingOrder, int fxLoopSend, int fxLoopReturn)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot reorder effects: MIDI output not open";
        return false;
    }

    std::array<bool, effectBlockCount> seen{};
    seen.fill (false);

    for (const auto blockIndex : routingOrder)
    {
        if (blockIndex < 0 || blockIndex >= static_cast<int> (effectBlockCount))
        {
            lastMessageText = "Cannot reorder effects: invalid routing order";
            return false;
        }

        if (seen[static_cast<std::size_t> (blockIndex)])
        {
            lastMessageText = "Cannot reorder effects: duplicate block in routing order";
            return false;
        }

        seen[static_cast<std::size_t> (blockIndex)] = true;
    }

    const auto bytes = buildReorderEffects (routingOrder, fxLoopSend, fxLoopReturn);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    updateCurrentPresetRoutingOrder (routingOrder);

    lastMessageText = "Sent effect chain reorder to GP-200";

    return true;
}

bool MidiConnection::storeCurrentPresetToGP200 ()
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot store preset: MIDI output not open";
        return false;
    }

    if (currentSlot < 0 || currentSlot > 255)
    {
        lastMessageText = "Cannot store preset: current slot unknown";
        return false;
    }

    const auto presetName = sanitizePresetNameForStore (currentPresetName);

    if (presetName.isEmpty ())
    {
        lastMessageText = "Cannot store preset: current preset name unknown";
        return false;
    }

    const auto bytes = buildStorePresetCommit (currentSlot, presetName);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    presetNameScanner.setCachedName (currentSlot, presetName);

    lastMessageText =
        "Stored current preset to GP-200 slot " + juce::String (currentSlot) + ": " + presetName;

    return true;
}
bool MidiConnection::renameCurrentPresetOnGP200 (const juce::String& newName)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot rename preset: MIDI output not open";
        return false;
    }

    if (currentSlot < 0 || currentSlot > 255)
    {
        lastMessageText = "Cannot rename preset: current slot unknown";
        return false;
    }

    const auto safeName = sanitizePresetNameForStore (newName);

    if (safeName.isEmpty ())
    {
        lastMessageText = "Cannot rename preset: invalid name";
        return false;
    }

    currentPresetName = safeName;

    constexpr auto presetNameLength = presetNameMaxLength;

    if (currentPresetDecodedData.getSize () >= presetNameOffset + presetNameLength)
    {
        auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());

        if (data != nullptr)
        {
            for (std::size_t i = 0; i < presetNameLength; ++i)
                data[presetNameOffset + i] = 0;

            for (int i = 0; i < safeName.length () && i < static_cast<int> (presetNameLength); ++i)
            {
                const auto c = safeName[i];

                if (c >= 32 && c <= 126)
                    data[presetNameOffset + static_cast<std::size_t> (i)] = static_cast<juce::uint8> (c);
                else
                    data[presetNameOffset + static_cast<std::size_t> (i)] = static_cast<juce::uint8> (' ');
            }
        }

        ++presetRevision;
    }

    if (!storeCurrentPresetToGP200 ())
        return false;

    lastMessageText = "Renamed current preset to: " + safeName;

    return true;
}

void MidiConnection::updateCurrentPresetEffectEnabled (int blockIndex, bool enabled)
{
    const juce::ScopedLock lock (stateLock);
    if (blockIndex < 0 || blockIndex >= static_cast<int> (effectBlockCount))
        return;

    const auto base = effectBlockStart + static_cast<std::size_t> (blockIndex) * effectBlockSize;
    const auto enabledByteOffset = base + enabledOffset;

    if (currentPresetDecodedData.getSize () <= enabledByteOffset)
        return;

    auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());

    if (data == nullptr)
        return;

    data[enabledByteOffset] = enabled ? 1 : 0;
    ++presetRevision;
}

void MidiConnection::updateCurrentPresetEffectId (int blockIndex, juce::uint32 effectId)
{
    const juce::ScopedLock lock (stateLock);
    if (blockIndex < 0 || blockIndex >= static_cast<int> (effectBlockCount))
        return;

    const auto base = effectBlockStart + static_cast<std::size_t> (blockIndex) * effectBlockSize;
    const auto effectIdByteOffset = base + effectIdOffset;
    const auto paramsByteOffset = base + paramsOffset;

    if (currentPresetDecodedData.getSize () < paramsByteOffset + effectParamCount * 4)
        return;

    auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());

    if (data == nullptr)
        return;

    data[effectIdByteOffset + 0] = static_cast<juce::uint8> (effectId & 0xFF);
    data[effectIdByteOffset + 1] = static_cast<juce::uint8> ((effectId >> 8) & 0xFF);
    data[effectIdByteOffset + 2] = static_cast<juce::uint8> ((effectId >> 16) & 0xFF);
    data[effectIdByteOffset + 3] = static_cast<juce::uint8> ((effectId >> 24) & 0xFF);

    EffectParameters defaultParams{};

    if (const auto* paramSet = GP200EffectParamDatabase::findParamsForEffect (effectId))
    {
        for (int i = 0; i < paramSet->count; ++i)
        {
            const auto& param = paramSet->params[i];

            if (param.idx >= 0 && param.idx < static_cast<int> (defaultParams.size ()))
                defaultParams[static_cast<std::size_t> (param.idx)] = param.defaultValue;
        }
    }

    for (std::size_t i = 0; i < defaultParams.size (); ++i)
    {
        const auto offset = paramsByteOffset + i * 4;
        const auto value = defaultParams[i];
        std::memcpy (data + offset, &value, sizeof (float));
    }

    ++presetRevision;
}

void MidiConnection::updateCurrentPresetEffectParam (int blockIndex, int paramIndex, float value)
{
    const juce::ScopedLock lock (stateLock);
    if (blockIndex < 0 || blockIndex >= static_cast<int> (effectBlockCount))
        return;

    if (paramIndex < 0 || paramIndex >= static_cast<int> (effectParamCount))
        return;

    const auto base = effectBlockStart + static_cast<std::size_t> (blockIndex) * effectBlockSize;
    const auto paramsByteOffset = base + paramsOffset;
    const auto valueOffset = paramsByteOffset + static_cast<std::size_t> (paramIndex) * 4;

    if (currentPresetDecodedData.getSize () < valueOffset + sizeof (float))
        return;

    auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());

    if (data == nullptr)
        return;

    std::memcpy (data + valueOffset, &value, sizeof (float));
    ++presetRevision;
}

void MidiConnection::updateCurrentPresetRoutingOrder (const RoutingOrder& routingOrder)
{
    const juce::ScopedLock lock (stateLock);
    if (currentPresetDecodedData.getSize () < routingOrderOffset + effectBlockCount)
        return;

    auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());

    if (data == nullptr)
        return;

    for (std::size_t i = 0; i < effectBlockCount; ++i)
        data[routingOrderOffset + i] = static_cast<juce::uint8> (routingOrder[i] & 0xFF);

    ++presetRevision;
}

int MidiConnection::getEffectOnOffCCForBlockIndex (int blockIndex)
{
    switch (blockIndex)
    {
    case 0:
        return 48; // PRE
    case 1:
        return 57; // WAH
    case 2:
        return 49; // DST
    case 3:
        return 50; // AMP
    case 4:
        return 51; // NR
    case 5:
        return 52; // CAB
    case 6:
        return 53; // EQ
    case 7:
        return 54; // MOD
    case 8:
        return 55; // DLY
    case 9:
        return 56; // RVB

    case 10:
        return -1; // VOL: not listed as ON/OFF CC in the manual

    default:
        return -1;
    }
}

bool MidiConnection::sendPresetChange (int slot)
{
    const juce::ScopedLock lock (stateLock);
    presetNameScanner.cancel ();
    if (slot < 0 || slot > 255)
    {
        lastMessageText = "Cannot restore preset: invalid slot";
        return false;
    }

    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot restore preset: MIDI output not open";
        return false;
    }

    const auto bytes = buildPresetChange (slot);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    currentSlot = slot;
    currentPresetName = "requesting...";
    presetNameRequestPending = true;
    lastRequestedNameSlot = -1;
    resetPresetDumpCaptureForSlot (slot);

    lastMessageText = "Sent DAW saved preset to GP-200: slot " + juce::String (slot);

    return true;
}

juce::String MidiConnection::getStatusText () const
{
    const juce::ScopedLock lock (stateLock);
    return statusText + " | " + modSyncStatus;
}

juce::String MidiConnection::getLastMessageText () const
{
    const juce::ScopedLock lock (stateLock);
    return lastMessageText;
}

int MidiConnection::getCurrentSlot () const
{
    const juce::ScopedLock lock (stateLock);
    return currentSlot;
}

juce::String MidiConnection::getCurrentSlotText () const
{
    const juce::ScopedLock lock (stateLock);
    if (currentSlot < 0)
        return "Current preset slot: unknown";

    const int bank = currentSlot / presetsPerBank + 1;
    const int slotInBank = currentSlot % presetsPerBank;

    const juce::String slotLetter = juce::String ("ABCD").substring (slotInBank, slotInBank + 1);

    return "Current preset slot: " + juce::String (currentSlot) + "  (" +
           juce::String (bank).paddedLeft ('0', 2) + "-" + slotLetter + ")";
}

juce::String MidiConnection::getCurrentPresetName () const
{
    const juce::ScopedLock lock (stateLock);
    return currentPresetName;
}

juce::String MidiConnection::getCurrentPresetNameText () const
{
    const juce::ScopedLock lock (stateLock);
    return "Current preset name: " + currentPresetName;
}

juce::String MidiConnection::getCurrentPresetDumpStatusText () const
{
    const juce::ScopedLock lock (stateLock);
    return currentPresetDumpStatusText;
}

int MidiConnection::getCurrentPresetDumpSize () const
{
    const juce::ScopedLock lock (stateLock);
    return static_cast<int> (currentPresetDecodedData.getSize ());
}

bool MidiConnection::hasLiveCurrentPresetData () const
{
    const juce::ScopedLock lock (stateLock);
    return currentPresetDataIsLive;
}

juce::MemoryBlock MidiConnection::getCurrentPresetDumpDataCopy () const
{
    const juce::ScopedLock lock (stateLock);
    return currentPresetDecodedData;
}

std::uint64_t MidiConnection::getPresetRevision () const
{
    const juce::ScopedLock lock (stateLock);
    return presetRevision;
}

std::uint64_t MidiConnection::getAssignmentNamesRevision () const
{
    const juce::ScopedLock lock (stateLock);
    return assignmentNamesRevision;
}

void MidiConnection::adoptCurrentPresetSnapshot (int slot,
                                                 const juce::String& presetName,
                                                 const juce::MemoryBlock& presetData)
{
    const juce::ScopedLock lock (stateLock);
    if (slot >= 0 && slot <= 255)
        currentSlot = slot;

    auto safeName = sanitizePresetNameForStore (presetName);

    if (safeName.isEmpty () && presetData.getSize () >= 44)
    {
        const auto* data = static_cast<const juce::uint8*> (presetData.getData ());

        if (data != nullptr)
        {
            juce::String nameFromData;

            for (int i = 0; i < presetNameMaxLength; ++i)
            {
                const auto b = data[presetNameOffset + static_cast<std::size_t> (i)];

                if (b == 0)
                    break;

                if (b >= 32 && b <= 126)
                    nameFromData += juce::String::charToString (static_cast<juce_wchar> (b));
            }

            safeName = sanitizePresetNameForStore (nameFromData);
        }
    }

    currentPresetName = safeName.isNotEmpty () ? safeName : "unknown";

    presetNameRequestPending = false;
    livePresetReadPending = false;
    liveRefreshPending = false;
    liveRefreshDueMs = 0.0;

    currentStateRequestQueued = false;
    currentStateRequestPending = false;

    if (presetRestoreTransactionActive)
        startupHandshakePhase = StartupHandshakePhase::Ready;

    lastRequestedNameSlot = currentSlot;
    presetDumpSlot = -1;
    presetReadChunks.clear ();

    currentPresetDecodedData = presetData;

    // The snapshot has just been applied completely to the current GP-200
    // edit buffer by Recall from DAW. Treat it as the current live state so
    // the startup retry timer does not immediately request another dump and
    // overwrite the state that has just been restored.
    currentPresetDataIsLive = true;
    ++presetRevision;

    currentPresetDumpStatusText = "Current full preset data: restored snapshot, " +
                                  juce::String (static_cast<int> (currentPresetDecodedData.getSize ())) +
                                  " bytes";

    lastMessageText = "Loaded DAW preset snapshot into editor: slot " + juce::String (currentSlot) + " / " +
                      currentPresetName;
}

void MidiConnection::beginPresetRestoreTransaction ()
{
    const juce::ScopedLock lock (stateLock);

    presetRestoreTransactionActive = true;
    currentStateRequestQueued = false;
    currentStateRequestPending = false;
    stateDumpChunks.clear ();
    liveRefreshPending = false;
    liveRefreshDueMs = 0.0;
    livePresetReadPending = false;
    presetNameRequestPending = false;
    lastRequestedNameSlot = -1;
    presetDumpSlot = -1;
    presetReadChunks.clear ();
}

void MidiConnection::endPresetRestoreTransaction ()
{
    const juce::ScopedLock lock (stateLock);

    currentStateRequestQueued = false;
    currentStateRequestPending = false;
    stateDumpChunks.clear ();
    liveRefreshPending = false;
    liveRefreshDueMs = 0.0;
    livePresetReadPending = false;
    presetNameRequestPending = false;
    presetDumpSlot = -1;
    presetReadChunks.clear ();
    presetRestoreTransactionActive = false;
}

bool MidiConnection::requestPresetNameForCurrentSlotIfNeeded ()
{
    const juce::ScopedLock lock (stateLock);
    if (modSyncActive || presetRestoreTransactionActive || !presetNameRequestPending)
        return false;

    if (currentSlot < 0)
        return false;

    if (lastRequestedNameSlot == currentSlot)
        return false;

    if (livePresetReadPending)
        return sendLiveReadRequestForSlot (currentSlot);

    return sendReadRequestForSlot (currentSlot);
}


void MidiConnection::startPresetNameScan (int prioritySlot)
{
    const juce::ScopedLock lock (stateLock);

    if (midiOutput == nullptr)
        return;

    presetNameScanner.start (prioritySlot);
}

void MidiConnection::cancelPresetNameScan ()
{
    const juce::ScopedLock lock (stateLock);
    presetNameScanner.cancel ();
}

void MidiConnection::processPresetNameScan ()
{
    const juce::ScopedLock lock (stateLock);

    if (!presetNameScanner.isScanning ())
        return;

    const auto nowMs = juce::Time::getMillisecondCounterHiRes ();

    // Even while the startup query is queued, allow the scanner's single
    // in-flight request to expire. Otherwise a lost scanner reply would keep
    // the live-state query paused indefinitely.
    presetNameScanner.handleTimeout (nowMs);

    const bool busy = modSyncActive || midiOutput == nullptr
                      || presetRestoreTransactionActive
                      || irUploadPhase != IRUploadPhase::Idle
                      || soundCloneUploadPhase != SoundCloneUploadPhase::Idle
                      || currentStateRequestQueued
                      || currentStateRequestPending
                      || presetNameRequestPending
                      || livePresetReadPending
                      || liveRefreshPending
                      || presetDumpSlot >= 0;

    if (busy)
        return;

    sendNextPresetNameScanRequestUnlocked ();
}

bool MidiConnection::sendNextPresetNameScanRequestUnlocked ()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes ();

    const bool presetTrafficBusy =
        currentStateRequestQueued
        || currentStateRequestPending
        || presetNameRequestPending
        || livePresetReadPending
        || liveRefreshPending
        || presetDumpSlot >= 0;

    if (modSyncActive || midiOutput == nullptr
        || presetTrafficBusy
        || !presetNameScanner.shouldSendNextRequest (nowMs))
    {
        return false;
    }

    const auto bytes = presetNameScanner.beginNextRequest (nowMs);
    if (bytes.size () < 3)
        return false;

    const auto message = juce::MidiMessage::createSysExMessage (
        bytes.data () + 1, static_cast<int> (bytes.size () - 2));
    midiOutput->sendMessageNow (message);
    return true;
}

bool MidiConnection::isPresetNameScanRunning () const
{
    const juce::ScopedLock lock (stateLock);
    return presetNameScanner.isScanning ();
}

float MidiConnection::getPresetNameScanProgress () const
{
    const juce::ScopedLock lock (stateLock);
    return presetNameScanner.getProgress ();
}

std::uint64_t MidiConnection::getPresetNameScanRevision () const
{
    const juce::ScopedLock lock (stateLock);
    return presetNameScanner.getRevision ();
}

juce::String MidiConnection::getPresetSlotName (int slot) const
{
    const juce::ScopedLock lock (stateLock);
    return presetNameScanner.getName (slot);
}

bool MidiConnection::hasPresetSlotName (int slot) const
{
    const juce::ScopedLock lock (stateLock);
    return presetNameScanner.hasName (slot);
}

bool MidiConnection::hasCompletePresetNameCache () const
{
    const juce::ScopedLock lock (stateLock);
    return presetNameScanner.isCacheComplete ();
}

void MidiConnection::updatePresetNameCache (int slot, const juce::String& name)
{
    const juce::ScopedLock lock (stateLock);
    presetNameScanner.setCachedName (slot, name);
}

void MidiConnection::invalidatePresetNameCacheSlot (int slot)
{
    const juce::ScopedLock lock (stateLock);
    presetNameScanner.invalidateSlot (slot);
}

bool MidiConnection::sendReadRequestForSlot (int slot)
{
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot request preset data: MIDI output not open";
        return false;
    }

    const auto bytes = buildReadRequest (slot);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    lastRequestedNameSlot = slot;
    resetPresetDumpCaptureForSlot (slot);

    lastMessageText = "Requested full preset data for slot " + juce::String (slot);

    return true;
}

bool MidiConnection::sendLiveReadRequestForSlot (int slot)
{
    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot request live preset data: MIDI output not open";
        return false;
    }

    const auto bytes = buildLiveReadRequest (slot);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    livePresetReadPending = false;
    lastRequestedNameSlot = slot;
    resetPresetDumpCaptureForSlot (slot);

    lastMessageText = "Requested live edit buffer for slot " + juce::String (slot);

    return true;
}

void MidiConnection::scheduleLivePresetRefresh ()
{
    constexpr double liveRefreshDebounceMs = 120.0;

    liveRefreshPending = true;
    liveRefreshDueMs =
        juce::Time::getMillisecondCounterHiRes () + liveRefreshDebounceMs;
}

void MidiConnection::resetPresetDumpCaptureForSlot (int slot)
{
    presetDumpSlot = slot;
    presetReadChunks.clear ();
    currentPresetDecodedData.setSize (0);
    currentPresetDataIsLive = false;
    ++presetRevision;

    currentPresetDumpStatusText = "Current full preset data: requesting slot " + juce::String (slot);
}

void MidiConnection::collectPresetReadChunk (const juce::uint8* data, int size)
{
    if (presetDumpSlot < 0)
        return;

    // Real preset read chunks are large. This avoids confusing small
    // real-time parameter messages, which also use CMD=0x12 / SUB=0x18.
    if (size < 100)
        return;

    const int offset = getChunkOffset (data, size);

    if (offset < 0)
        return;

    for (const auto& existingChunk : presetReadChunks)
    {
        if (getChunkOffset (existingChunk.data (), static_cast<int> (existingChunk.size ())) == offset)
            return;
    }

    presetReadChunks.emplace_back (data, data + size);

    currentPresetDumpStatusText = "Current full preset data: receiving " +
                                  juce::String (static_cast<int> (presetReadChunks.size ())) + "/7 chunks";

    if (presetReadChunks.size () >= 7)
    {
        currentPresetDecodedData = assemblePresetReadChunks (presetReadChunks);
        currentPresetDataIsLive = currentPresetDecodedData.getSize () > 0;
        ++presetRevision;

        currentPresetDumpStatusText = "Current full preset data: captured, " +
                                      juce::String (static_cast<int> (currentPresetDecodedData.getSize ())) +
                                      " bytes";

        // A complete reply for the active slot completes the name request too.
        // Do not leave MOD_SYNC blocked on optional first-chunk name extraction.
        if (currentPresetDataIsLive && presetDumpSlot == currentSlot)
        {
            presetNameRequestPending = false;
            livePresetReadPending = false;
        }
        presetDumpSlot = -1;

        if (startupHandshakePhase == StartupHandshakePhase::WaitingForCurrentPreset)
            startupHandshakePhase = StartupHandshakePhase::Ready;
    }
}

int MidiConnection::getChunkOffset (const juce::uint8* data, int size)
{
    if (data == nullptr || size <= 12)
        return -1;

    return data[11] | (data[12] << 7);
}

std::vector<juce::uint8> MidiConnection::buildIdentityQuery ()
{
    return {0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32, 0x11, 0x04, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7};
}

std::vector<juce::uint8> MidiConnection::buildEnterEditorMode ()
{
    return {0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32, 0x11, 0x12, 0x00,
            0x00, 0x00, 0xF7};
}

std::vector<juce::uint8> MidiConnection::buildStateDumpRequest ()
{
    return {0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32, 0x11, 0x04, 0x00,
            0x00, 0x00, 0x00, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7};
}

std::vector<juce::uint8> MidiConnection::buildAssignmentNameQuery (int section, int page, int block)
{
    const std::vector<juce::uint8> section0Header{0x00, 0x00, 0x00, 0x00, 0x09, 0x01, 0x00, 0x01, 0x08};

    const std::vector<juce::uint8> section1Header{0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x00, 0x01, 0x08};

    const auto& sectionHeader = section == 1 ? section1Header : section0Header;

    const std::vector<juce::uint8> referenceData{
        0x01, 0x00, 0x00, 0x0C, 0x0E, 0x07, 0x03, 0x0B, 0x02, 0x00, 0x00, 0x07, 0x02, 0x04, 0x0F,
        0x06, 0x05, 0x00, 0x09, 0x00, 0x0C, 0x0F, 0x0E, 0x0D, 0x0A, 0x00, 0x0B, 0x09, 0x08, 0x07,
        0x05, 0x0E, 0x08, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    std::vector<juce::uint8> message;

    message.reserve (70);

    message.push_back (0xF0);
    message.push_back (0x21);
    message.push_back (0x25);
    message.push_back (0x7E);
    message.push_back (0x47);
    message.push_back (0x50);
    message.push_back (0x2D);
    message.push_back (0x32);
    message.push_back (0x11);
    message.push_back (0x1C);

    message.insert (message.end (), sectionHeader.begin (), sectionHeader.end ());

    message.push_back (0x00);
    message.push_back (0x00);
    message.push_back (static_cast<juce::uint8> (page & 0xFF));
    message.push_back (static_cast<juce::uint8> (block & 0x0F));
    message.push_back (0x00);
    message.push_back (0x00);
    message.push_back (0x00);

    message.insert (message.end (), referenceData.begin (), referenceData.end ());

    message.push_back (0xF7);

    return message;
}
std::vector<juce::uint8> MidiConnection::buildPresetChange (int slot)
{
    const auto sh = static_cast<juce::uint8> ((slot >> 4) & 0x0F);
    const auto sl = static_cast<juce::uint8> (slot & 0x0F);

    return {0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32, 0x12, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08,
            0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, sh,   sl,   0x00, 0x00, 0xF7};
}

std::vector<juce::uint8> MidiConnection::buildAutoCabMatch (bool shouldBeEnabled)
{
    return {0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32,
            0x12, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01,
            0x00, 0x00, 0x04, 0x00, 0x00, 0x02, 0x04, 0x00,
            0x00, 0x00, static_cast<juce::uint8> (shouldBeEnabled ? 0x01 : 0x00),
            0x00, 0x00, 0xF7};
}

std::vector<juce::uint8> MidiConnection::buildEffectChange (int blockIndex, juce::uint32 effectId)
{
    const auto moduleType = static_cast<juce::uint8> ((effectId >> 24) & 0xFF);
    const auto subCategory = static_cast<juce::uint8> ((effectId >> 16) & 0xFF);
    const auto variant = static_cast<juce::uint8> (effectId & 0xFF);

    return {0xF0,
            0x21,
            0x25,
            0x7E,
            0x47,
            0x50,
            0x2D,
            0x32,
            0x12,
            0x14,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x04,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x01,
            0x06,
            0x00,
            0x00,
            0x00,
            0x08,
            0x00,
            0x00,
            0x00,
            static_cast<juce::uint8> (blockIndex & 0x0F),
            0x00,
            0x00,
            0x07,
            0x06,
            0x00,
            0x02,
            static_cast<juce::uint8> ((variant >> 4) & 0x0F),
            static_cast<juce::uint8> (variant & 0x0F),
            0x00,
            0x00,
            static_cast<juce::uint8> ((subCategory >> 4) & 0x0F),
            static_cast<juce::uint8> (subCategory & 0x0F),
            0x00,
            moduleType,
            0xF7};
}

std::vector<juce::uint8> MidiConnection::buildPatchSetting (int target, int value)
{
    const auto safeTarget = juce::jlimit (0, 15, target);
    const auto safeValue = juce::jlimit (0, 511, value);

    std::vector<juce::uint8> message{0xF0,
                                     0x21,
                                     0x25,
                                     0x7E,
                                     0x47,
                                     0x50,
                                     0x2D,
                                     0x32,
                                     0x12,
                                     0x10,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x04,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x06,
                                     0x00,
                                     0x00,
                                     0x00,
                                     0x04,
                                     0x00,
                                     0x00,
                                     0x00,
                                     static_cast<juce::uint8> (safeTarget & 0x0F),
                                     0x00,
                                     0x00,
                                     static_cast<juce::uint8> ((safeValue >> 4) & 0x0F),
                                     static_cast<juce::uint8> (safeValue & 0x0F),
                                     0x00,
                                     0x00,
                                     0xF7};

    if (safeTarget == 0x06 && safeValue > 127)
    {
        message[43] = 0x0F;
        message[44] = 0x0F;
    }

    return message;
}

std::vector<juce::uint8>
MidiConnection::buildParamChange (int blockIndex, int paramIndex, juce::uint32 effectId, float value)
{
    juce::uint8 decoded[24]{};

    decoded[2] = 0x04;
    decoded[8] = 0x05;
    decoded[10] = 0x0C;
    decoded[12] = static_cast<juce::uint8> (blockIndex & 0x0F);
    decoded[13] = static_cast<juce::uint8> (paramIndex & 0x0F);

    const auto displayValue = encodeDisplayValue (value);
    decoded[14] = displayValue[0];
    decoded[15] = displayValue[1];

    decoded[16] = static_cast<juce::uint8> (effectId & 0xFF);
    decoded[17] = static_cast<juce::uint8> ((effectId >> 8) & 0xFF);
    decoded[18] = static_cast<juce::uint8> ((effectId >> 16) & 0xFF);
    decoded[19] = static_cast<juce::uint8> ((effectId >> 24) & 0xFF);

    std::memcpy (decoded + 20, &value, sizeof (float));

    const auto nibbles = nibbleEncode (decoded, 24);

    std::vector<juce::uint8> message;
    message.reserve (62);

    message.push_back (0xF0);
    message.push_back (0x21);
    message.push_back (0x25);
    message.push_back (0x7E);
    message.push_back (0x47);
    message.push_back (0x50);
    message.push_back (0x2D);
    message.push_back (0x32);
    message.push_back (0x12);
    message.push_back (0x18);
    message.push_back (0x00);
    message.push_back (0x00);
    message.push_back (0x00);

    message.insert (message.end (), nibbles.begin (), nibbles.end ());

    message.push_back (0xF7);

    return message;
}

std::vector<juce::uint8>
MidiConnection::buildReorderEffects (const RoutingOrder& routingOrder, int fxLoopSend, int fxLoopReturn)
{
    juce::uint8 decoded[32]{};

    decoded[2] = 0x04;
    decoded[8] = 0x08;
    decoded[10] = 0x10;
    decoded[14] = static_cast<juce::uint8> (fxLoopSend & 0xFF);
    decoded[15] = static_cast<juce::uint8> (fxLoopReturn & 0xFF);

    for (int i = 0; i < 11; ++i)
        decoded[16 + i] = static_cast<juce::uint8> (routingOrder[static_cast<std::size_t> (i)] & 0xFF);

    decoded[27] = 0x44;

    const auto nibbles = nibbleEncode (decoded, 32);

    std::vector<juce::uint8> message;
    message.reserve (78);

    message.push_back (0xF0);
    message.push_back (0x21);
    message.push_back (0x25);
    message.push_back (0x7E);
    message.push_back (0x47);
    message.push_back (0x50);
    message.push_back (0x2D);
    message.push_back (0x32);
    message.push_back (0x12);
    message.push_back (0x20);
    message.push_back (0x00);
    message.push_back (0x00);
    message.push_back (0x00);

    message.insert (message.end (), nibbles.begin (), nibbles.end ());
    message.push_back (0xF7);

    return message;
}

std::vector<juce::uint8> MidiConnection::buildReadRequest (int slot)
{
    const auto sh = static_cast<juce::uint8> ((slot >> 4) & 0x0F);
    const auto sl = static_cast<juce::uint8> (slot & 0x0F);

    return {0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, sh,   sl,   0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x04, 0x00, 0x00, sh,   sl,   0x00, 0x00, sh,   sl,   0x00, 0x00, 0xF7};
}

std::vector<juce::uint8> MidiConnection::buildLiveReadRequest (int slot)
{
    const auto sh = static_cast<juce::uint8> ((slot >> 4) & 0x0F);
    const auto sl = static_cast<juce::uint8> (slot & 0x0F);

    // Exact request observed from the official editor after its complete
    // preset-library scan. For captured slot 179, sh/sl were 0x0B/0x03:
    // ... 04 00 00 00 00 00 00 0B 03 00 00 00 01 00 00 00
    // ... 04 00 00 0F 0F 0F 0F 0B 03 00 00
    return {0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32,
            0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, sh,   sl,   0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x04, 0x00, 0x00, 0x0F, 0x0F, 0x0F,
            0x0F, sh,   sl,   0x00, 0x00, 0xF7};
}

std::vector<juce::uint8> MidiConnection::buildStorePresetCommit (int slot, const juce::String& presetName)
{
    juce::uint8 decoded[24]{};

    decoded[0] = 0x03;
    decoded[1] = 0x20;
    decoded[2] = 0x14;

    // Important: this is the sub-slot inside the current bank:
    // A=0, B=1, C=2, D=3.
    decoded[4] = static_cast<juce::uint8> (slot);

    const auto safeName = sanitizePresetNameForStore (presetName);

    for (int i = 0; i < 16 && i < safeName.length (); ++i)
    {
        const auto c = safeName[i];

        if (c >= 32 && c <= 126)
            decoded[8 + i] = static_cast<juce::uint8> (c);
        else
            decoded[8 + i] = static_cast<juce::uint8> (' ');
    }

    const auto nibbles = nibbleEncode (decoded, 24);

    std::vector<juce::uint8> message;

    message.reserve (62);

    message.push_back (0xF0);
    message.push_back (0x21);
    message.push_back (0x25);
    message.push_back (0x7E);
    message.push_back (0x47);
    message.push_back (0x50);
    message.push_back (0x2D);
    message.push_back (0x32);
    message.push_back (0x12);
    message.push_back (0x18);
    message.push_back (0x00);
    message.push_back (0x00);
    message.push_back (0x00);

    message.insert (message.end (), nibbles.begin (), nibbles.end ());

    message.push_back (0xF7);

    return message;
}

std::vector<juce::uint8> MidiConnection::buildRenameSnapTone (int globalSlot, const juce::String& newName)
{
    juce::uint8 decoded[24]{};
    decoded[0] = 0x15;
    decoded[1] = 0x10;
    decoded[2] = 0x14;
    decoded[4] = static_cast<juce::uint8> (globalSlot & 0xFF);

    const auto safeName = newName.trim ().substring (0, 16);
    for (int i = 0; i < safeName.length () && i < 16; ++i)
    {
        const auto c = safeName[i];
        decoded[8 + i] = static_cast<juce::uint8> (c >= 32 && c <= 126 ? c : ' ');
    }

    const auto nibbles = nibbleEncode (decoded, 24);
    std::vector<juce::uint8> message;
    message.reserve (62);
    message.insert (message.end (), { 0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32,
                                     0x12, 0x18, 0x00, 0x00, 0x00 });
    message.insert (message.end (), nibbles.begin (), nibbles.end ());
    message.push_back (0xF7);
    return message;
}

std::vector<juce::uint8> MidiConnection::buildRenameUserIR (int zeroBasedIndex, const juce::String& newName)
{
    juce::uint8 decoded[24]{};
    decoded[0] = 0x0C;
    decoded[1] = 0x10;
    decoded[2] = 0x14;
    decoded[4] = static_cast<juce::uint8> (zeroBasedIndex & 0xFF);

    const auto safeName = newName.trim ().substring (0, 16);
    for (int i = 0; i < safeName.length () && i < 16; ++i)
    {
        const auto c = safeName[i];
        decoded[8 + i] = static_cast<juce::uint8> (c >= 32 && c <= 126 ? c : ' ');
    }

    const auto nibbles = nibbleEncode (decoded, 24);
    std::vector<juce::uint8> message;
    message.reserve (62);
    message.insert (message.end (), { 0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32,
                                     0x12, 0x18, 0x00, 0x00, 0x00 });
    message.insert (message.end (), nibbles.begin (), nibbles.end ());
    message.push_back (0xF7);
    return message;
}

std::vector<juce::uint8> MidiConnection::nibbleEncode (const juce::uint8* data, int size)
{
    std::vector<juce::uint8> encoded;

    if (data == nullptr || size <= 0)
        return encoded;

    encoded.reserve (static_cast<std::size_t> (size * 2));

    for (int i = 0; i < size; ++i)
    {
        encoded.push_back (static_cast<juce::uint8> ((data[i] >> 4) & 0x0F));
        encoded.push_back (static_cast<juce::uint8> (data[i] & 0x0F));
    }

    return encoded;
}

std::array<juce::uint8, 2> MidiConnection::encodeDisplayValue (float value)
{
    int encodedValue = 0;

    if (std::isfinite (value) && value > 0.0f)
    {
        encodedValue = static_cast<int> (std::round (16367.0f + 16.0f * std::log2 (value)));
        encodedValue = juce::jlimit (0, 0xFFFF, encodedValue);
    }

    return {static_cast<juce::uint8> (encodedValue & 0xFF),
            static_cast<juce::uint8> ((encodedValue >> 8) & 0xFF)};
}

juce::String MidiConnection::parseAssignmentNameResponse (const juce::uint8* data, int size)
{
    if (data == nullptr || size <= 28)
        return {};

    const auto decoded = nibbleDecode (data + 27, size - 28);

    int nameStart = 0;

    while (nameStart < static_cast<int> (decoded.size ()) &&
           decoded[static_cast<std::size_t> (nameStart)] == 0)
    {
        ++nameStart;
    }

    juce::String name;

    for (int i = nameStart; i < static_cast<int> (decoded.size ()); ++i)
    {
        const auto b = decoded[static_cast<std::size_t> (i)];

        if (b == 0)
            break;

        if (b >= 32 && b <= 126)
            name += juce::String::charToString (static_cast<juce_wchar> (b));
    }

    return cleanAssignmentName (name);
}

juce::String MidiConnection::cleanAssignmentName (const juce::String& name)
{
    auto cleaned = name.trim ();

    if (cleaned == "User IR" || cleaned == "SnapTone" || cleaned == "Empty")
    {
        return {};
    }

    return cleaned.substring (0, 32);
}

juce::String MidiConnection::sanitizePresetNameForStore (const juce::String& presetName)
{
    const auto name = presetName.trim ();

    if (name.isEmpty () || name == "unknown" || name == "requesting...")
    {
        return {};
    }

    return name.substring (0, presetNameMaxLength);
}

void MidiConnection::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    const juce::ScopedLock lock (stateLock);
    if (message.isSysEx ())
    {
        handleIncomingSysEx (message);
        return;
    }

    lastMessageText = "Received non-SysEx MIDI message";
}

void MidiConnection::handleIncomingSysEx (const juce::MidiMessage& message)
{
    const auto* sysExData = message.getSysExData ();
    const auto sysExSize = message.getSysExDataSize ();

    std::vector<juce::uint8> fullMessage;
    fullMessage.reserve (static_cast<std::size_t> (sysExSize + 2));

    fullMessage.push_back (0xF0);

    for (int i = 0; i < sysExSize; ++i)
        fullMessage.push_back (sysExData[i]);

    fullMessage.push_back (0xF7);

    juce::String hex;
    hex << "Received SysEx: F0 ";

    const int bytesToShow = juce::jmin (sysExSize, 20);

    for (int i = 0; i < bytesToShow; ++i)
    {
        hex << juce::String::toHexString (static_cast<int> (sysExData[i])).paddedLeft ('0', 2).toUpperCase ()
            << " ";
    }

    if (sysExSize > bytesToShow)
        hex << "... ";

    hex << "F7";

    lastMessageText = hex;

    parseGP200SysEx (fullMessage.data (), static_cast<int> (fullMessage.size ()));
}

void MidiConnection::parseGP200SysEx (const juce::uint8* data, int size)
{
    if (size < 15)
        return;

    const bool isGP200Header = data[0] == 0xF0 && data[1] == 0x21 && data[2] == 0x25 && data[3] == 0x7E &&
                               data[4] == 0x47 && data[5] == 0x50 && data[6] == 0x2D && data[7] == 0x32;

    if (!isGP200Header)
        return;

    if (handleModSyncResponse (data, size)) return;

    const auto command = data[8];
    const auto subCommand = data[9];

    if (command == 0x12 && subCommand == 0x0c && handleSoundCloneUploadAck (data, size))
        return;

    if (command == 0x12 && subCommand == 0x1C)
    {
        handleAssignmentNameResponse (data, size);
        return;
    }

    // During the first connection handshake, the first 0x12/0x08 response is
    // the answer to the official editor's identity query. Consume it before
    // the generic 0x12/0x08 notification parser so it can never be mistaken
    // for a preset change or a global-setting echo.
    if (command == 0x12 && subCommand == 0x08
        && startupHandshakePhase == StartupHandshakePhase::WaitingForIdentity)
    {
        sendEnterEditorModeUnlocked ();
        return;
    }

    // GP-200 spontaneous notifications.
    //
    // data[14] == 0x08 identifies a real preset-slot change.
    // The official editor capture also shows 0x12/0x08 messages with other
    // values (for example 0x05) around block bypass and effect-model changes.
    // Those messages are edit notifications, not preset changes, so they must
    // trigger the same debounced live-buffer refresh as 0x12/0x10.
    const auto decodePresetSlotFromNotification = [data, size] () -> int
    {
        if (size < 28)
            return -1;

        const int slot = ((static_cast<int> (data[25]) & 0x0F) << 4)
                       |  (static_cast<int> (data[26]) & 0x0F);

        return slot >= 0 && slot < 256 ? slot : -1;
    };

    if (command == 0x12 && subCommand == 0x08 && size >= 28)
    {
        // The GP-200 echoes/confirms the AUTO CAB global-setting command using
        // the same 0x12/0x08 command family as preset-change notifications.
        // Its payload has 0x02, 0x04 at bytes 21-22 and the enabled state at
        // byte 26. Never decode that state byte as a preset slot (which would
        // otherwise turn AUTO CAB ON into slot 01-B).
        const bool isAutoCabReply = size >= 30
            && data[14] == 0x08
            && data[15] == 0x01
            && data[18] == 0x04
            && data[21] == 0x02
            && data[22] == 0x04
            && data[25] == 0x00
            && (data[26] == 0x00 || data[26] == 0x01)
            && data[27] == 0x00
            && data[28] == 0x00;

        if (isAutoCabReply)
        {
            lastMessageText = data[26] != 0 ? "AUTO CAB enabled" : "AUTO CAB disabled";
            return;
        }

        if (presetRestoreTransactionActive)
            return;

        if (data[14] == 0x08)
        {
            const int slot = decodePresetSlotFromNotification ();

            if (slot >= 0 && slot < 256)
            {
                if (slot != currentSlot)
                {
                    currentSlot = slot;
                    currentPresetName = "requesting...";
                    presetNameRequestPending = true;
                    livePresetReadPending = false;
                    liveRefreshPending = false;
                    lastRequestedNameSlot = -1;
                    resetPresetDumpCaptureForSlot (slot);
                }
                else
                {
                    // A same-slot 0x12/0x08 notification can correspond to a
                    // structural edit in the currently active preset.
                    scheduleLivePresetRefresh ();
                }

                lastMessageText = "GP-200 preset changed: " + getCurrentSlotText ();
            }
        }
        else
        {
            // Block on/off and effect-model changes observed in gpcambios.pcapng.
            scheduleLivePresetRefresh ();
        }

        return;
    }

    // GP-200 real-time edit notification. The official editor receives
    // these 0x12/0x10 messages when a block or parameter is changed directly
    // on the pedal. Instead of maintaining a second incremental decoder, we
    // debounce the notifications and request the complete live edit buffer,
    // which is then handled by the existing seven-chunk preset decoder.
    if (command == 0x12 && subCommand == 0x10)
    {
        scheduleLivePresetRefresh ();
        return;
    }

    // The optional preset-name scanner uses the dedicated 0x20 request and
    // owns exactly one pending reply at a time. Consume that reply here so it
    // cannot be mistaken for the currently loaded preset.
    if (command == 0x12 && subCommand == 0x18
        && presetNameScanner.hasPendingRequest ()
        && presetNameScanner.handleSysEx (
            data,
            size,
            juce::Time::getMillisecondCounterHiRes ()))
    {
        // Do not chain another scan request from inside the MIDI callback.
        // A CAB/effect notification may already have scheduled a high-priority
        // live preset refresh. processPresetNameScan() will resume the scan
        // later, once the live read and all other preset traffic have settled.
        return;
    }

    if (command == 0x12 && subCommand == 0x18 && presetRestoreTransactionActive)
        return;

    // Response to preset read request. We use the same 7 chunks for:
    // - preset name
    // - full decoded preset dump capture
    if (command == 0x12 && subCommand == 0x18 && size > 14)
    {
        const int offset = data[11] | (data[12] << 7);

        if (offset == 0)
        {
            const auto name = extractPresetNameFromReadChunk (data, size);

            if (name.isNotEmpty ())
            {
                // A preset-read response identifies the slot that was requested,
                // but it must never change the physically active GP-200 slot.
                // Delayed replies can arrive after the active slot has changed.
                const int responseSlot = lastRequestedNameSlot;

                if (responseSlot >= 0)
                    presetNameScanner.setCachedName (responseSlot, name);

                // Only update the header when this reply still belongs to the
                // currently active preset. A stale reply must not replace either
                // the visible name or the pending state for the current slot.
                if (responseSlot == currentSlot)
                {
                    currentPresetName = name;
                    presetNameRequestPending = false;
                }

                lastMessageText = "Preset name received: " + name + " for slot " + juce::String (responseSlot);
            }
        }

        collectPresetReadChunk (data, size);

        return;
    }

    // Response to the current-state query. The official GP-200 editor sends
    // five 0x12/0x4E chunks (offsets 0,185,370,555,740) and only treats the
    // state as complete after all five have arrived.
    if (command == 0x12 && subCommand == 0x4E && size >= 28)
    {
        if (!presetRestoreTransactionActive)
            collectStateDumpChunk (data, size);
        return;
    }

}

void MidiConnection::handleAssignmentNameResponse (const juce::uint8* data, int size)
{
    if (!assignmentNameRequestInProgress)
        return;

    const auto query = currentAssignmentNameQuery;
    const auto name = parseAssignmentNameResponse (data, size);

    if (query.section == 0)
    {
        const int index = query.page * 16 + query.block;

        if (index >= 0 && index < static_cast<int> (userIRNames.size ()))
            userIRNames[static_cast<std::size_t> (index)] = name;
    }
    else if (query.section == 1)
    {
        const int index = query.block;

        if (index >= 0 && index < static_cast<int> (snapToneNames.size ()))
            snapToneNames[static_cast<std::size_t> (index)] = name;
    }

    ++assignmentNamesRevision;
    assignmentNameRequestInProgress = false;

    if (name.isNotEmpty ())
        lastMessageText = "Assignment name received: " + name;
    else
        lastMessageText = "Assignment name received: empty";

    sendNextAssignmentNameQuery ();
}

std::vector<juce::uint8> MidiConnection::nibbleDecode (const juce::uint8* data, int size)
{
    std::vector<juce::uint8> decoded;

    if (size <= 1)
        return decoded;

    decoded.reserve (static_cast<std::size_t> (size / 2));

    for (int i = 0; i + 1 < size; i += 2)
    {
        const auto value = static_cast<juce::uint8> (((data[i] & 0x0F) << 4) | (data[i + 1] & 0x0F));

        decoded.push_back (value);
    }

    return decoded;
}

juce::MemoryBlock
MidiConnection::assemblePresetReadChunks (const std::vector<std::vector<juce::uint8>>& chunks)
{
    auto sortedChunks = chunks;

    std::sort (sortedChunks.begin (),
               sortedChunks.end (),
               [] (const auto& a, const auto& b)
               {
                   return getChunkOffset (a.data (), static_cast<int> (a.size ())) <
                          getChunkOffset (b.data (), static_cast<int> (b.size ()));
               });

    std::vector<juce::uint8> allNibbles;

    for (const auto& chunk : sortedChunks)
    {
        if (chunk.size () <= 14)
            continue;

        const auto* nibbleStart = chunk.data () + 13;
        const auto nibbleSize = static_cast<int> (chunk.size ()) - 14;

        allNibbles.insert (allNibbles.end (), nibbleStart, nibbleStart + nibbleSize);
    }

    const auto decoded = nibbleDecode (allNibbles.data (), static_cast<int> (allNibbles.size ()));

    juce::MemoryBlock result;

    if (!decoded.empty ())
        result.append (decoded.data (), decoded.size ());

    return result;
}

juce::String MidiConnection::extractPresetNameFromDecodedPresetData (const std::vector<juce::uint8>& decoded)
{
    if (decoded.size () < 44)
        return {};

    juce::String name;

    for (int i = 0; i < 16; ++i)
    {
        const auto b = decoded[28 + i];

        if (b == 0)
            break;

        name += juce::String::charToString (static_cast<juce_wchar> (b));
    }

    return name.trim ();
}

juce::String MidiConnection::extractPresetNameFromReadChunk (const juce::uint8* data, int size)
{
    if (size <= 14)
        return {};

    const auto* nibbleData = data + 13;
    const int nibbleSize = size - 14;

    const auto decoded = nibbleDecode (nibbleData, nibbleSize);

    return extractPresetNameFromDecodedPresetData (decoded);
}

} // namespace gp200