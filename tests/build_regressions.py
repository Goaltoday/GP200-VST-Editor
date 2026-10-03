from pathlib import Path
r=Path(__file__).resolve().parent;src=r.parent/'source/libgp200/MidiConnection.cpp';s=src.read_text()
# Compile exact production method bodies; JUCE ports and preset codec are test doubles.
names=[('processBlendReadback','void'),('isBlendWritePending','bool'),('sendIndependentBlend','bool'),('getRoutingRequestSnapshot','MidiConnection::RoutingRequestSnapshot'),('getRoutingStateSnapshot','MidiConnection::RoutingStateSnapshot'),('timerCallback','void'),('sendFlexibleRouting','bool'),('canSaveCurrentPreset','bool'),('isRoutingTransactionBusy','bool'),('getRoutingTransactionStatus','juce::String'),('failRoutingTransaction','void'),('processRoutingTransaction','void'),('processPresetReadRecovery','void'),('sendStateDumpRequestUnlocked','bool'),('requestPresetNameForCurrentSlotIfNeeded','bool'),('sendLiveReadRequestForSlot','bool'),('resetPresetDumpCaptureForSlot','void'),('collectPresetReadChunk','void'),('getChunkOffset','int'),('assemblePresetReadChunks','juce::MemoryBlock'),('nibbleDecode','std::vector<juce::uint8>'),('buildLiveReadRequest','std::vector<juce::uint8>'),('buildStateDumpRequest','std::vector<juce::uint8>'),('sendRoutingModeValue','bool'),('requestRoutingModeFromGP200','bool'),('sendReorderEffects','bool'),('buildReorderEffects','std::vector<juce::uint8>'),('nibbleEncode','std::vector<juce::uint8>'),('scheduleLivePresetRefresh','void'),('processPendingLivePresetRefresh','void'),('beginPresetRestoreTransaction','void'),('endPresetRestoreTransaction','void'),('sendPresetRestoreRoutingMode','bool'),('storeCurrentPresetToGP200','bool')]
methods=[];decl=[]
for name,ret in names:
 a=s.index('MidiConnection::'+name+' (');b=s.index('\n}',a)+2;body=s[a:b];methods.append(ret+' '+body);signature=body[:body.index('\n{')].replace('MidiConnection::','')
 if name == 'endPresetRestoreTransaction': signature=signature.replace('int expectedRoutingMode)', 'int expectedRoutingMode = -1)')
 if name in ['sendFlexibleRouting']: signature=signature.replace('int expectedSlot)','int expectedSlot = -1)')
 decl.append(('static ' if name in ['getChunkOffset','assemblePresetReadChunks','nibbleDecode','buildLiveReadRequest','buildStateDumpRequest','buildReorderEffects','nibbleEncode'] else '')+ret.replace('MidiConnection::','')+' '+signature+';')
h=r'''#include "../source/libgp200/GP200FlexibleRouting.h"
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
double currentStateRequestSentMs=0,startupHandshakePhaseStartedMs=0,liveRefreshDueMs=0;
std::vector<std::vector<juce::uint8>> presetReadChunks,stateDumpChunks;juce::MemoryBlock currentPresetDecodedData;
struct RoutingRequestSnapshot {bool active=false;int slot=-1;RoutingOrder order{};int send=0,boundary=0,ret=0,mode=1;};
struct RoutingModeSnapshot{int mode=0x85,slot=0;std::uint64_t revision=1;};RoutingModeSnapshot routingModeSnapshot;
struct RoutingStateSnapshot {bool connected=false;int slot=-1;bool live=false;RoutingModeSnapshot mode;juce::MemoryBlock data;std::uint64_t presetRevision=0,liveRevision=0;bool canSave=false,modeFresh=false;};
bool isIRUploadInProgress(){return irUploadPhase!=IRUploadPhase::Idle;}bool isSoundCloneUploadInProgress(){return soundCloneUploadPhase!=SoundCloneUploadPhase::Idle;}juce::String getLastMessageText(){return lastMessageText;}
std::uint64_t presetRevision=1,livePresetRevision=1;
juce::String currentPresetName="test",currentPresetDumpStatusText,lastMessageText;
int nativeRoutingMode=1;bool blendWritePending=false,blendWriteMarked=false;int blendWriteSlot=-1;std::uint64_t blendWriteBaseline=0;float blendWriteExpected=50.0f;double blendWriteDeadline=0;
std::vector<std::pair<int,float>> parameterWrites;
bool sendParamChange(int block,int param,juce::uint32,float value){parameterWrites.push_back({param,value});std::memcpy(currentPresetDecodedData.v.data()+effectBlockStart+block*effectBlockSize+paramsOffset+param*4,&value,4);return true;}
int routingStage=0,routingSlot=-1,routingSend=0,routingBoundary=0,routingReturn=0,routingValue=1;RoutingOrder routingOrder{};
std::uint64_t routingGeneration=0,slotGeneration=0,routingModeBaseline=0,routingLiveBaseline=0,presetModeBaseline=0,presetRestoreSlotGeneration=0;
double routingNextMs=0,routingDeadlineMs=0,routingQueryMs=0,presetReadStartedMs=0,presetReadResumeMs=0,modePollMs=0;int presetReadRetries=0;bool presetReadIsLive=false;
juce::String routingTransactionStatus="SPR: idle";
int irTicks=0,cloneTicks=0,scanTicks=0;bool stopped=false;
bool isConnected()const{return midiOutput!=nullptr;}void stopTimer(){stopped=true;}void processIRUpload(){irTicks++;}void processSoundCloneUpload(){cloneTicks++;}void finishModSyncFailure(juce::String){modSyncActive=false;}bool requestCurrentPresetFromGP200(){return sendStateDumpRequestUnlocked();}void processStartupHandshake(){}void processPresetNameScan(){scanTicks++;}
static juce::String sanitizePresetNameForStore(juce::String x){return x;}static std::vector<juce::uint8> buildStorePresetCommit(int slot,juce::String){return {0xf0,(juce::uint8)slot,0xf7};}
DECL
};
METHODS
} // namespace gp200
'''
ui_source=(r.parent/'source/GP200Plugin/PluginEditor.cpp').read_text()
ui_methods=[]
for name,ret in [('sendFlexibleRouteFromRibbon','void '),('syncFlexibleRoutingFromDevice','void '),('toggleSeriesParallel','void '),('~AudioPluginAudioProcessorEditor','')]:
 a=ui_source.index('AudioPluginAudioProcessorEditor::'+name+' ()');b=ui_source.index('\n}',a)+2;ui_methods.append(ret+ui_source[a:b])
ui_header=r"""
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
"""
ui=ui_header+'\n'.join(ui_methods)
t=r'''
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
{MidiConnection m;init(m);for(int value=0;value<256;value++){m.routingModeSnapshot.mode=value;assert(m.canSaveCurrentPreset()==validRoutingModeValue(value)&&(!isExtendedRoutingMode(value)||(value&15)>=2&&(value&15)<=8));cases++;}}
{MidiConnection m;init(m);AudioPluginAudioProcessorEditor editor(m);editor.syncFlexibleRoutingFromDevice();assert(editor.parallelRoutingSelected);m.irUploadPhase=MidiConnection::IRUploadPhase::Busy;editor.toggleSeriesParallel();assert(editor.parallelRoutingSelected&&editor.effectChainRibbon.parallel&&m.output.messages.empty());m.irUploadPhase=MidiConnection::IRUploadPhase::Idle;editor.syncFlexibleRoutingFromDevice();assert(editor.parallelRoutingSelected);cases++;}
{MidiConnection m;init(m);AudioPluginAudioProcessorEditor editor(m);editor.syncFlexibleRoutingFromDevice();m.currentPresetDataIsLive=false;editor.toggleSeriesParallel();assert(editor.parallelRoutingSelected&&m.output.messages.empty());cases++;}
{MidiConnection m;init(m);stageToReply(m);AudioPluginAudioProcessorEditor editor(m);editor.effectChainRibbon.order[0]=1;editor.parallelRoutingSelected=false;editor.sendFlexibleRouteFromRibbon();assert(m.routingStage==3&&editor.effectChainRibbon.order==order&&editor.parallelRoutingSelected);cases++;}
{MidiConnection m;init(m);{AudioPluginAudioProcessorEditor editor(m);m.beginPresetRestoreTransaction();editor.presetRestoreInProgress=true;}assert(!m.presetRestoreTransactionActive&&m.currentStateRequestQueued&&!m.canSaveCurrentPreset());cases++;}
{MidiConnection m;init(m);m.sendLiveReadRequestForSlot(0);receive(m);assert(!m.canSaveCurrentPreset());mode(m,0x85);assert(m.canSaveCurrentPreset());cases++;}
{MidiConnection m;init(m);assert(m.sendFlexibleRouting(order,2,5,8,true,0));++m.slotGeneration;juce::Time::now=150;auto n=m.output.messages.size();m.processRoutingTransaction();assert(m.routingStage==6&&m.output.messages.size()==n);cases++;}
{MidiConnection m;init(m);m.sendLiveReadRequestForSlot(0);for(int attempt=0;attempt<4;attempt++){juce::Time::now=m.presetReadStartedMs+1500;m.processPresetReadRecovery();double expected=attempt==3?5000:300;assert(m.presetReadResumeMs==juce::Time::now+expected);juce::Time::now=m.presetReadResumeMs;assert(m.requestPresetNameForCurrentSlotIfNeeded());}cases++;}
for(int wanted:{0,1,0x82,0x85,0x88,0x92,0x95,0x98}){MidiConnection m;init(m);m.beginPresetRestoreTransaction();assert(m.sendPresetRestoreRoutingMode(0,wanted));m.endPresetRestoreTransaction(wanted);assert(m.routingStage==4&&!m.canSaveCurrentPreset());m.sendLiveReadRequestForSlot(0);receive(m);m.processRoutingTransaction();assert(m.routingStage==5);mode(m,wanted);m.processBlendReadback();m.processRoutingTransaction();assert(m.routingStage==0&&m.canSaveCurrentPreset());cases++;}
{MidiConnection m;init(m);m.beginPresetRestoreTransaction();m.currentSlot=1;++m.slotGeneration;auto n=m.output.messages.size();assert(!m.sendPresetRestoreRoutingMode(0,0x85)&&m.output.messages.size()==n);cases++;}
{MidiConnection m;init(m);m.beginPresetRestoreTransaction();++m.slotGeneration;assert(!m.sendPresetRestoreRoutingMode(0,0x85));cases++;}
{MidiConnection m;init(m);m.beginPresetRestoreTransaction();m.endPresetRestoreTransaction(0x95);m.sendLiveReadRequestForSlot(0);receive(m);m.processRoutingTransaction();mode(m,0x85);m.processRoutingTransaction();assert(m.routingStage==5&&!m.canSaveCurrentPreset());juce::Time::now=7000;m.processRoutingTransaction();assert(m.routingStage==6&&!m.canSaveCurrentPreset());cases++;}
std::cout<<cases<<" FIX8 connection / read / save regressions passed (real method bodies, simulated ports and codec)\n";
}
'''
# Parenthesize the eligibility formula instead of comparing two chained bool expressions.
t=t.replace('m.canSaveCurrentPreset()==validRoutingModeValue(value)&&(!isExtendedRoutingMode(value)||(value&15)>=2&&(value&15)<=8)', 'm.canSaveCurrentPreset()==(validRoutingModeValue(value)&&(!isExtendedRoutingMode(value)||((value&15)>=2&&(value&15)<=8)))')
(r/'connection_regressions.cpp').write_text(h.replace('DECL','\n'.join(decl)).replace('METHODS','\n'.join(methods))+ui+t)
print('Production methods extracted')
