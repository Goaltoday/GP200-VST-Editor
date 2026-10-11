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
#include <cstring>
#include "../source/libgp200/GP200ChainBlend.h"
#include "../source/libgp200/GP200SceneSync.h"
namespace juce {
using uint8=std::uint8_t;using uint32=std::uint32_t;
template<class T>T jlimit(T a,T b,T v){return std::clamp(v,a,b);}
struct String:std::string {using std::string::string;using std::string::operator=;String(int n):std::string(std::to_string(n)){}String(std::string s):std::string(s){}bool isEmpty()const{return empty();}};
struct MemoryBlock {std::vector<uint8> v;void setSize(size_t n){v.resize(n);}size_t getSize()const{return v.size();}void* getData(){return v.data();}const void* getData()const{return v.data();}void append(const void*p,size_t n){auto b=(const uint8*)p;v.insert(v.end(),b,b+n);}};
struct CriticalSection {mutable std::recursive_mutex m;};struct ScopedLock{const CriticalSection& c;ScopedLock(const CriticalSection& x):c(x){c.m.lock();}~ScopedLock(){c.m.unlock();}};
struct Time {static inline double now=0;static double getMillisecondCounterHiRes(){return now;}};
struct MidiMessage {std::vector<uint8> bytes;static MidiMessage createSysExMessage(const uint8*p,int n){MidiMessage m;m.bytes.push_back(0xf0);m.bytes.insert(m.bytes.end(),p,p+n);m.bytes.push_back(0xf7);return m;}};
}
namespace gp200 {
using RoutingOrder=std::array<int,11>;
struct GP200EffectSlot {int blockIndex=0,slotIndex=0;bool enabled=true;juce::uint32 effectId=0x06000003;std::array<float,15> params{};};
struct GP200Preset {std::array<GP200EffectSlot,11> effects{};bool isValid=false;RoutingOrder routingOrder{};int fxLoopSend=0,fxLoopReturn=0;};
struct GP200PresetCodec {static GP200Preset decodeLivePresetDump(const juce::MemoryBlock& b){GP200Preset p;if(b.getSize()<912)return p;p.isValid=true;auto x=(const juce::uint8*)b.getData();p.fxLoopSend=x[106];p.fxLoopReturn=x[107];for(int i=0;i<11;i++){p.routingOrder[i]=x[108+i];p.effects[i].blockIndex=i;std::memcpy(p.effects[i].params.data(),x+effectBlockStart+i*effectBlockSize+paramsOffset,60);}return p;}};
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
double currentStateRequestSentMs=0,startupHandshakePhaseStartedMs=0,liveRefreshDueMs=0,lastLiveRefreshSentMs=-1000;
std::vector<std::vector<juce::uint8>> presetReadChunks,stateDumpChunks;juce::MemoryBlock currentPresetDecodedData;
struct RoutingRequestSnapshot {bool active=false;int slot=-1;RoutingOrder order{};int send=0,boundary=0,ret=0,mode=1;};
struct RoutingModeSnapshot{int mode=0x85,slot=0;std::uint64_t revision=1;};RoutingModeSnapshot routingModeSnapshot;
struct RoutingStateSnapshot {bool connected=false;int slot=-1;bool live=false;RoutingModeSnapshot mode;juce::MemoryBlock data;std::uint64_t presetRevision=0,liveRevision=0;bool canSave=false,modeFresh=false;};
bool isIRUploadInProgress(){return irUploadPhase!=IRUploadPhase::Idle;}bool isSoundCloneUploadInProgress(){return soundCloneUploadPhase!=SoundCloneUploadPhase::Idle;}juce::String getLastMessageText(){return lastMessageText;}
std::uint64_t presetRevision=1,livePresetRevision=1;
juce::String currentPresetName="test",currentPresetDumpStatusText,lastMessageText;
bool adjustmentWritePending=false,sceneRefreshPending=false,restoreChainMetadataReadback=false;std::uint32_t adjustmentWriteExpected=0;std::uint64_t sceneChangeRevision=0;
int remoteSceneStatus=-1,remoteSceneSlot=-1;double remoteSceneDeadline=0,remoteSceneLastSend=0;
int nativeRoutingMode=0;bool blendWritePending=false,blendWriteMarked=false;int blendWriteSlot=-1;std::uint64_t blendWriteBaseline=0;float blendWriteExpected=50.0f;double blendWriteDeadline=0;
std::vector<std::pair<int,float>> parameterWrites;
bool sendParamChange(int block,int param,juce::uint32,float value){parameterWrites.push_back({param,value});std::memcpy(currentPresetDecodedData.v.data()+effectBlockStart+block*effectBlockSize+paramsOffset+param*4,&value,4);return true;}
int routingStage=0,routingSlot=-1,routingSend=0,routingBoundary=0,routingReturn=0,routingValue=1;RoutingOrder routingOrder{};
std::uint64_t routingGeneration=0,slotGeneration=0,routingModeBaseline=0,routingLiveBaseline=0,presetModeBaseline=0,presetRestoreSlotGeneration=0;
double routingNextMs=0,routingDeadlineMs=0,routingQueryMs=0,presetReadStartedMs=0,presetReadResumeMs=0,modePollMs=0;int presetReadRetries=0;bool presetReadIsLive=false;
juce::String routingTransactionStatus="SPR: idle";
int irTicks=0,cloneTicks=0,scanTicks=0;bool stopped=false;
bool isConnected()const{return midiOutput!=nullptr;}void stopTimer(){stopped=true;}void processIRUpload(){irTicks++;}void processSoundCloneUpload(){cloneTicks++;}void finishModSyncFailure(juce::String){modSyncActive=false;}bool requestCurrentPresetFromGP200(){return sendStateDumpRequestUnlocked();}void processStartupHandshake(){}void processPresetNameScan(){scanTicks++;}
static juce::String sanitizePresetNameForStore(juce::String x){return x;}static std::vector<juce::uint8> buildStorePresetCommit(int slot,juce::String){return {0xf0,(juce::uint8)slot,0xf7};}
int getRemoteSceneStatus () const;
bool isSceneCommandPending () const;
bool sendSceneCommand (int command);
static std::vector<juce::uint8> buildParamChange (int blockIndex, int paramIndex, juce::uint32 effectId, float value);
static std::array<juce::uint8, 2> encodeDisplayValue (float value);
bool sendChainAdjustment (int frames, bool inverted, int expectedSlot);
std::uint64_t getSceneChangeRevision () const;
bool handleSceneChangeNotification (const juce::uint8* data, int size);
void processBlendReadback ();
bool isBlendWritePending () const;
bool sendIndependentBlend (float value, bool activate, int expectedSlot);
RoutingRequestSnapshot getRoutingRequestSnapshot () const;
RoutingStateSnapshot getRoutingStateSnapshot (bool includeSavePermission = true) const;
void timerCallback ();
bool sendFlexibleRouting (const RoutingOrder& order, int send, int boundary, int ret, bool parallel, int expectedSlot = -1, bool modeOnly = false);
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
void endPresetRestoreTransaction (int expectedRoutingMode = -1);
bool sendPresetRestoreRoutingMode (int expectedSlot, int mode);
bool storeCurrentPresetToGP200 ();
};
int MidiConnection::getRemoteSceneStatus () const
{
    const juce::ScopedLock lock(stateLock);
    return remoteSceneSlot==currentSlot ? remoteSceneStatus : -1;
}
bool MidiConnection::isSceneCommandPending () const
{
    const juce::ScopedLock lock(stateLock);
    return juce::Time::getMillisecondCounterHiRes()<remoteSceneDeadline;
}
bool MidiConnection::sendSceneCommand (int command)
{
    const juce::ScopedLock lock(stateLock);
    const auto now=juce::Time::getMillisecondCounterHiRes();
    if(command<0 || command>10 || midiOutput==nullptr || !currentPresetDataIsLive
       || currentSlot<0 || currentSlot>255 || sceneRefreshPending || routingStage!=0
       || blendWritePending || presetRestoreTransactionActive || modSyncActive
       || presetDumpSlot>=0 || currentStateRequestPending
       || irUploadPhase!=IRUploadPhase::Idle || soundCloneUploadPhase!=SoundCloneUploadPhase::Idle
       || now<remoteSceneDeadline || now-remoteSceneLastSend<500.0) return false;
    if(remoteSceneSlot!=currentSlot || remoteSceneStatus<0)return false;
    if(command>=3 && remoteSceneStatus!=5 && !(remoteSceneStatus>=0 && remoteSceneStatus<4))return false;
    const auto word=0x53430000u | (static_cast<std::uint32_t>(currentSlot)<<8) | static_cast<std::uint32_t>(command);
    const auto preset=GP200PresetCodec::decodeLivePresetDump(currentPresetDecodedData);
    if(!preset.isValid)return false;
    // P15 is the command mailbox, not one of the 15 stored parameters.
    const auto bytes=buildParamChange(10,15,preset.effects[10].effectId,std::bit_cast<float>(word));
    midiOutput->sendMessageNow(juce::MidiMessage::createSysExMessage(bytes.data()+1,static_cast<int>(bytes.size()-2)));
    remoteSceneLastSend=now;remoteSceneDeadline=now+2500.0;
    lastMessageText="Scene command sent: awaiting GP200 confirmation";
    return true;
}
std::vector<juce::uint8> MidiConnection::buildParamChange (int blockIndex, int paramIndex, juce::uint32 effectId, float value)
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
bool MidiConnection::sendChainAdjustment (int frames, bool inverted, int expectedSlot)
{
    const juce::ScopedLock lock (stateLock);
    if (frames < -32 || frames > 32 || midiOutput == nullptr || currentSlot != expectedSlot
        || !currentPresetDataIsLive || blendWritePending || routingStage != 0 || presetRestoreTransactionActive
        || modSyncActive || presetDumpSlot >= 0 || currentStateRequestPending
        || irUploadPhase != IRUploadPhase::Idle || soundCloneUploadPhase != SoundCloneUploadPhase::Idle
        || !routingModeIsChain(routingModeSnapshot.mode) || routingModeSnapshot.slot != currentSlot
        || routingModeSnapshot.revision <= presetModeBaseline) return false;
    const auto p = GP200PresetCodec::decodeLivePresetDump(currentPresetDecodedData);
    if (!p.isValid) return false;
    const auto word = alignWord(frames,inverted);
    if (!sendParamChange(10,12,p.effects[10].effectId,std::bit_cast<float>(word))) return false;
    adjustmentWriteExpected=word; adjustmentWritePending=true; restoreChainMetadataReadback=false; blendWriteSlot=currentSlot;
    blendWriteBaseline=livePresetRevision; blendWriteDeadline=juce::Time::getMillisecondCounterHiRes()+6500.0;
    blendWritePending=true; scheduleLivePresetRefresh();
    lastMessageText="ALIGN/PHASE sent: awaiting pedal readback";
    return true;
}
std::uint64_t MidiConnection::getSceneChangeRevision () const
{
    const juce::ScopedLock lock (stateLock);
    return sceneChangeRevision;
}
bool MidiConnection::handleSceneChangeNotification (const juce::uint8* data, int size)
{
    const int slot = sceneChangeSlot(data,size);
    if (slot < 0) return false;
    if (slot != currentSlot) return true; // Discard an event from a previous slot.
    if(((data[41]<<4)|data[42])==0x52 || (remoteSceneSlot==slot && remoteSceneStatus>=0)){remoteSceneStatus=sceneStatus(data,size); remoteSceneSlot=slot; remoteSceneDeadline=0;}
    ++sceneChangeRevision;
    if(sceneStatus(data,size)>=4) return true;
    sceneRefreshPending=true; currentPresetDataIsLive=false; ++presetRevision;
    blendWritePending=false; adjustmentWritePending=false; restoreChainMetadataReadback=false;
    if(presetRestoreTransactionActive) {
        presetRestoreTransactionActive=false;
        currentStateRequestQueued=false; currentStateRequestPending=false; stateDumpChunks.clear();
    }
    if(routingStage != 0) failRoutingTransaction("Scene changed");
    if(presetDumpSlot >= 0) {
        presetDumpSlot=-1; presetReadChunks.clear();
        presetReadResumeMs=juce::Time::getMillisecondCounterHiRes()+300.0;
    }
    // A physical recall wins over unsent gestures. Recovery reads never resend edits.
    if (!presetRestoreTransactionActive && routingStage == 0) scheduleLivePresetRefresh();
    return true;
}
void MidiConnection::processBlendReadback ()
{
    if (!blendWritePending) return;
    if (currentSlot != blendWriteSlot || midiOutput == nullptr) { blendWritePending = false; return; }
    if (currentPresetDataIsLive && livePresetRevision > blendWriteBaseline) {
        const auto p = GP200PresetCodec::decodeLivePresetDump (currentPresetDecodedData);
        const bool matches = p.isValid && (!restoreChainMetadataReadback
            || std::bit_cast<std::uint32_t>(p.effects[10].params[12]) == adjustmentWriteExpected) && (adjustmentWritePending
            ? std::bit_cast<std::uint32_t>(p.effects[10].params[12]) == adjustmentWriteExpected
            : (blendWriteMarked
            ? hasIndependentBlend(p) && p.effects[10].params[14] == blendWriteExpected
            : std::bit_cast<std::uint32_t>(p.effects[10].params[13]) == 0));
        blendWritePending = false;
        lastMessageText = adjustmentWritePending
            ? (matches ? "ALIGN/PHASE confirmed by pedal" : "ALIGN/PHASE not confirmed: actual pedal value recovered")
            : (matches ? "BLEND confirmed by pedal" : "BLEND not confirmed: actual pedal value recovered; requires FIX34 firmware");
        adjustmentWritePending = false; restoreChainMetadataReadback=false;
        return;
    }
    if (juce::Time::getMillisecondCounterHiRes () >= blendWriteDeadline) {
        blendWritePending = false; currentPresetDataIsLive = false;
        scheduleLivePresetRefresh ();
        lastMessageText = "BLEND readback timed out";
    }
}
bool MidiConnection::isBlendWritePending () const
{
    const juce::ScopedLock lock (stateLock);
    return blendWritePending;
}
bool MidiConnection::sendIndependentBlend (float value, bool activate, int expectedSlot)
{
    const juce::ScopedLock lock (stateLock);
    if (!std::isfinite(value) || value < 0.0f || value > 100.0f || midiOutput == nullptr
        || currentSlot != expectedSlot || !currentPresetDataIsLive || blendWritePending
        || routingStage != 0 || presetRestoreTransactionActive || modSyncActive || presetDumpSlot >= 0
        || currentStateRequestPending || irUploadPhase != IRUploadPhase::Idle || soundCloneUploadPhase != SoundCloneUploadPhase::Idle
        || !routingModeIsChain(routingModeSnapshot.mode) || routingModeSnapshot.slot != currentSlot
        || routingModeSnapshot.revision <= presetModeBaseline) return false;
    const auto p = GP200PresetCodec::decodeLivePresetDump(currentPresetDecodedData);
    if (!p.isValid || (!hasIndependentBlend(p) && !activate)) return false;
    if (!hasIndependentBlend(p) && activate) {
        // This is a deliberate slider edit: apply the requested value, not the
        // old VOL value. Loading a preset alone never initializes the signature.
        if (legacyVolumeIsBlend(p) && !sendParamChange(10,0,p.effects[10].effectId,100.0f)) return false;
    }
    if (!sendParamChange(10,14,p.effects[10].effectId,value)
        || !sendParamChange(10,13,p.effects[10].effectId,blendTagFloat())) return false;
    adjustmentWritePending=false; restoreChainMetadataReadback=false; blendWriteExpected=value; blendWriteMarked=true; blendWriteSlot=currentSlot;
    blendWriteBaseline=livePresetRevision; blendWriteDeadline=juce::Time::getMillisecondCounterHiRes()+6500.0;
    blendWritePending=true; scheduleLivePresetRefresh();
    lastMessageText="BLEND sent: awaiting pedal readback";
    return true;
}
MidiConnection::RoutingRequestSnapshot MidiConnection::getRoutingRequestSnapshot () const
{
    const juce::ScopedLock lock (stateLock);
    return { routingStage >= 1 && routingStage <= 5, routingSlot, routingOrder, routingSend, routingBoundary, routingReturn, routingValue };
}
MidiConnection::RoutingStateSnapshot MidiConnection::getRoutingStateSnapshot (bool includeSavePermission) const
{
    const juce::ScopedLock lock (stateLock);
    return { midiInput != nullptr && midiOutput != nullptr, currentSlot, currentPresetDataIsLive,
        routingModeSnapshot, currentPresetDecodedData, presetRevision, livePresetRevision, includeSavePermission && canSaveCurrentPreset (),
        routingModeSnapshot.slot == currentSlot && routingModeSnapshot.revision > presetModeBaseline };
}
void MidiConnection::timerCallback ()
{
    { const juce::ScopedLock lock(stateLock);
      if(remoteSceneDeadline>0 && juce::Time::getMillisecondCounterHiRes()>=remoteSceneDeadline) {
          remoteSceneDeadline=0;lastMessageText="Scene command not confirmed; no automatic write retry";
      }
    }
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
    processBlendReadback ();
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
bool MidiConnection::sendFlexibleRouting (const RoutingOrder& order, int send, int boundary, int ret, bool parallel, int expectedSlot, bool modeOnly)
{
    const juce::ScopedLock lock (stateLock);
    // Validation and first write are atomic with respect to MIDI receive callbacks.
    if (midiOutput == nullptr || currentSlot < 0 || (expectedSlot >= 0 && expectedSlot != currentSlot)
        || !currentPresetDataIsLive || !validFlexibleRouting (order, send, boundary, ret)
        || presetRestoreTransactionActive || modSyncActive || presetDumpSlot >= 0 || currentStateRequestPending
        || irUploadPhase != IRUploadPhase::Idle || soundCloneUploadPhase != SoundCloneUploadPhase::Idle
        || presetNameScanner.hasPendingRequest () || blendWritePending || routingStage >= 3)
    {
        lastMessageText = "SPR not sent: device state unavailable or operation busy";
        return false;
    }
    if (modeOnly)
    {
        const auto preset = GP200PresetCodec::decodeLivePresetDump (currentPresetDecodedData);
        if (routingStage != 0 || !preset.isValid || preset.routingOrder != order
            || preset.fxLoopSend != send || preset.fxLoopReturn != ret)
        { lastMessageText = "CHAIN not sent: routing changed; refresh device state"; return false; }
    }
    if (!modeOnly && !sendRoutingModeValue (1)) return false;
    presetNameScanner.cancel ();
    liveRefreshPending = false;
    routingOrder = order; routingSend = send; routingBoundary = boundary; routingReturn = ret;
    routingValue = parallel ? (0x80 | boundary) : nativeRoutingMode;
    routingSlot = currentSlot; routingGeneration = slotGeneration;
    routingStage = modeOnly ? 3 : 1;
    routingNextMs = juce::Time::getMillisecondCounterHiRes () + 150.0;
    routingDeadlineMs = routingNextMs + 6500.0;
    if (modeOnly)
    {
        routingModeBaseline = routingModeSnapshot.revision;
        routingQueryMs = routingNextMs;
        if (!sendRoutingModeValue (static_cast<juce::uint8> (routingValue)))
        { failRoutingTransaction ("mode send failed"); return false; }
    }
    routingTransactionStatus = modeOnly ? "CHAIN sent; waiting for device" : "SPR sending: Series, order, mode; waiting for device";
    return true;
}
bool MidiConnection::canSaveCurrentPreset () const
{
    const juce::ScopedLock lock (stateLock);
    const auto preset = GP200PresetCodec::decodeLivePresetDump (currentPresetDecodedData);
    const int boundary = isExtendedRoutingMode (routingModeSnapshot.mode) ? routingModeSnapshot.mode & 15 : preset.fxLoopSend;
    return preset.isValid && validFlexibleRouting (preset.routingOrder, preset.fxLoopSend, boundary, preset.fxLoopReturn)
        && midiOutput != nullptr && currentSlot >= 0 && currentPresetDataIsLive && routingStage == 0
        && juce::Time::getMillisecondCounterHiRes()>=remoteSceneDeadline
        && !blendWritePending && !presetRestoreTransactionActive && !currentStateRequestPending && presetDumpSlot < 0
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
    const bool modeMatches = routingModeSnapshot.mode == routingValue;
    if (routingStage == 3 && freshMode && modeMatches)
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
    if (routingStage == 5 && freshMode && modeMatches)
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
    if (routingModeSnapshot.slot != slot) { nativeRoutingMode = 0; blendWritePending = false; }
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
        if (currentPresetDataIsLive && presetDumpSlot == currentSlot) { ++livePresetRevision; sceneRefreshPending=false; }
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
    if (sceneRefreshPending || juce::Time::getMillisecondCounterHiRes()<remoteSceneDeadline) { lastMessageText="Scene changed: waiting for fresh pedal state"; return false; }
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

    if (nowMs < liveRefreshDueMs || (routingStage == 0 && nowMs < lastLiveRefreshSentMs + 350.0) || nowMs < presetReadResumeMs
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
    if (sendLiveReadRequestForSlot (currentSlot)) lastLiveRefreshSentMs = nowMs;
}
void MidiConnection::beginPresetRestoreTransaction ()
{
    const juce::ScopedLock lock (stateLock);

    blendWritePending = false; adjustmentWritePending=false; restoreChainMetadataReadback=false; sceneRefreshPending=false;
    remoteSceneStatus=-1; remoteSceneSlot=-1; remoteSceneDeadline=0;
    if (routingStage != 0) failRoutingTransaction ("Recall started");
    currentPresetDataIsLive = false;
    routingModeSnapshot.mode = -1; ++routingModeSnapshot.revision;
    presetRestoreTransactionActive = true;
    presetRestoreSlotGeneration = slotGeneration;
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
void MidiConnection::endPresetRestoreTransaction (int expectedRoutingMode)
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
    const auto restoredBlend = GP200PresetCodec::decodeLivePresetDump (currentPresetDecodedData);
    if (restoredBlend.isValid) {
        const auto savedAlign=std::bit_cast<std::uint32_t>(restoredBlend.effects[10].params[12]);
        adjustmentWriteExpected=validAlignWord(savedAlign)?savedAlign:0u;
        adjustmentWritePending=false; restoreChainMetadataReadback=true;
        blendWriteMarked = hasIndependentBlend (restoredBlend);
        blendWriteExpected = restoredBlend.effects[10].params[14];
        blendWriteSlot = currentSlot; blendWriteBaseline = livePresetRevision;
        blendWriteDeadline = juce::Time::getMillisecondCounterHiRes () + 6500.0;
        blendWritePending = true;
    }
    currentPresetDataIsLive = false;
    startupHandshakePhase = StartupHandshakePhase::Ready;
    currentStateRequestQueued = true;
    if (validRoutingModeValue (expectedRoutingMode))
    {
        const auto preset = GP200PresetCodec::decodeLivePresetDump (currentPresetDecodedData);
        routingSlot = currentSlot; routingGeneration = slotGeneration;
        routingOrder = preset.routingOrder; routingSend = preset.fxLoopSend; routingReturn = preset.fxLoopReturn;
        routingValue = expectedRoutingMode;
        routingBoundary = isExtendedRoutingMode (expectedRoutingMode) ? expectedRoutingMode & 15 : routingSend;
        routingLiveBaseline = livePresetRevision;
        routingModeBaseline = routingModeSnapshot.revision;
        routingStage = 4; // Already sent by Recall: verify a new live preset, then query the mode.
        routingDeadlineMs = juce::Time::getMillisecondCounterHiRes () + 6500.0;
        routingQueryMs = 0;
        routingTransactionStatus = "Recall: waiting for Series/Parallel/P and order confirmation";
        if (!preset.isValid || !validFlexibleRouting (routingOrder, routingSend, routingBoundary, routingReturn))
            failRoutingTransaction ("invalid Recall routing snapshot");
    }
}
bool MidiConnection::sendPresetRestoreRoutingMode (int expectedSlot, int mode)
{
    const juce::ScopedLock lock (stateLock);
    if (!presetRestoreTransactionActive || currentSlot != expectedSlot
        || slotGeneration != presetRestoreSlotGeneration || !validRoutingModeValue (mode))
    {
        lastMessageText = "Recall routing not sent: slot changed or invalid mode";
        return false;
    }
    return sendRoutingModeValue (static_cast<juce::uint8> (mode));
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


using namespace gp200;
juce::MemoryBlock remoteData(){juce::MemoryBlock b;b.setSize(1176);b.v[106]=2;b.v[107]=8;for(int i=0;i<11;i++)b.v[108+i]=i;return b;}
std::array<std::uint8_t,46> status(int slot,int scene,int marker=0x52){
 std::array<std::uint8_t,46>d{};std::array<std::uint8_t,13>h{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,16,0,0,0};std::copy(h.begin(),h.end(),d.begin());
 std::array<std::uint8_t,16>p{5,0,12,0,10,15,0,0,0,0,0,0,static_cast<std::uint8_t>(scene),static_cast<std::uint8_t>(slot),static_cast<std::uint8_t>(marker),0x53};
 for(int i=0;i<16;i++){d[13+2*i]=p[i]>>4;d[14+2*i]=p[i]&15;}d[45]=0xf7;return d;
}
int main(){int checks=0;juce::Time::now=1000;
 MidiConnection m;m.currentPresetDecodedData=remoteData();
 assert(!m.sendSceneCommand(0)&&!m.sendSceneCommand(2)&&m.output.messages.empty());++checks;
 auto old=status(0,0,0x43);m.handleSceneChangeNotification(old.data(),46);assert(m.getRemoteSceneStatus()==-1);++checks;
 auto patch=status(0,4);m.handleSceneChangeNotification(patch.data(),46);m.currentPresetDataIsLive=true;m.sceneRefreshPending=false;
 assert(m.getRemoteSceneStatus()==4 && !m.sendSceneCommand(7));++checks;
 assert(m.sendSceneCommand(2));assert(m.isSceneCommandPending()&&!m.canSaveCurrentPreset());
 auto bytes=m.output.messages.back().bytes;assert(bytes.size()==62);
 auto p=MidiConnection::nibbleDecode(bytes.data()+13,48);assert(p[12]==10&&p[13]==15);std::uint32_t w;std::memcpy(&w,p.data()+20,4);assert(w==0x53430002);++checks;
 assert(!m.sendSceneCommand(2));++checks;
 auto scene=status(0,2);m.handleSceneChangeNotification(scene.data(),46);assert(m.getRemoteSceneStatus()==2&&!m.isSceneCommandPending()&&m.sceneRefreshPending);++checks;
 m.currentPresetDataIsLive=true;m.sceneRefreshPending=false;juce::Time::now=1600;
 assert(m.sendSceneCommand(9));bytes=m.output.messages.back().bytes;p=MidiConnection::nibbleDecode(bytes.data()+13,48);std::memcpy(&w,p.data()+20,4);assert(w==0x53430009);++checks;
 m.currentSlot=1;assert(m.getRemoteSceneStatus()==-1&&!m.sendSceneCommand(2));++checks;
 for(int slot=0;slot<256;slot++)for(int value=0;value<=5;value++) {auto n=status(slot,value);assert(sceneChangeSlot(n.data(),46)==slot&&sceneStatus(n.data(),46)==value);++checks;}
 auto n=status(0,5);for(int i=0;i<46;i++){auto bad=n;bad[i]^=0x80;assert(sceneChangeSlot(bad.data(),46)==-1);++checks;}
 std::cout<<checks<<" Scene Remote production protocol/sender checks PASS (JUCE ports and preset codec modeled)\n";
}
