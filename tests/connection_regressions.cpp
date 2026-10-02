#include "../source/libgp200/GP200FlexibleRouting.h"
#include "../source/libgp200/GP200Constants.h"
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <mutex>
#include <thread>
namespace juce {
using uint8=std::uint8_t;
template<class T>T jlimit(T a,T b,T v){return std::clamp(v,a,b);}
struct String:std::string {using std::string::string;using std::string::operator=;String(int n):std::string(std::to_string(n)){}String(std::string s):std::string(s){}bool isEmpty()const{return empty();}};
struct MemoryBlock {std::vector<uint8> v;void setSize(size_t n){v.resize(n);}size_t getSize()const{return v.size();}void* getData(){return v.data();}const void* getData()const{return v.data();}void append(const void*p,size_t n){auto b=(const uint8*)p;v.insert(v.end(),b,b+n);}};
struct CriticalSection {mutable std::recursive_mutex m;};struct ScopedLock{const CriticalSection& c;ScopedLock(const CriticalSection& x):c(x){c.m.lock();}~ScopedLock(){c.m.unlock();}};
struct Time {static inline double now=0;static double getMillisecondCounterHiRes(){return now;}};
struct MidiMessage {std::vector<uint8> bytes;static MidiMessage createSysExMessage(const uint8*p,int n){MidiMessage m;m.bytes.push_back(0xf0);m.bytes.insert(m.bytes.end(),p,p+n);m.bytes.push_back(0xf7);return m;}};
}
namespace gp200 {
using RoutingOrder=std::array<int,11>;
struct GP200Preset {bool isValid=false;RoutingOrder routingOrder{};int fxLoopSend=0,fxLoopReturn=0;};
struct GP200PresetCodec {static GP200Preset decodeLivePresetDump(const juce::MemoryBlock& b){GP200Preset p;if(b.getSize()<912)return p;p.isValid=true;auto x=(const juce::uint8*)b.getData();p.fxLoopSend=x[106];p.fxLoopReturn=x[107];for(int i=0;i<11;i++)p.routingOrder[i]=x[108+i];return p;}};
struct Scanner {bool pending=false;void cancel(){pending=false;}bool hasPendingRequest(){return pending;}void setCachedName(int,juce::String){}};
struct MidiConnection {
int input=0;int* midiInput=&input;
struct Output {std::vector<juce::MidiMessage> messages;void sendMessageNow(juce::MidiMessage m){messages.push_back(m);}} output;Output* midiOutput=&output;
mutable juce::CriticalSection stateLock;
enum class IRUploadPhase{Idle,Busy};enum class SoundCloneUploadPhase{Idle,Busy};enum class StartupHandshakePhase{Idle,WaitingForIdentity,WaitingBeforeStateDump,WaitingForStateDump,WaitingForCurrentPreset,Ready};
IRUploadPhase irUploadPhase=IRUploadPhase::Idle;SoundCloneUploadPhase soundCloneUploadPhase=SoundCloneUploadPhase::Idle;
StartupHandshakePhase startupHandshakePhase=StartupHandshakePhase::Ready;
int currentSlot=0,presetDumpSlot=-1,lastRequestedNameSlot=-1;Scanner presetNameScanner;
bool modSyncActive=false,presetRestoreTransactionActive=false,currentPresetDataIsLive=true,currentStateRequestPending=false,currentStateRequestQueued=false,livePresetReadPending=false,presetNameRequestPending=false,liveRefreshPending=false;
double currentStateRequestSentMs=0,startupHandshakePhaseStartedMs=0,liveRefreshDueMs=0;
std::vector<std::vector<juce::uint8>> presetReadChunks,stateDumpChunks;juce::MemoryBlock currentPresetDecodedData;
struct RoutingRequestSnapshot {bool active=false;int slot=-1;RoutingOrder order{};int send=0,boundary=0,ret=0,mode=1;};
struct RoutingModeSnapshot{int mode=0x85,slot=0;std::uint64_t revision=1;};RoutingModeSnapshot routingModeSnapshot;
struct RoutingStateSnapshot {bool connected=false;int slot=-1;bool live=false;RoutingModeSnapshot mode;juce::MemoryBlock data;std::uint64_t presetRevision=0,liveRevision=0;bool canSave=false,modeFresh=false;};
bool isIRUploadInProgress(){return irUploadPhase!=IRUploadPhase::Idle;}bool isSoundCloneUploadInProgress(){return soundCloneUploadPhase!=SoundCloneUploadPhase::Idle;}juce::String getLastMessageText(){return lastMessageText;}
std::uint64_t presetRevision=1,livePresetRevision=1;
juce::String currentPresetName="test",currentPresetDumpStatusText,lastMessageText;
int routingStage=0,routingSlot=-1,routingSend=0,routingBoundary=0,routingReturn=0,routingValue=1;RoutingOrder routingOrder{};
std::uint64_t routingGeneration=0,slotGeneration=0,routingModeBaseline=0,routingLiveBaseline=0,presetModeBaseline=0;
double routingNextMs=0,routingDeadlineMs=0,routingQueryMs=0,presetReadStartedMs=0,presetReadResumeMs=0,modePollMs=0;int presetReadRetries=0;bool presetReadIsLive=false;
juce::String routingTransactionStatus="SPR: idle";
int irTicks=0,cloneTicks=0,scanTicks=0;bool stopped=false;
bool isConnected()const{return midiOutput!=nullptr;}void stopTimer(){stopped=true;}void processIRUpload(){irTicks++;}void processSoundCloneUpload(){cloneTicks++;}void finishModSyncFailure(juce::String){modSyncActive=false;}bool requestCurrentPresetFromGP200(){return sendStateDumpRequestUnlocked();}void processStartupHandshake(){}void processPresetNameScan(){scanTicks++;}
static juce::String sanitizePresetNameForStore(juce::String x){return x;}static std::vector<juce::uint8> buildStorePresetCommit(int slot,juce::String){return {0xf0,(juce::uint8)slot,0xf7};}
RoutingRequestSnapshot getRoutingRequestSnapshot () const;
RoutingStateSnapshot getRoutingStateSnapshot () const;
void timerCallback ();
bool sendFlexibleRouting (const RoutingOrder& order, int send, int boundary, int ret, bool parallel, int expectedSlot = -1);
bool canSaveCurrentPreset () const;
bool isRoutingTransactionBusy () const;
juce::String getRoutingTransactionStatus () const;
void failRoutingTransaction (const juce::String& reason);
void processRoutingTransaction ();
void processPresetReadRecovery ();
bool sendStateDumpRequestUnlocked ();
bool requestPresetNameForCurrentSlotIfNeeded ();
bool sendLiveReadRequestForSlot (int slot);
void resetPresetDumpCaptureForSlot (int slot);
void collectPresetReadChunk (const juce::uint8* data, int size);
static int getChunkOffset (const juce::uint8* data, int size);
static juce::MemoryBlock assemblePresetReadChunks (const std::vector<std::vector<juce::uint8>>& chunks);
static std::vector<juce::uint8> nibbleDecode (const juce::uint8* data, int size);
static std::vector<juce::uint8> buildLiveReadRequest (int slot);
static std::vector<juce::uint8> buildStateDumpRequest ();
bool sendRoutingModeValue (juce::uint8 value);
bool requestRoutingModeFromGP200 ();
bool sendReorderEffects (const RoutingOrder& routingOrder, int fxLoopSend, int fxLoopReturn);
static std::vector<juce::uint8> buildReorderEffects (const RoutingOrder& routingOrder, int fxLoopSend, int fxLoopReturn);
static std::vector<juce::uint8> nibbleEncode (const juce::uint8* data, int size);
void scheduleLivePresetRefresh ();
void processPendingLivePresetRefresh ();
void beginPresetRestoreTransaction ();
void endPresetRestoreTransaction ();
bool storeCurrentPresetToGP200 ();
};
MidiConnection::RoutingRequestSnapshot MidiConnection::getRoutingRequestSnapshot () const
{
    const juce::ScopedLock lock (stateLock);
    return { routingStage >= 1 && routingStage <= 5, routingSlot, routingOrder, routingSend, routingBoundary, routingReturn, routingValue };
}
MidiConnection::RoutingStateSnapshot MidiConnection::getRoutingStateSnapshot () const
{
    const juce::ScopedLock lock (stateLock);
    return { midiInput != nullptr && midiOutput != nullptr, currentSlot, currentPresetDataIsLive,
        routingModeSnapshot, currentPresetDecodedData, presetRevision, livePresetRevision, canSaveCurrentPreset (),
        routingModeSnapshot.slot == currentSlot && routingModeSnapshot.revision > presetModeBaseline };
}
void MidiConnection::timerCallback ()
{
    if (!isConnected ()) { stopTimer (); return; }
    const juce::ScopedLock lock (stateLock);
    // These operations belong to the connection, never to the editor window.
    processIRUpload ();
    processSoundCloneUpload ();
    if (irUploadPhase != IRUploadPhase::Idle || soundCloneUploadPhase != SoundCloneUploadPhase::Idle || presetRestoreTransactionActive)
    {
        if (modSyncActive) finishModSyncFailure ("interrupted by a transfer");
        if (routingStage > 0 && routingStage != 6) failRoutingTransaction ("interrupted by a transfer");
        return;
    }
    processPresetReadRecovery ();
    processRoutingTransaction ();
    if (routingStage == 1 || routingStage == 2) return;
    if (startupHandshakePhase == StartupHandshakePhase::Idle) requestCurrentPresetFromGP200 ();
    processStartupHandshake ();
    if (!modSyncActive)
    {
        if (currentStateRequestQueued && !currentStateRequestPending && presetDumpSlot < 0
            && !livePresetReadPending && !presetNameScanner.hasPendingRequest ()
            && startupHandshakePhase == StartupHandshakePhase::Ready)
            sendStateDumpRequestUnlocked ();
        processPendingLivePresetRefresh ();
        requestPresetNameForCurrentSlotIfNeeded ();
        const auto now = juce::Time::getMillisecondCounterHiRes ();
        if (currentPresetDataIsLive && routingStage == 0 && !presetNameScanner.hasPendingRequest () && now >= modePollMs)
        {
            requestRoutingModeFromGP200 (); modePollMs = now + 1200.0;
        }
        processPresetNameScan ();
    }
}
bool MidiConnection::sendFlexibleRouting (const RoutingOrder& order, int send, int boundary, int ret, bool parallel, int expectedSlot)
{
    const juce::ScopedLock lock (stateLock);
    // Validation and first write are atomic with respect to MIDI receive callbacks.
    if (midiOutput == nullptr || currentSlot < 0 || (expectedSlot >= 0 && expectedSlot != currentSlot)
        || !currentPresetDataIsLive || !validFlexibleRouting (order, send, boundary, ret)
        || presetRestoreTransactionActive || modSyncActive || presetDumpSlot >= 0 || currentStateRequestPending
        || irUploadPhase != IRUploadPhase::Idle || soundCloneUploadPhase != SoundCloneUploadPhase::Idle
        || presetNameScanner.hasPendingRequest () || routingStage >= 3)
    {
        lastMessageText = "SPR not sent: device state unavailable or operation busy";
        return false;
    }
    if (!sendRoutingModeValue (1)) return false;
    presetNameScanner.cancel ();
    liveRefreshPending = false;
    routingOrder = order; routingSend = send; routingBoundary = boundary; routingReturn = ret;
    routingValue = (parallel ? 0x80 : 0x90) | boundary;
    routingSlot = currentSlot; routingGeneration = slotGeneration;
    routingStage = 1;
    routingNextMs = juce::Time::getMillisecondCounterHiRes () + 150.0;
    routingDeadlineMs = routingNextMs + 6500.0;
    routingTransactionStatus = "SPR sending: Series, order, mode; waiting for device";
    return true;
}
bool MidiConnection::canSaveCurrentPreset () const
{
    const juce::ScopedLock lock (stateLock);
    const auto preset = GP200PresetCodec::decodeLivePresetDump (currentPresetDecodedData);
    const int boundary = isExtendedRoutingMode (routingModeSnapshot.mode) ? routingModeSnapshot.mode & 15 : preset.fxLoopSend;
    return preset.isValid && validFlexibleRouting (preset.routingOrder, preset.fxLoopSend, boundary, preset.fxLoopReturn)
        && midiOutput != nullptr && currentSlot >= 0 && currentPresetDataIsLive && routingStage == 0
        && !presetRestoreTransactionActive && !currentStateRequestPending && presetDumpSlot < 0
        && irUploadPhase == IRUploadPhase::Idle && soundCloneUploadPhase == SoundCloneUploadPhase::Idle
        && routingModeSnapshot.slot == currentSlot && routingModeSnapshot.revision > presetModeBaseline
        && validRoutingModeValue (routingModeSnapshot.mode);
}
bool MidiConnection::isRoutingTransactionBusy () const
{
    const juce::ScopedLock lock (stateLock);
    return routingStage != 0;
}
juce::String MidiConnection::getRoutingTransactionStatus () const
{
    const juce::ScopedLock lock (stateLock);
    return routingTransactionStatus;
}
void MidiConnection::failRoutingTransaction (const juce::String& reason)
{
    // Failure remains a save barrier until NEW device data and a NEW mode reply arrive.
    routingStage = 6; routingLiveBaseline = livePresetRevision;
    routingModeBaseline = routingModeSnapshot.revision;
    routingTransactionStatus = "SPR unconfirmed: " + reason + "; recovering device state";
    routingQueryMs = 0;
    scheduleLivePresetRefresh ();
}
void MidiConnection::processRoutingTransaction ()
{
    const juce::ScopedLock lock (stateLock);
    if (routingStage == 0 || midiOutput == nullptr) return;
    const auto now = juce::Time::getMillisecondCounterHiRes ();
    if (routingStage != 6 && (routingSlot != currentSlot || routingGeneration != slotGeneration))
    { failRoutingTransaction ("preset changed"); return; }
    if (routingStage != 6 && now >= routingDeadlineMs)
    { failRoutingTransaction ("readback timed out"); return; }
    if (routingStage == 1 || routingStage == 2)
    {
        if (now < routingNextMs) return;
        if (routingStage == 1)
        {
            if (!sendReorderEffects (routingOrder, routingSend, routingReturn))
            { failRoutingTransaction ("order send failed"); return; }
            routingStage = 2; routingNextMs = now + 150.0;
        }
        else
        {
            routingModeBaseline = routingModeSnapshot.revision;
            if (!sendRoutingModeValue (static_cast<juce::uint8> (routingValue)))
            { failRoutingTransaction ("mode send failed"); return; }
            routingStage = 3; routingQueryMs = now + 150.0;
        }
        return;
    }
    if (currentPresetDataIsLive && now >= routingQueryMs)
    {
        requestRoutingModeFromGP200 (); routingQueryMs = now + 700.0;
    }
    const bool freshMode = routingModeSnapshot.slot == currentSlot
        && routingModeSnapshot.revision > routingModeBaseline && validRoutingModeValue (routingModeSnapshot.mode);
    if (routingStage == 3 && freshMode && routingModeSnapshot.mode == routingValue)
    {
        routingLiveBaseline = livePresetRevision; routingStage = 4;
        scheduleLivePresetRefresh (); return;
    }
    if (routingStage == 4 && currentPresetDataIsLive && livePresetRevision > routingLiveBaseline)
    {
        const auto preset = GP200PresetCodec::decodeLivePresetDump (currentPresetDecodedData);
        if (!preset.isValid || preset.routingOrder != routingOrder || preset.fxLoopSend != routingSend || preset.fxLoopReturn != routingReturn)
        { failRoutingTransaction ("device order differs"); return; }
        routingModeBaseline = routingModeSnapshot.revision; routingStage = 5;
        routingQueryMs = 0; return;
    }
    if (routingStage == 5 && freshMode && routingModeSnapshot.mode == routingValue)
    {
        routingStage = 0; routingTransactionStatus = "SPR routing confirmed by device";
    }
    else if (routingStage == 6 && currentPresetDataIsLive && livePresetRevision > routingLiveBaseline && freshMode
        && routingModeSnapshot.revision > presetModeBaseline)
    {
        routingStage = 0;
        routingTransactionStatus = "SPR recovered actual device state; requested change was not confirmed";
    }
}
void MidiConnection::processPresetReadRecovery ()
{
    const juce::ScopedLock lock (stateLock);
    const auto now = juce::Time::getMillisecondCounterHiRes ();
    if (presetDumpSlot >= 0 && now - presetReadStartedMs >= 1500.0)
    {
        presetDumpSlot = -1; presetReadChunks.clear (); currentPresetDataIsLive = false;
        ++presetRevision; lastRequestedNameSlot = -1;
        // A quiet interval discards late replies before beginning a new capture.
        presetReadResumeMs = now + (++presetReadRetries <= 3 ? 300.0 : 5000.0);
        if (presetReadRetries > 3) presetReadRetries = 0;
        presetNameRequestPending = true; livePresetReadPending = true;
        currentPresetDumpStatusText = "Preset read incomplete: retry scheduled";
    }
}
bool MidiConnection::sendStateDumpRequestUnlocked ()
{
    if (midiOutput == nullptr || presetRestoreTransactionActive || routingStage == 1 || routingStage == 2
        || juce::Time::getMillisecondCounterHiRes () < presetReadResumeMs)
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
bool MidiConnection::requestPresetNameForCurrentSlotIfNeeded ()
{
    const juce::ScopedLock lock (stateLock);
    if (modSyncActive || presetRestoreTransactionActive || routingStage == 1 || routingStage == 2 || !presetNameRequestPending
        || juce::Time::getMillisecondCounterHiRes () < presetReadResumeMs)
        return false;

    if (currentSlot < 0)
        return false;

    if (lastRequestedNameSlot == currentSlot)
        return false;

    // Active slot reads must reflect the edit buffer, including unsaved routing.
    return sendLiveReadRequestForSlot (currentSlot);
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
    presetReadIsLive = true;

    lastMessageText = "Requested live edit buffer for slot " + juce::String (slot);

    return true;
}
void MidiConnection::resetPresetDumpCaptureForSlot (int slot)
{
    if (routingModeSnapshot.slot != slot)
    {
        routingModeSnapshot.mode = -1; routingModeSnapshot.slot = slot;
        ++routingModeSnapshot.revision;
    }
    presetModeBaseline = routingModeSnapshot.revision;
    presetReadStartedMs = juce::Time::getMillisecondCounterHiRes ();
    presetDumpSlot = slot;
    presetReadChunks.clear ();
    currentPresetDecodedData.setSize (0);
    currentPresetDataIsLive = false;
    ++presetRevision;

    currentPresetDumpStatusText = "Current full preset data: requesting slot " + juce::String (slot);
}
void MidiConnection::collectPresetReadChunk (const juce::uint8* data, int size)
{
    if (presetDumpSlot < 0 || presetDumpSlot != currentSlot
        || juce::Time::getMillisecondCounterHiRes () < presetReadResumeMs)
        return;

    // Real preset read chunks are large. This avoids confusing small
    // real-time parameter messages, which also use CMD=0x12 / SUB=0x18.
    if (size < 100 || (size - 14) % 2 != 0 || data[0] != 0xf0 || data[size-1] != 0xf7)
        return;
    for (int i = 13; i < size - 1; ++i) if (data[i] > 15) return;

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
        const auto preset = GP200PresetCodec::decodeLivePresetDump (currentPresetDecodedData);
        currentPresetDataIsLive = presetReadIsLive && preset.isValid
            && validFlexibleRouting (preset.routingOrder, preset.fxLoopSend, preset.fxLoopSend, preset.fxLoopReturn);
        if (!currentPresetDataIsLive)
        {
            currentPresetDumpStatusText = "Preset data rejected: invalid or not a live reply";
            presetReadChunks.clear (); return; // deadline recovers without trusting partial bytes
        }
        presetReadRetries = 0; modePollMs = 0;
        if (currentPresetDataIsLive && presetDumpSlot == currentSlot) ++livePresetRevision;
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
juce::MemoryBlock MidiConnection::assemblePresetReadChunks (const std::vector<std::vector<juce::uint8>>& chunks)
{
    if (chunks.empty ()) return {};
    auto sortedChunks = chunks;

    std::sort (sortedChunks.begin (),
               sortedChunks.end (),
               [] (const auto& a, const auto& b)
               {
                   return getChunkOffset (a.data (), static_cast<int> (a.size ())) <
                          getChunkOffset (b.data (), static_cast<int> (b.size ()));
               });

    if (getChunkOffset (sortedChunks.front ().data (), static_cast<int> (sortedChunks.front ().size ())) != 0) return {};
    std::vector<juce::uint8> allNibbles;
    int expectedOffset = 0, declaredLength = -1;
    for (const auto& chunk : sortedChunks)
    {
        const auto offset = getChunkOffset (chunk.data (), static_cast<int> (chunk.size ()));
        if (chunk.size () <= 14 || (chunk.size () - 14) % 2 != 0 || chunk.front () != 0xf0 || chunk.back () != 0xf7
            || offset != expectedOffset || chunk[9] > 127 || chunk[10] > 127) return {};
        const int total = chunk[9] | (chunk[10] << 7);
        if (total <= 0 || total > 4096 || (declaredLength >= 0 && total != declaredLength)) return {};
        declaredLength = total;
        expectedOffset += static_cast<int> ((chunk.size () - 14) / 2);
        if (expectedOffset > declaredLength) return {};
        for (std::size_t i = 13; i + 1 < chunk.size (); ++i) if (chunk[i] > 15) return {};

        const auto* nibbleStart = chunk.data () + 13;
        const auto nibbleSize = static_cast<int> (chunk.size ()) - 14;

        allNibbles.insert (allNibbles.end (), nibbleStart, nibbleStart + nibbleSize);
    }

    const auto decoded = nibbleDecode (allNibbles.data (), static_cast<int> (allNibbles.size ()));

    juce::MemoryBlock result;

    if (expectedOffset == declaredLength && decoded.size () == static_cast<std::size_t> (declaredLength))
        result.append (decoded.data (), decoded.size ());

    return result;
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
std::vector<juce::uint8> MidiConnection::buildStateDumpRequest ()
{
    return {0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32, 0x11, 0x04, 0x00,
            0x00, 0x00, 0x00, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7};
}
bool MidiConnection::sendRoutingModeValue (juce::uint8 value)
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr || !validRoutingModeValue (value))
    {
        lastMessageText = "Cannot change routing mode: MIDI output not open";
        return false;
    }
    const auto bytes = flexibleRoutingModeMessage (value);
    const auto message = juce::MidiMessage::createSysExMessage (bytes.data () + 1,
        static_cast<int> (bytes.size () - 2));
    midiOutput->sendMessageNow (message);
    lastMessageText = "Sent routing mode " + juce::String (static_cast<int> (value));
    return true;
}
bool MidiConnection::requestRoutingModeFromGP200 ()
{
    const juce::ScopedLock lock (stateLock);
    if (midiOutput == nullptr || modSyncActive || presetRestoreTransactionActive) return false;
    const auto bytes = routingModeQueryMessage ();
    midiOutput->sendMessageNow (juce::MidiMessage::createSysExMessage (bytes.data () + 1,
        static_cast<int> (bytes.size () - 2)));
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

    if (fxLoopSend < 0 || fxLoopSend > 11 || fxLoopReturn < fxLoopSend || fxLoopReturn > 11)
    {
        lastMessageText = "Cannot reorder effects: invalid FX Loop position";
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

    if (currentPresetDecodedData.getSize () >= routingOrderOffset + effectBlockCount &&
        currentPresetDecodedData.getSize () > fxLoopReturnOffset)
    {
        auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());
        if (data != nullptr)
        {
            data[fxLoopSendOffset] = static_cast<juce::uint8> (fxLoopSend);
            data[fxLoopReturnOffset] = static_cast<juce::uint8> (fxLoopReturn);
            for (std::size_t i = 0; i < effectBlockCount; ++i)
                data[routingOrderOffset + i] = static_cast<juce::uint8> (routingOrder[i] & 0xFF);
            ++presetRevision;
        }
    }

    lastMessageText = "Sent effect chain reorder to GP-200";

    return true;
}
std::vector<juce::uint8> MidiConnection::buildReorderEffects (const RoutingOrder& routingOrder, int fxLoopSend, int fxLoopReturn)
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
void MidiConnection::scheduleLivePresetRefresh ()
{
    constexpr double liveRefreshDebounceMs = 120.0;

    liveRefreshPending = true;
    liveRefreshDueMs =
        juce::Time::getMillisecondCounterHiRes () + liveRefreshDebounceMs;
}
void MidiConnection::processPendingLivePresetRefresh ()
{
    const juce::ScopedLock lock (stateLock);

    if (modSyncActive || !liveRefreshPending || midiOutput == nullptr || currentSlot < 0)
        return;

    const auto nowMs = juce::Time::getMillisecondCounterHiRes ();

    if (nowMs < liveRefreshDueMs || nowMs < presetReadResumeMs
        || currentStateRequestPending || livePresetReadPending)
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
void MidiConnection::beginPresetRestoreTransaction ()
{
    const juce::ScopedLock lock (stateLock);

    if (routingStage != 0) failRoutingTransaction ("Recall started");
    currentPresetDataIsLive = false;
    routingModeSnapshot.mode = -1; ++routingModeSnapshot.revision;
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
    currentPresetDataIsLive = false;
    startupHandshakePhase = StartupHandshakePhase::Ready;
    currentStateRequestQueued = true;
}
bool MidiConnection::storeCurrentPresetToGP200 ()
{
    const juce::ScopedLock lock (stateLock);
    if (!canSaveCurrentPreset ())
    {
        lastMessageText = "Cannot store: wait for fresh device preset and routing confirmation";
        return false;
    }
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
        "Store request sent to GP-200 slot " + juce::String (currentSlot) + ": " + presetName;

    return true;
}
} // namespace gp200

struct Ribbon {
 gp200::RoutingOrder order{0,1,2,3,4,5,6,7,8,9,10};int s=2,p=5,r=8;bool parallel=true,draft=false;
 gp200::RoutingOrder getLocalOrder(){return order;}int getSend(){return s;}int getBoundary(){return p;}int getReturn(){return r;}
 void keepRoutingDraft(){draft=true;}void releaseRoutingDraft(){draft=false;}
 void setDeviceRouting(int a,int b,int c,bool v){s=a;p=b;r=c;parallel=v;}void setParallelMode(bool v){parallel=v;}
};
struct AudioPluginAudioProcessorEditor {
 gp200::MidiConnection& midiConnection;Ribbon effectChainRibbon;
 explicit AudioPluginAudioProcessorEditor(gp200::MidiConnection& m):midiConnection(m){}
 ~AudioPluginAudioProcessorEditor();
 bool presetRestoreInProgress=false,parallelRoutingSelected=true,sprWasConnected=false;int sprDeviceSlot=-2,sprDeviceMode=-1;
 std::uint64_t sprAppliedPresetRevision=0;gp200::GP200Preset sprConfirmedPreset;int sprConfirmedBoundary=5,sprConfirmedMode=-1;
 juce::String effectsStatusText;
 struct Slider{int getValue(){return 0;}}patchVolumeSlider,panSlider,tempoSlider;int offlinePatchVolume=0,offlinePatchPan=0,offlinePatchTempo=0;
 struct Processor{void notifyOfflineStateChanged(){}}processorRef;
 void repaint(){}void updateSeriesParallelButtonText(){}void scheduleEditorHeightUpdate(){}void clearInterfaceTypography(){}void stopTimer(){}
 void updateEffectChainRibbon(gp200::GP200Preset p){effectChainRibbon.order=p.routingOrder;}
 void sendFlexibleRouteFromRibbon();void syncFlexibleRoutingFromDevice();void toggleSeriesParallel();
};
void AudioPluginAudioProcessorEditor::sendFlexibleRouteFromRibbon ()
{
    const auto state = midiConnection.getRoutingStateSnapshot ();
    if (presetRestoreInProgress || !midiConnection.sendFlexibleRouting (effectChainRibbon.getLocalOrder (),
        effectChainRibbon.getSend (), effectChainRibbon.getBoundary (), effectChainRibbon.getReturn (), parallelRoutingSelected, state.slot))
    {
        // Force reapplication even when the confirmed revision did not change.
        const auto accepted = midiConnection.getRoutingRequestSnapshot ();
        auto fallback = sprConfirmedPreset;
        int boundary = sprConfirmedBoundary, mode = sprConfirmedMode;
        if (accepted.active && accepted.slot == state.slot)
        {
            if (!fallback.isValid) fallback = gp200::GP200PresetCodec::decodeLivePresetDump (state.data);
            fallback.routingOrder = accepted.order; fallback.fxLoopSend = accepted.send; fallback.fxLoopReturn = accepted.ret;
            boundary = accepted.boundary; mode = accepted.mode;
        }
        effectChainRibbon.releaseRoutingDraft ();
        if (fallback.isValid && gp200::validRoutingModeValue (mode))
        {
            updateEffectChainRibbon (fallback);
            parallelRoutingSelected = gp200::routingModeIsParallel (mode);
            effectChainRibbon.setDeviceRouting (fallback.fxLoopSend, boundary, fallback.fxLoopReturn, parallelRoutingSelected);
            updateSeriesParallelButtonText ();
            if (accepted.active) effectChainRibbon.keepRoutingDraft ();
        }
        sprAppliedPresetRevision = 0;
        syncFlexibleRoutingFromDevice ();
        effectsStatusText = "SPR NOT SENT: " + midiConnection.getLastMessageText ();
        repaint (); return;
    }
    effectChainRibbon.keepRoutingDraft ();
    effectsStatusText = midiConnection.getRoutingTransactionStatus ();
    repaint ();
}
void AudioPluginAudioProcessorEditor::syncFlexibleRoutingFromDevice ()
{
    const auto state = midiConnection.getRoutingStateSnapshot ();
    if (!state.connected || !sprWasConnected || state.slot != sprDeviceSlot)
    {
        sprWasConnected = state.connected; sprDeviceSlot = state.slot;
        sprDeviceMode = -1; sprAppliedPresetRevision = 0;
        sprConfirmedPreset = {}; sprConfirmedMode = -1;
        effectChainRibbon.releaseRoutingDraft ();
    }
    if (!state.connected) return;
    if (midiConnection.isRoutingTransactionBusy ())
    {
        effectsStatusText = midiConnection.getRoutingTransactionStatus ();
        repaint (); return;
    }
    if (!state.live || !state.modeFresh || state.mode.slot != state.slot || !gp200::validRoutingModeValue (state.mode.mode)) return;
    const auto preset = gp200::GP200PresetCodec::decodeLivePresetDump (state.data);
    const auto boundary = gp200::isExtendedRoutingMode (state.mode.mode) ? state.mode.mode & 15
        : juce::jlimit (preset.fxLoopSend, preset.fxLoopReturn, effectChainRibbon.getBoundary ());
    if (!preset.isValid || !gp200::validFlexibleRouting (preset.routingOrder, preset.fxLoopSend, boundary, preset.fxLoopReturn)) return;
    if (state.presetRevision == sprAppliedPresetRevision && state.mode.mode == sprDeviceMode) return;
    sprAppliedPresetRevision = state.presetRevision; sprDeviceMode = state.mode.mode;
    sprConfirmedPreset = preset; sprConfirmedBoundary = boundary; sprConfirmedMode = state.mode.mode;
    effectChainRibbon.releaseRoutingDraft ();
    updateEffectChainRibbon (preset);
    parallelRoutingSelected = gp200::routingModeIsParallel (state.mode.mode);
    effectChainRibbon.setDeviceRouting (preset.fxLoopSend, boundary, preset.fxLoopReturn, parallelRoutingSelected);
    updateSeriesParallelButtonText (); scheduleEditorHeightUpdate ();
    effectsStatusText = midiConnection.getRoutingTransactionStatus ();
    repaint ();
}
void AudioPluginAudioProcessorEditor::toggleSeriesParallel ()
{
    if (midiConnection.isRoutingTransactionBusy ())
    { effectsStatusText = midiConnection.getRoutingTransactionStatus (); repaint (); return; }
    const bool newParallelState = !parallelRoutingSelected;

    parallelRoutingSelected = newParallelState;
    effectChainRibbon.setParallelMode (parallelRoutingSelected);
    updateSeriesParallelButtonText ();
    effectsStatusText = parallelRoutingSelected
        ? "SPR Parallel"
        : "SPR Series";
    repaint ();
    sendFlexibleRouteFromRibbon ();
    scheduleEditorHeightUpdate ();
}
AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor ()
{
    stopTimer ();
    // Recall steps still belong to this window: cancel and recover actual device state.
    if (presetRestoreInProgress) midiConnection.endPresetRestoreTransaction ();

    if (!midiConnection.isConnected ())
    {
        offlinePatchVolume = static_cast<int> (patchVolumeSlider.getValue ());
        offlinePatchPan = static_cast<int> (panSlider.getValue ());
        offlinePatchTempo = static_cast<int> (tempoSlider.getValue ());
        processorRef.notifyOfflineStateChanged ();
    }

    clearInterfaceTypography ();
}
using namespace gp200;
RoutingOrder order{0,1,2,3,4,5,6,7,8,9,10};
juce::MemoryBlock data(){juce::MemoryBlock b;b.setSize(1176);auto p=(juce::uint8*)b.getData();p[106]=2;p[107]=8;for(int i=0;i<11;i++)p[108+i]=i;return b;}
void init(MidiConnection& m){m.currentPresetDecodedData=data();juce::Time::now=0;}
std::vector<std::vector<juce::uint8>> chunks(const juce::MemoryBlock& b,int stride=185){std::vector<std::vector<juce::uint8>> out;auto p=(const juce::uint8*)b.getData();int total=b.getSize();for(int off=0;off<total;off+=stride){std::vector<juce::uint8> x{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,(juce::uint8)(total&127),(juce::uint8)(total>>7),(juce::uint8)(off&127),(juce::uint8)(off>>7)};for(int i=off;i<std::min(total,off+stride);i++){x.push_back(p[i]>>4);x.push_back(p[i]&15);}x.push_back(0xf7);out.push_back(x);}return out;}
void receive(MidiConnection& m){auto cs=chunks(data());std::reverse(cs.begin(),cs.end());for(auto& x:cs)m.collectPresetReadChunk(x.data(),x.size());}
void mode(MidiConnection& m,int value){m.routingModeSnapshot.mode=value;m.routingModeSnapshot.slot=m.currentSlot;++m.routingModeSnapshot.revision;}
void stageToReply(MidiConnection& m){assert(m.sendFlexibleRouting(order,2,5,8,true,0));assert(!m.canSaveCurrentPreset());juce::Time::now=150;m.processRoutingTransaction();assert(m.routingStage==2);juce::Time::now=300;m.processRoutingTransaction();assert(m.routingStage==3);}
int main(){int cases=0;
{MidiConnection m;init(m);stageToReply(m);mode(m,0x85);m.processRoutingTransaction();assert(m.routingStage==4);juce::Time::now=500;m.processPendingLivePresetRefresh();assert(m.presetDumpSlot==0&&!m.currentPresetDataIsLive);receive(m);m.processRoutingTransaction();assert(m.routingStage==5&&!m.canSaveCurrentPreset());mode(m,0x85);m.processRoutingTransaction();assert(m.routingStage==0&&m.canSaveCurrentPreset());assert(m.storeCurrentPresetToGP200());cases++;}
// There is no editor in this harness: the production connection timer alone advances writes.
{MidiConnection m;init(m);assert(m.sendFlexibleRouting(order,2,5,8,true,0));juce::Time::now=150;m.timerCallback();assert(m.routingStage==2&&m.irTicks==1&&m.cloneTicks==1);juce::Time::now=300;m.timerCallback();assert(m.routingStage==3);cases++;}
for(int step:{1,2}){MidiConnection m;init(m);assert(m.sendFlexibleRouting(order,2,5,8,true,0));if(step==2){juce::Time::now=150;m.processRoutingTransaction();}auto before=m.output.messages.size();m.currentSlot=1;++m.slotGeneration;juce::Time::now=300;m.processRoutingTransaction();assert(m.routingStage==6&&m.output.messages.size()==before&&!m.canSaveCurrentPreset());cases++;}
{MidiConnection m;init(m);assert(!m.sendFlexibleRouting(order,2,5,8,true,1));assert(m.output.messages.empty());cases++;}
{MidiConnection m;init(m);stageToReply(m);juce::Time::now=7000;m.processRoutingTransaction();assert(m.routingStage==6&&!m.canSaveCurrentPreset());assert(!m.storeCurrentPresetToGP200());mode(m,0x85);m.processRoutingTransaction();assert(m.routingStage==6);++m.livePresetRevision;m.processRoutingTransaction();assert(m.routingStage==0&&m.canSaveCurrentPreset());cases++;}
{MidiConnection m;init(m);stageToReply(m);mode(m,0x95);m.processRoutingTransaction();assert(m.routingStage==3);juce::Time::now=7000;m.processRoutingTransaction();assert(m.routingStage==6);cases++;}
{MidiConnection m;init(m);stageToReply(m);mode(m,0x85);m.processRoutingTransaction();++m.livePresetRevision;((juce::uint8*)m.currentPresetDecodedData.getData())[106]=3;m.processRoutingTransaction();assert(m.routingStage==6&&!m.canSaveCurrentPreset());cases++;}
{MidiConnection m;init(m);assert(m.sendFlexibleRouting(order,2,5,8,true,0));auto value=m.routingValue;auto bad=order;bad[0]=bad[1];assert(!m.sendFlexibleRouting(bad,2,5,8,false,0));assert(m.routingStage==1&&m.routingValue==value);cases++;}
{MidiConnection m;init(m);m.beginPresetRestoreTransaction();assert(!m.canSaveCurrentPreset()&&m.presetRestoreTransactionActive);m.endPresetRestoreTransaction();assert(!m.presetRestoreTransactionActive&&m.currentStateRequestQueued&&!m.currentPresetDataIsLive);m.timerCallback();assert(m.currentStateRequestPending&&!m.currentStateRequestQueued);cases++;}
for(int missing=0;missing<7;missing++){MidiConnection m;init(m);m.sendLiveReadRequestForSlot(0);auto cs=chunks(data());for(int i=0;i<7;i++)if(i!=missing)m.collectPresetReadChunk(cs[i].data(),cs[i].size());assert(!m.currentPresetDataIsLive);juce::Time::now=1500;m.processPresetReadRecovery();assert(m.presetDumpSlot<0&&m.presetReadChunks.empty());auto before=m.output.messages.size();assert(!m.requestPresetNameForCurrentSlotIfNeeded());juce::Time::now=1800;assert(m.requestPresetNameForCurrentSlotIfNeeded());assert(m.output.messages.size()==before+1);receive(m);assert(m.currentPresetDataIsLive&&m.livePresetRevision==2);cases++;}
{MidiConnection m;init(m);m.sendLiveReadRequestForSlot(0);m.currentStateRequestQueued=true;receive(m);assert(m.currentStateRequestQueued);m.timerCallback();assert(!m.currentStateRequestQueued&&m.currentStateRequestPending);cases++;}
{MidiConnection m;init(m);m.sendLiveReadRequestForSlot(0);auto cs=chunks(data());m.collectPresetReadChunk(cs[0].data(),cs[0].size());m.collectPresetReadChunk(cs[0].data(),cs[0].size());assert(m.presetReadChunks.size()==1);cases++;}
{auto cs=chunks(data());assert(MidiConnection::assemblePresetReadChunks(cs).getSize()==1176);for(int kind=0;kind<8;kind++){auto bad=cs;if(kind==0)bad[6][11]++;if(kind==1)bad[6][11]--;if(kind==2)bad[0][13]=0x7f;if(kind==3)bad[3][10]++;if(kind==4)bad[0][11]=1;if(kind==5)bad.back().back()=0;if(kind==6)bad.back().insert(bad.back().end()-1,0);if(kind==7)bad.pop_back();assert(MidiConnection::assemblePresetReadChunks(bad).getSize()==0);cases++;}}
{juce::MemoryBlock b;b.setSize(846);auto cs=chunks(b);assert(cs.size()==5);assert(MidiConnection::assemblePresetReadChunks(cs).getSize()==846);cases++;}
{MidiConnection m;init(m);for(int value=0;value<256;value++){m.routingModeSnapshot.mode=value;assert(m.canSaveCurrentPreset()==(validRoutingModeValue(value)&&(!isExtendedRoutingMode(value)||((value&15)>=2&&(value&15)<=8))));cases++;}}
{MidiConnection m;init(m);AudioPluginAudioProcessorEditor editor(m);editor.syncFlexibleRoutingFromDevice();assert(editor.parallelRoutingSelected);m.irUploadPhase=MidiConnection::IRUploadPhase::Busy;editor.toggleSeriesParallel();assert(editor.parallelRoutingSelected&&editor.effectChainRibbon.parallel&&m.output.messages.empty());m.irUploadPhase=MidiConnection::IRUploadPhase::Idle;editor.syncFlexibleRoutingFromDevice();assert(editor.parallelRoutingSelected);cases++;}
{MidiConnection m;init(m);AudioPluginAudioProcessorEditor editor(m);editor.syncFlexibleRoutingFromDevice();m.currentPresetDataIsLive=false;editor.toggleSeriesParallel();assert(editor.parallelRoutingSelected&&m.output.messages.empty());cases++;}
{MidiConnection m;init(m);stageToReply(m);AudioPluginAudioProcessorEditor editor(m);editor.effectChainRibbon.order[0]=1;editor.parallelRoutingSelected=false;editor.sendFlexibleRouteFromRibbon();assert(m.routingStage==3&&editor.effectChainRibbon.order==order&&editor.parallelRoutingSelected);cases++;}
{MidiConnection m;init(m);{AudioPluginAudioProcessorEditor editor(m);m.beginPresetRestoreTransaction();editor.presetRestoreInProgress=true;}assert(!m.presetRestoreTransactionActive&&m.currentStateRequestQueued&&!m.canSaveCurrentPreset());cases++;}
{MidiConnection m;init(m);m.sendLiveReadRequestForSlot(0);receive(m);assert(!m.canSaveCurrentPreset());mode(m,0x85);assert(m.canSaveCurrentPreset());cases++;}
{MidiConnection m;init(m);assert(m.sendFlexibleRouting(order,2,5,8,true,0));++m.slotGeneration;juce::Time::now=150;auto n=m.output.messages.size();m.processRoutingTransaction();assert(m.routingStage==6&&m.output.messages.size()==n);cases++;}
{MidiConnection m;init(m);m.sendLiveReadRequestForSlot(0);for(int attempt=0;attempt<4;attempt++){juce::Time::now=m.presetReadStartedMs+1500;m.processPresetReadRecovery();double expected=attempt==3?5000:300;assert(m.presetReadResumeMs==juce::Time::now+expected);juce::Time::now=m.presetReadResumeMs;assert(m.requestPresetNameForCurrentSlotIfNeeded());}cases++;}
std::cout<<cases<<" FIX7 connection / read / save regressions passed (real method bodies, simulated ports and codec)\n";
}
