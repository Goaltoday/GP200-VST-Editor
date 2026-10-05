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
#include <map>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include "../source/libgp200/GP200ChainBlend.h"
namespace juce {
using uint8=std::uint8_t;using uint32=std::uint32_t;constexpr int dontSendNotification=0;using uint32=std::uint32_t;
template<class T>T jlimit(T a,T b,T v){return std::clamp(v,a,b);}

struct String:std::string {using std::string::string;using std::string::operator=;String(int n):std::string(std::to_string(n)){}String(std::string s):std::string(s){}bool isEmpty()const{return empty();}bool isNotEmpty()const{return !empty();}String trim()const{auto a=find_first_not_of(" \r\n\t");return a==npos?String{}:String(substr(a,find_last_not_of(" \r\n\t")-a+1));}String substring(int a,int b)const{return substr(a,std::max(0,b-a));}};
struct MemoryBlock {std::vector<uint8> v;void setSize(size_t n){v.resize(n);}size_t getSize()const{return v.size();}void* getData(){return v.data();}const void* getData()const{return v.data();}void append(const void*p,size_t n){auto b=(const uint8*)p;v.insert(v.end(),b,b+n);}String toBase64Encoding()const{static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";String s;unsigned buffer=0;int bits=-6;for(auto c:v){buffer=(buffer<<8)|c;bits+=8;while(bits>=0){s+=chars[(buffer>>bits)&63];bits-=6;}}if(bits>-6)s+=chars[((buffer<<8)>>(bits+8))&63];while(s.size()%4)s+='=';return s;}bool fromBase64Encoding(const String& s){static const std::string chars="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";v.clear();unsigned buffer=0;int bits=-8;for(char c:s){if(c=='=')break;auto i=chars.find(c);if(i==std::string::npos){v.clear();return false;}buffer=(buffer<<6)|unsigned(i);bits+=6;if(bits>=0){v.push_back((buffer>>bits)&255);bits-=8;}}return true;}};
struct XmlElement {String tag;std::map<std::string,String> attrs;XmlElement(String s):tag(s){}void setAttribute(String k,String v){attrs[k]=v;}void setAttribute(String k,int v){attrs[k]=String(v);}int getIntAttribute(String k,int d)const{auto i=attrs.find(k);if(i==attrs.end())return d;try{return std::stoi(i->second);}catch(...){return d;}}String getStringAttribute(String k,String d)const{auto i=attrs.find(k);return i==attrs.end()?d:i->second;}bool getBoolAttribute(String k,bool d)const{return getIntAttribute(k,int(d))!=0;}bool hasAttribute(String k)const{return attrs.count(k)>0;}bool hasTagName(String t)const{return tag==t;}};
void copyXmlToBinary(const XmlElement& xml,MemoryBlock& b){std::ostringstream out;out<<std::quoted(std::string(xml.tag))<<'\n';for(auto& [k,v]:xml.attrs)out<<std::quoted(k)<<' '<<std::quoted(std::string(v))<<'\n';auto text=out.str();b.v.assign(text.begin(),text.end());}
std::unique_ptr<XmlElement> getXmlFromBinary(const void* p,int n){std::string s((const char*)p,n);std::istringstream in(s);std::string tag,k,v;if(!(in>>std::quoted(tag)))return {};auto xml=std::make_unique<XmlElement>(tag);while(in>>std::quoted(k)>>std::quoted(v))xml->attrs[k]=v;return xml;}
struct CriticalSection {mutable std::recursive_mutex m;};struct ScopedLock{const CriticalSection& c;ScopedLock(const CriticalSection& x):c(x){c.m.lock();}~ScopedLock(){c.m.unlock();}};
struct Time {static inline double now=0;static double getMillisecondCounterHiRes(){return now;}};
struct MidiMessage {std::vector<uint8> bytes;static MidiMessage createSysExMessage(const uint8*p,int n){MidiMessage m;m.bytes.push_back(0xf0);m.bytes.insert(m.bytes.end(),p,p+n);m.bytes.push_back(0xf7);return m;}};
}
namespace gp200 {
using RoutingOrder=std::array<int,11>;
struct GP200EffectSlot {int blockIndex=0,slotIndex=0;bool enabled=true;juce::uint32 effectId=0x06000003;std::array<float,15> params{};};
struct GP200Preset {juce::String patchName{"preset"};std::array<GP200EffectSlot,11> effects{};bool isValid=false;RoutingOrder routingOrder{};int fxLoopSend=0,fxLoopReturn=0;};
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
double currentStateRequestSentMs=0,startupHandshakePhaseStartedMs=0,liveRefreshDueMs=0;
std::vector<std::vector<juce::uint8>> presetReadChunks,stateDumpChunks;juce::MemoryBlock currentPresetDecodedData;
struct RoutingRequestSnapshot {bool active=false;int slot=-1;RoutingOrder order{};int send=0,boundary=0,ret=0,mode=1;};
struct RoutingModeSnapshot{int mode=0x85,slot=0;std::uint64_t revision=1;};RoutingModeSnapshot routingModeSnapshot;
struct RoutingStateSnapshot {bool connected=false;int slot=-1;bool live=false;RoutingModeSnapshot mode;juce::MemoryBlock data;std::uint64_t presetRevision=0,liveRevision=0;bool canSave=false,modeFresh=false;};
bool isIRUploadInProgress(){return irUploadPhase!=IRUploadPhase::Idle;}bool isSoundCloneUploadInProgress(){return soundCloneUploadPhase!=SoundCloneUploadPhase::Idle;}juce::String getLastMessageText(){return lastMessageText;}
std::uint64_t presetRevision=1,livePresetRevision=1;
juce::String currentPresetName="test",currentPresetDumpStatusText,lastMessageText;
int nativeRoutingMode=0;bool blendWritePending=false,blendWriteMarked=false;int blendWriteSlot=-1;std::uint64_t blendWriteBaseline=0;float blendWriteExpected=50.0f;double blendWriteDeadline=0;
std::vector<std::pair<int,float>> parameterWrites;
bool sendParamChange(int block,int param,juce::uint32,float value){parameterWrites.push_back({param,value});std::memcpy(currentPresetDecodedData.v.data()+effectBlockStart+block*effectBlockSize+paramsOffset+param*4,&value,4);return true;}
int routingStage=0,routingSlot=-1,routingSend=0,routingBoundary=0,routingReturn=0,routingValue=1;RoutingOrder routingOrder{};
std::uint64_t routingGeneration=0,slotGeneration=0,routingModeBaseline=0,routingLiveBaseline=0,presetModeBaseline=0,presetRestoreSlotGeneration=0;
double routingNextMs=0,routingDeadlineMs=0,routingQueryMs=0,presetReadStartedMs=0,presetReadResumeMs=0,modePollMs=0;int presetReadRetries=0;bool presetReadIsLive=false;
juce::String routingTransactionStatus="SPR: idle";
int irTicks=0,cloneTicks=0,scanTicks=0;bool stopped=false;
int getCurrentSlot()const{return currentSlot;}
bool sendPatchVolume(int){return true;}bool sendPatchTempoBpm(int){return true;}bool sendEffectChange(int,juce::uint32){return true;}bool sendEffectOnOff(int,bool){return true;}
void adoptCurrentPresetSnapshot(int slot,juce::String name,juce::MemoryBlock data){currentSlot=slot;currentPresetName=name;currentPresetDecodedData=data;currentPresetDataIsLive=false;++presetRevision;}
bool isConnected()const{return midiOutput!=nullptr;}void stopTimer(){stopped=true;}void processIRUpload(){irTicks++;}void processSoundCloneUpload(){cloneTicks++;}void finishModSyncFailure(juce::String){modSyncActive=false;}bool requestCurrentPresetFromGP200(){return sendStateDumpRequestUnlocked();}void processStartupHandshake(){}void processPresetNameScan(){scanTicks++;}
static juce::String sanitizePresetNameForStore(juce::String x){return x;}static std::vector<juce::uint8> buildStorePresetCommit(int slot,juce::String){return {0xf0,(juce::uint8)slot,0xf7};}
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
void MidiConnection::processBlendReadback ()
{
    if (!blendWritePending) return;
    if (currentSlot != blendWriteSlot || midiOutput == nullptr) { blendWritePending = false; return; }
    if (currentPresetDataIsLive && livePresetRevision > blendWriteBaseline) {
        const auto p = GP200PresetCodec::decodeLivePresetDump (currentPresetDecodedData);
        const bool matches = p.isValid && (blendWriteMarked
            ? hasIndependentBlend(p) && p.effects[10].params[14] == blendWriteExpected
            : std::bit_cast<std::uint32_t>(p.effects[10].params[13]) == 0);
        blendWritePending = false;
        lastMessageText = matches ? "BLEND confirmed by pedal" : "BLEND not confirmed: actual pedal value recovered; requires FIX34 firmware";
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
    blendWriteExpected=value; blendWriteMarked=true; blendWriteSlot=currentSlot;
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

    blendWritePending = false;
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


using juce::copyXmlToBinary;using juce::getXmlFromBinary;
juce::MemoryBlock serialiseOfflineState(gp200::GP200Preset,int,int,int){return {};}
bool deserialiseOfflineState(juce::MemoryBlock,gp200::GP200Preset&,int&,int&,int&){return false;}
struct AudioPluginAudioProcessor {
 struct GP200PresetSnapshot {int slot=-1;juce::String name="unknown",displayName;juce::MemoryBlock data;int routingMode=-1;std::uint64_t revision=0;};
 struct GP200PresetRecallSnapshot {juce::MemoryBlock data;juce::String name;int routingMode=-1;};
 std::array<GP200PresetSnapshot,2> savedGP200PresetSnapshots;mutable juce::CriticalSection stateLock;
 int savedGP200Slot=-1,offlinePatchVolume=50,offlinePatchPan=0,offlinePatchTempo=120;juce::String savedGP200PresetName="unknown";gp200::GP200Preset offlinePreset;bool offlinePresetDirty=false;std::uint64_t offlinePresetRevision=0;
 static bool isValidSnapshotIndex(int i){return i>=0&&i<2;}static bool isUsefulPresetName(juce::String s){return !s.trim().empty()&&s!="unknown";}
 void setGP200PresetSnapshotState(int,int,const juce::String&,const juce::MemoryBlock&,int=-1);
 GP200PresetRecallSnapshot getGP200PresetRecallSnapshot(int)const;
 void getStateInformation(juce::MemoryBlock&);void setStateInformation(const void*,int);bool hasSavedGP200PresetData(int)const;
 void notifyOfflineStateChanged(){}
};
struct AudioPluginAudioProcessorEditor {
 AudioPluginAudioProcessor& processorRef;gp200::MidiConnection& midiConnection;explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p,gp200::MidiConnection& m):processorRef(p),midiConnection(m){}
 int selected=0;static constexpr int idleTimerHz=20,restoreTimerHz=100;
 struct Slider {int value=50;int getValue(){return value;}void setValue(int v,int){value=v;}}patchVolumeSlider,panSlider,tempoSlider;
 struct Edit {juce::String text="preset";juce::String getText(){return text;}void setText(juce::String s,int){text=s;}}presetNameEditor;
 gp200::GP200Preset offlinePreset;bool offlinePresetDirty=false;std::uint64_t offlinePresetRevision=0;int offlinePatchVolume=50,offlinePatchPan=0,offlinePatchTempo=120;
 juce::String effectsStatusText,effectBlocksSignature,effectBlocksDataSignature,patchVolumeSourceSignature,presetNameEditorSignature;
 enum class PresetRestoreStepType {PatchVolume,PatchTempo,EffectChange,ParamChange,ToggleEffect,ReorderEffects,RoutingMode};
 struct PresetRestoreStep {PresetRestoreStepType type=PresetRestoreStepType::ParamChange;int blockIndex=-1,paramIndex=-1;juce::uint32 effectId=0;float value=0;bool shouldBeOn=false;gp200::RoutingOrder routingOrder{};int fxLoopSend=4,fxLoopReturn=4,routingMode=-1;};
 std::vector<PresetRestoreStep> presetRestoreSteps;int presetRestoreStepIndex=0;bool presetRestoreInProgress=false;
 juce::MemoryBlock presetRestoreSnapshotData;int presetRestoreSlot=-1;juce::String presetRestoreName;int presetRestoreRoutingMode=-1;bool presetRestoreRoutingMetadataFromDaw=false;double presetRestoreRoutingNotBeforeMs=0;
 bool hasSavedBlockEnabledStates=false,allBlocksAreTemporarilyOff=false;int savedBlockEnabledSlot=-1;
 int getSelectedCompareSnapshotIndex(){return selected;}juce::String getSelectedCompareSnapshotLabel(){return selected==0?"A":"B";}
 static bool isUsefulPresetName(juce::String s){return !s.empty()&&s!="unknown";}
 static juce::MemoryBlock serialiseOfflineSnapshot(gp200::GP200Preset,int,int,int){juce::MemoryBlock b;b.setSize(100);return b;}
 static bool deserialiseOfflineSnapshot(juce::MemoryBlock,gp200::GP200Preset&,int&,int&,int&){return false;}
 void repaint(){}void startTimerHz(int){}void updateSnapshotNameEditor(){}void updateEffectBlocksUI(){}void updateAllBlocksOffButtonText(){}
 // The parameter/effect replay is unchanged production code; only represent its final reorder here.
 void buildFullPresetRestoreSteps(gp200::GP200Preset p,juce::MemoryBlock){presetRestoreSteps.clear();PresetRestoreStep s;s.type=PresetRestoreStepType::ReorderEffects;s.routingOrder=p.routingOrder;s.fxLoopSend=p.fxLoopSend;s.fxLoopReturn=p.fxLoopReturn;presetRestoreSteps.push_back(s);}
 void saveCurrentPresetToProject();void startFullPresetRestoreFromSnapshot();void processFullPresetRestoreStep();void finishFullPresetRestore();
};
void AudioPluginAudioProcessor::setGP200PresetSnapshotState (
    int snapshotIndex,
    int slot,
    const juce::String& presetName,
    const juce::MemoryBlock& presetData,
    int routingMode)
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

    // A newly saved snapshot starts with a useful independent display name.
    // The original preset name remains in snapshot.name for legacy behaviour,
    // while displayName is what the A/B UI shows and allows the user to edit.
    snapshot.displayName = isUsefulPresetName (presetName)
                               ? presetName.trim ()
                               : juce::String (snapshotIndex == 0 ? "Snapshot A" : "Snapshot B");

    snapshot.data = presetData;
    // Do not inherit routing when replacing a snapshot with offline/old data.
    snapshot.routingMode = slot >= 0 && presetData.getSize () > 0 && gp200::validRoutingModeValue (routingMode)
        ? routingMode : -1;
    ++snapshot.revision;
}
AudioPluginAudioProcessor::GP200PresetRecallSnapshot AudioPluginAudioProcessor::getGP200PresetRecallSnapshot (int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex)) return {};
    const juce::ScopedLock lock (stateLock);
    const auto& snapshot = savedGP200PresetSnapshots[static_cast<std::size_t> (snapshotIndex)];
    return { snapshot.data, snapshot.name, snapshot.routingMode };
}
void AudioPluginAudioProcessor::getStateInformation (
    juce::MemoryBlock& destData)
{
    auto xml =
        std::make_unique<juce::XmlElement> ("GP200StudioState");

    {
        const juce::ScopedLock lock (stateLock);

        xml->setAttribute ("version", 8);

        xml->setAttribute ("slotReferenceSlot", savedGP200Slot);
        xml->setAttribute ("slotReferenceName",
                           savedGP200PresetName);

        const auto& snapshotA = savedGP200PresetSnapshots[0];
        const auto& snapshotB = savedGP200PresetSnapshots[1];

        xml->setAttribute ("snapshotASlot", snapshotA.slot);
        xml->setAttribute ("snapshotARoutingMode", snapshotA.routingMode);
        xml->setAttribute ("snapshotAName", snapshotA.name);
        xml->setAttribute ("snapshotADisplayName", snapshotA.displayName);
        xml->setAttribute (
            "snapshotADataBase64",
            snapshotA.data.toBase64Encoding ());

        xml->setAttribute ("snapshotBSlot", snapshotB.slot);
        xml->setAttribute ("snapshotBRoutingMode", snapshotB.routingMode);
        xml->setAttribute ("snapshotBName", snapshotB.name);
        xml->setAttribute ("snapshotBDisplayName", snapshotB.displayName);
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
        snapshot.displayName.clear ();
        snapshot.data.setSize (0);
        snapshot.routingMode = -1;
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

        snapshotA.displayName =
            xml->getStringAttribute (
                "snapshotADisplayName",
                snapshotA.name);

        const auto snapshotADataBase64 =
            xml->getStringAttribute (
                "snapshotADataBase64",
                {});

        if (snapshotADataBase64.isNotEmpty ())
        {
            snapshotA.data.fromBase64Encoding (
                snapshotADataBase64);
        }
        const int modeA = xml->getIntAttribute ("snapshotARoutingMode", -1);
        snapshotA.routingMode = snapshotA.slot >= 0 && snapshotA.data.getSize () > 0
            && gp200::validRoutingModeValue (modeA) ? modeA : -1;

        snapshotB.slot =
            xml->getIntAttribute ("snapshotBSlot", -1);

        snapshotB.name =
            xml->getStringAttribute (
                "snapshotBName",
                "unknown");

        snapshotB.displayName =
            xml->getStringAttribute (
                "snapshotBDisplayName",
                snapshotB.name);

        const auto snapshotBDataBase64 =
            xml->getStringAttribute (
                "snapshotBDataBase64",
                {});

        if (snapshotBDataBase64.isNotEmpty ())
        {
            snapshotB.data.fromBase64Encoding (
                snapshotBDataBase64);
        }
        const int modeB = xml->getIntAttribute ("snapshotBRoutingMode", -1);
        snapshotB.routingMode = snapshotB.slot >= 0 && snapshotB.data.getSize () > 0
            && gp200::validRoutingModeValue (modeB) ? modeB : -1;

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
    snapshotA.displayName = snapshotA.name;

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
bool AudioPluginAudioProcessor::hasSavedGP200PresetData (
    int snapshotIndex) const
{
    if (!isValidSnapshotIndex (snapshotIndex))
        return false;

    const juce::ScopedLock lock (stateLock);

    return savedGP200PresetSnapshots[
        static_cast<std::size_t> (snapshotIndex)].data.getSize () > 0;
}
void AudioPluginAudioProcessorEditor::saveCurrentPresetToProject ()
{
    if (midiConnection.isConnected () && !midiConnection.canSaveCurrentPreset ())
    { effectsStatusText = "Save/export blocked: wait for fresh device preset and routing confirmation"; repaint (); return; }

    const auto snapshotIndex = getSelectedCompareSnapshotIndex ();

    if (!midiConnection.isConnected ())
    {
        const auto visiblePresetName =
            presetNameEditor.getText ().trim ().substring (0, gp200::presetNameMaxLength);

        if (visiblePresetName.isNotEmpty ())
            offlinePreset.patchName = visiblePresetName;

        const auto snapshotData = serialiseOfflineSnapshot (
            offlinePreset,
            static_cast<int> (patchVolumeSlider.getValue ()),
            static_cast<int> (panSlider.getValue ()),
            static_cast<int> (tempoSlider.getValue ()));

        processorRef.setGP200PresetSnapshotState (
            snapshotIndex,
            -1,
            offlinePreset.patchName,
            snapshotData);

        offlinePresetDirty = false;
        effectsStatusText = "Saved offline preset snapshot " +
                            getSelectedCompareSnapshotLabel () +
                            " to DAW";
        updateSnapshotNameEditor ();
        updateEffectBlocksUI ();
        repaint ();
        return;
    }

    const auto deviceState = midiConnection.getRoutingStateSnapshot ();
    if (!deviceState.canSave) { effectsStatusText = "Save blocked: device state changed"; repaint (); return; }
    const auto currentSlot = deviceState.slot;
    const auto presetData = deviceState.data;
    const auto preset = gp200::GP200PresetCodec::decodeLivePresetDump (presetData);

    if (presetData.getSize () == 0)
    {
        effectsStatusText = "Save Preset failed: no full preset data captured yet";
        repaint ();
        return;
    }

    processorRef.setGP200PresetSnapshotState (
        snapshotIndex,
        currentSlot,
        preset.patchName,
        presetData,
        deviceState.mode.mode);

    effectsStatusText = "Saved full preset snapshot " +
                        getSelectedCompareSnapshotLabel () +
                        " to DAW";
    updateSnapshotNameEditor ();
    updateEffectBlocksUI ();
    repaint ();
}
void AudioPluginAudioProcessorEditor::startFullPresetRestoreFromSnapshot ()
{
    if (presetRestoreInProgress)
    {
        effectsStatusText = "Recall Preset: restore already in progress";
        repaint ();
        return;
    }
	const auto snapshotIndex =
    getSelectedCompareSnapshotIndex ();

const auto snapshotLabel =
    getSelectedCompareSnapshotLabel ();

    if (!processorRef.hasSavedGP200PresetData (
        snapshotIndex))
    {
        effectsStatusText =
    "Recall snapshot " + snapshotLabel +
    " failed: no preset saved";
        repaint ();
        return;
    }

    const auto savedSnapshot = processorRef.getGP200PresetRecallSnapshot (snapshotIndex);
    const auto presetData = savedSnapshot.data;

    if (presetData.getSize () == 0)
    {
        effectsStatusText = "Recall Preset failed: saved preset data is empty";
        repaint ();
        return;
    }

    if (!midiConnection.isConnected ())
    {
        int restoredVolume = 50;
        int restoredPan = 0;
        int restoredTempo = 120;
        gp200::GP200Preset restoredPreset;

        if (!deserialiseOfflineSnapshot (presetData,
                                         restoredPreset,
                                         restoredVolume,
                                         restoredPan,
                                         restoredTempo))
        {
            effectsStatusText =
                "Recall snapshot " + snapshotLabel +
                " failed: it was saved from a connected GP-200";
            repaint ();
            return;
        }

        offlinePreset = std::move (restoredPreset);
        offlinePresetDirty = false;
        ++offlinePresetRevision;

        offlinePatchVolume = restoredVolume;
        offlinePatchPan = restoredPan;
        offlinePatchTempo = restoredTempo;
        patchVolumeSlider.setValue (offlinePatchVolume, juce::dontSendNotification);
        panSlider.setValue (offlinePatchPan, juce::dontSendNotification);
        tempoSlider.setValue (offlinePatchTempo, juce::dontSendNotification);
        presetNameEditor.setText (offlinePreset.patchName, juce::dontSendNotification);

        effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
        patchVolumeSourceSignature.clear ();
        presetNameEditorSignature.clear ();

        processorRef.notifyOfflineStateChanged ();
        effectsStatusText = "Recalled offline preset snapshot " +
                            snapshotLabel +
                            " from DAW";
        updateEffectBlocksUI ();
        repaint ();
        return;
    }

    const auto preset = gp200::GP200PresetCodec::decodeLivePresetDump (presetData);

    if (!preset.isValid)
    {
        effectsStatusText = "Recall Preset failed: saved preset data could not be decoded";
        repaint ();
        return;
    }

    const auto targetSlot = midiConnection.getCurrentSlot ();

    if (targetSlot < 0 || targetSlot > 255)
    {
        effectsStatusText = "Recall Preset failed: current GP-200 slot is unknown";
        repaint ();
        return;
    }

    auto savedName = savedSnapshot.name;

    if (!isUsefulPresetName (savedName))
        savedName = preset.patchName;

    // Important: do not change slot here.
    // Recall Preset restores the saved sound into the currently selected GP-200 slot,
    // so the user can then press Store preset and save it wherever they are.
    presetRestoreSnapshotData = presetData;
    presetRestoreSlot = targetSlot;
    presetRestoreName = savedName;

    presetRestoreRoutingMetadataFromDaw = true;
    presetRestoreRoutingMode = savedSnapshot.routingMode;
    presetRestoreRoutingNotBeforeMs = 0;
    if (gp200::validRoutingModeValue (presetRestoreRoutingMode))
    {
        const int boundary = gp200::isExtendedRoutingMode (presetRestoreRoutingMode)
            ? presetRestoreRoutingMode & 15 : preset.fxLoopSend;
        if (!gp200::validFlexibleRouting (preset.routingOrder, preset.fxLoopSend, boundary, preset.fxLoopReturn))
        {
            effectsStatusText = "Recall Preset failed: saved routing mode/P does not match Send/Return";
            repaint (); return;
        }
    }
    buildFullPresetRestoreSteps (preset, presetData);
    if (gp200::validRoutingModeValue (presetRestoreRoutingMode))
    {
        PresetRestoreStep modeStep;
        modeStep.type = PresetRestoreStepType::RoutingMode;
        modeStep.routingMode = presetRestoreRoutingMode;
        presetRestoreSteps.push_back (modeStep);
    }

    if (presetRestoreSteps.empty ())
    {
        effectsStatusText = "Recall Preset failed: no restore steps were generated";
        repaint ();
        return;
    }

    presetRestoreStepIndex = 0;
    midiConnection.beginPresetRestoreTransaction ();
    presetRestoreInProgress = true;
    startTimerHz (restoreTimerHz);

    effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
    patchVolumeSourceSignature.clear ();
    presetNameEditorSignature.clear ();

    effectsStatusText = "Recall Preset: restoring snapshot into current slot 0/" +
                        juce::String (static_cast<int> (presetRestoreSteps.size ()));

    updateEffectBlocksUI ();
    repaint ();
}
void AudioPluginAudioProcessorEditor::processFullPresetRestoreStep ()
{
    if (!presetRestoreInProgress)
        return;

    if (presetRestoreStepIndex >= static_cast<int> (presetRestoreSteps.size ()))
    {
        finishFullPresetRestore ();
        return;
    }

    const auto& step = presetRestoreSteps[static_cast<std::size_t> (presetRestoreStepIndex)];

    // The final routing write follows the existing reorder, with the same pacing as SPR.
    if (step.type == PresetRestoreStepType::RoutingMode
        && juce::Time::getMillisecondCounterHiRes () < presetRestoreRoutingNotBeforeMs) return;

    bool sent = false;

    switch (step.type)
    {
    case PresetRestoreStepType::PatchVolume:
        sent = midiConnection.sendPatchVolume (static_cast<int> (std::round (step.value)));
        break;

    case PresetRestoreStepType::PatchTempo:
        sent = midiConnection.sendPatchTempoBpm (static_cast<int> (std::round (step.value)));
        break;

    case PresetRestoreStepType::EffectChange:
        sent = midiConnection.sendEffectChange (step.blockIndex, step.effectId);
        break;

    case PresetRestoreStepType::ParamChange:
        sent = midiConnection.sendParamChange (step.blockIndex, step.paramIndex, step.effectId, step.value);
        break;

    case PresetRestoreStepType::ToggleEffect:
        sent = midiConnection.sendEffectOnOff (step.blockIndex, step.shouldBeOn);
        break;

    case PresetRestoreStepType::ReorderEffects:
        sent = midiConnection.sendReorderEffects (step.routingOrder, step.fxLoopSend, step.fxLoopReturn);
        if (sent && gp200::validRoutingModeValue (presetRestoreRoutingMode))
            presetRestoreRoutingNotBeforeMs = juce::Time::getMillisecondCounterHiRes () + 150.0;
        break;

    case PresetRestoreStepType::RoutingMode:
        sent = midiConnection.sendPresetRestoreRoutingMode (presetRestoreSlot, step.routingMode);
        break;
    }

    if (!sent)
    {
        presetRestoreInProgress = false;
        midiConnection.endPresetRestoreTransaction ();
        startTimerHz (idleTimerHz);
        effectsStatusText = "Recall Preset failed: " + midiConnection.getLastMessageText ();
        return;
    }

    ++presetRestoreStepIndex;

    effectsStatusText = "Recall Preset: restoring snapshot into current slot " +
                        juce::String (presetRestoreStepIndex) + "/" +
                        juce::String (static_cast<int> (presetRestoreSteps.size ()));

    if (presetRestoreStepIndex >= static_cast<int> (presetRestoreSteps.size ()))
        finishFullPresetRestore ();
}
void AudioPluginAudioProcessorEditor::finishFullPresetRestore ()
{
    presetRestoreInProgress = false;
    startTimerHz (idleTimerHz);

    midiConnection.adoptCurrentPresetSnapshot (
        presetRestoreSlot, presetRestoreName, presetRestoreSnapshotData);
    midiConnection.endPresetRestoreTransaction (presetRestoreRoutingMode);

    presetRestoreSteps.clear ();
    presetRestoreStepIndex = 0;

    effectBlocksSignature.clear ();
        effectBlocksDataSignature.clear ();
    patchVolumeSourceSignature.clear ();
    presetNameEditorSignature.clear ();

    hasSavedBlockEnabledStates = false;
    allBlocksAreTemporarilyOff = false;
    savedBlockEnabledSlot = -1;
    updateAllBlocksOffButtonText ();

    if (gp200::validRoutingModeValue (presetRestoreRoutingMode))
        effectsStatusText = "Recall Preset: preset and Series/Parallel/P sent; waiting for device confirmation";
    else if (presetRestoreRoutingMetadataFromDaw)
        effectsStatusText = "Recall Preset: preset restored; this snapshot has no saved Series/Parallel/P metadata";
    else
        effectsStatusText = "Recall Preset: snapshot restored into current slot. Press Store preset to save it here.";
    presetRestoreRoutingMetadataFromDaw = false;
    presetRestoreRoutingMode = -1;
    presetRestoreRoutingNotBeforeMs = 0;

    updateEffectBlocksUI ();
}

using namespace gp200;
juce::MemoryBlock data(int s=2,int r=8){juce::MemoryBlock b;b.setSize(1176);auto p=(juce::uint8*)b.getData();p[106]=s;p[107]=r;for(int i=0;i<11;i++)p[108+i]=i;p[16]=71;return b;}
void init(MidiConnection& m,int mode=0x85,int s=2,int r=8){m.currentPresetDecodedData=data(s,r);m.routingModeSnapshot.mode=mode;juce::Time::now=0;}
void fresh(MidiConnection& m,const juce::MemoryBlock& wanted,int value){m.currentPresetDecodedData=wanted;m.currentPresetDataIsLive=true;++m.livePresetRevision;++m.presetRevision;m.processRoutingTransaction();assert(m.routingStage==5&&!m.canSaveCurrentPreset());m.routingModeSnapshot.mode=value;m.routingModeSnapshot.slot=m.currentSlot;++m.routingModeSnapshot.revision;m.processRoutingTransaction();m.processBlendReadback();assert(m.routingStage==0&&m.canSaveCurrentPreset());}
int main(){int count=0;
for(int slot:{0,1})for(int mode=0;mode<256;mode++) if(validRoutingModeValue(mode)){AudioPluginAudioProcessor p;MidiConnection m;init(m,mode,0,11);AudioPluginAudioProcessorEditor e(p,m);e.selected=slot;e.saveCurrentPresetToProject();auto saved=p.getGP200PresetRecallSnapshot(slot);assert(saved.routingMode==mode&&saved.data.v==m.currentPresetDecodedData.v);assert(p.getGP200PresetRecallSnapshot(1-slot).routingMode==-1);juce::MemoryBlock state;p.getStateInformation(state);AudioPluginAudioProcessor reopened;reopened.setStateInformation(state.getData(),state.getSize());auto recovered=reopened.getGP200PresetRecallSnapshot(slot);assert(recovered.routingMode==mode&&recovered.data.v==saved.data.v);m.routingModeSnapshot.mode=mode==0x85?0x95:0x85;AudioPluginAudioProcessorEditor recalled(reopened,m);recalled.selected=slot;recalled.startFullPresetRestoreFromSnapshot();assert(recalled.presetRestoreInProgress&&recalled.presetRestoreSteps.size()==2);assert(recalled.presetRestoreSteps.back().routingMode==mode);recalled.processFullPresetRestoreStep();auto n=m.output.messages.size();recalled.processFullPresetRestoreStep();assert(m.output.messages.size()==n&&recalled.presetRestoreInProgress);juce::Time::now=150;recalled.processFullPresetRestoreStep();assert(!recalled.presetRestoreInProgress&&m.routingStage==4&&!m.canSaveCurrentPreset());auto frame=m.output.messages.back().bytes;assert(routingModeResponse(frame.data(),frame.size())==mode);fresh(m,recovered.data,mode);count++;}
{AudioPluginAudioProcessor p;MidiConnection m;init(m,0x85);AudioPluginAudioProcessorEditor e(p,m);e.saveCurrentPresetToProject();m.routingModeSnapshot.mode=0x98;e.selected=1;e.saveCurrentPresetToProject();juce::MemoryBlock state;p.getStateInformation(state);AudioPluginAudioProcessor q;q.setStateInformation(state.getData(),state.getSize());assert(q.getGP200PresetRecallSnapshot(0).routingMode==0x85&&q.getGP200PresetRecallSnapshot(1).routingMode==0x98);count++;}
for(int value:{-1,2,127,0x8c,0x8f,0x9c,255,256}){AudioPluginAudioProcessor p;auto b=data();p.setGP200PresetSnapshotState(0,0,"preset",b,value);assert(p.getGP200PresetRecallSnapshot(0).routingMode==-1);count++;}
{AudioPluginAudioProcessor p;p.setGP200PresetSnapshotState(0,0,"old",data(),0x95);p.setGP200PresetSnapshotState(0,0,"new",data());assert(p.getGP200PresetRecallSnapshot(0).routingMode==-1);p.setGP200PresetSnapshotState(0,-1,"offline",data(),0x85);assert(p.getGP200PresetRecallSnapshot(0).routingMode==-1);count++;}
for(int format:{0,1}){juce::XmlElement xml("GP200StudioState");xml.setAttribute(format==0?"snapshotASlot":"presetSnapshotSlot",0);xml.setAttribute(format==0?"snapshotAName":"presetSnapshotName","old");xml.setAttribute(format==0?"snapshotADataBase64":"presetSnapshotDataBase64",data().toBase64Encoding());juce::MemoryBlock state;copyXmlToBinary(xml,state);AudioPluginAudioProcessor p;p.setGP200PresetSnapshotState(0,0,"stale",data(),0x95);p.setStateInformation(state.getData(),state.getSize());assert(p.getGP200PresetRecallSnapshot(0).data.v==data().v&&p.getGP200PresetRecallSnapshot(0).routingMode==-1);MidiConnection m;init(m,0x85);AudioPluginAudioProcessorEditor e(p,m);e.startFullPresetRestoreFromSnapshot();assert(e.presetRestoreSteps.size()==1);e.processFullPresetRestoreStep();assert(m.routingStage==0);for(auto& msg:m.output.messages)assert(routingModeResponse(msg.bytes.data(),msg.bytes.size())<0);count++;}
for(int bad:{0x8c,255}){juce::XmlElement xml("GP200StudioState");xml.setAttribute("snapshotASlot",0);xml.setAttribute("snapshotADataBase64",data().toBase64Encoding());xml.setAttribute("snapshotARoutingMode",bad);juce::MemoryBlock state;copyXmlToBinary(xml,state);AudioPluginAudioProcessor p;p.setStateInformation(state.getData(),state.getSize());assert(p.getGP200PresetRecallSnapshot(0).routingMode==-1);count++;}
{AudioPluginAudioProcessor p;p.setGP200PresetSnapshotState(0,0,"bad geometry",data(),0x8b);MidiConnection m;init(m);AudioPluginAudioProcessorEditor e(p,m);e.startFullPresetRestoreFromSnapshot();assert(!e.presetRestoreInProgress&&m.output.messages.empty());count++;}
{AudioPluginAudioProcessor p;MidiConnection m;init(m);AudioPluginAudioProcessorEditor e(p,m);e.saveCurrentPresetToProject();e.startFullPresetRestoreFromSnapshot();e.processFullPresetRestoreStep();m.currentSlot=1;++m.slotGeneration;juce::Time::now=150;auto n=m.output.messages.size();e.processFullPresetRestoreStep();assert(!e.presetRestoreInProgress&&m.output.messages.size()==n);count++;}
{AudioPluginAudioProcessor p;MidiConnection m;init(m);AudioPluginAudioProcessorEditor e(p,m);e.saveCurrentPresetToProject();m.currentPresetDataIsLive=false;e.selected=1;e.saveCurrentPresetToProject();assert(!p.hasSavedGP200PresetData(1));count++;}
{AudioPluginAudioProcessor p;p.setGP200PresetSnapshotState(0,0,"initial",data(),0x85);MidiConnection m;init(m);m.midiOutput=nullptr;AudioPluginAudioProcessorEditor e(p,m);e.saveCurrentPresetToProject();assert(p.getGP200PresetRecallSnapshot(0).routingMode==-1&&p.getGP200PresetRecallSnapshot(0).data.getSize()==100);count++;}
{AudioPluginAudioProcessor p;MidiConnection m;init(m);AudioPluginAudioProcessorEditor e(p,m);e.presetRestoreSnapshotData=data();e.presetRestoreSlot=0;e.presetRestoreRoutingMode=-1;e.presetRestoreRoutingMetadataFromDaw=false;e.finishFullPresetRestore();assert(e.effectsStatusText=="Recall Preset: snapshot restored into current slot. Press Store preset to save it here.");count++;}
std::cout<<count<<" Save/Recall pipeline + A/B project-state regressions passed (actual methods; simulated JUCE XML, codec and ports)\n";
}
