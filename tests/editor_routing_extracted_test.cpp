
#include "../source/libgp200/GP200FlexibleRouting.h"
#include <string>
#include <array>
#include <vector>
#include <cassert>
#include <sstream>
#include <iostream>
#include <algorithm>
namespace juce { using uint8=unsigned char; struct Time { static inline double now=0; static double getMillisecondCounterHiRes(){return now;} }; template<class T>T jlimit(T a,T b,T x){return std::clamp(x,a,b);}struct String:std::string {using std::string::string; String(std::string s):std::string(s){} static String toHexString(int x){std::ostringstream s;s<<std::hex<<x;return s.str();}}; }
namespace gp200 {using RoutingOrder=std::array<int,11>;struct GP200Preset {bool isValid=true;RoutingOrder routingOrder{0,1,2,3,4,5,6,7,8,9,10};int fxLoopSend=2,fxLoopReturn=8;};struct GP200PresetCodec{static GP200Preset decodeLivePresetDump(GP200Preset x){return x;}};}
struct Midi {bool connected=true,live=true,fail=false,ir=false;int slot=0,queries=0,reads=0;std::uint64_t revision=1,liveRevision=1;struct Snapshot {int mode=-1,slot=0;std::uint64_t revision=1;};Snapshot snap;gp200::GP200Preset preset;std::vector<int> log;
bool isConnected(){return connected;}int getCurrentSlot(){return slot;}bool hasLiveCurrentPresetData(){return live;}gp200::GP200Preset getCurrentPresetDumpDataCopy(){return preset;}std::string getLastMessageText(){return "failure";}bool sendRoutingModeValue(int x){log.push_back(x);return !fail;}bool sendReorderEffects(gp200::RoutingOrder,int,int){log.push_back(1000);return !fail;}struct State{bool connected;int slot;bool live;Snapshot mode;gp200::GP200Preset data;std::uint64_t presetRevision,liveRevision;};State getRoutingStateSnapshot(){return {connected,slot,live,snap,preset,revision,liveRevision};}Snapshot getRoutingModeSnapshot(){return snap;}std::uint64_t getPresetRevision(){return revision;}std::uint64_t getLivePresetRevision(){return liveRevision;}bool isIRUploadInProgress(){return ir;}bool isSoundCloneUploadInProgress(){return false;}bool isPresetNameScanRunning(){return false;}bool requestRoutingModeFromGP200(){queries++;return true;}bool requestCurrentPresetFromGP200(){reads++;return true;}};
struct Ribbon {int p=5,s=2,r=8;bool draft=false,parallel=false;gp200::RoutingOrder order{0,1,2,3,4,5,6,7,8,9,10};gp200::RoutingOrder getLocalOrder(){return order;}int getSend(){return s;}int getBoundary(){return p;}int getReturn(){return r;}void keepRoutingDraft(){draft=true;}void releaseRoutingDraft(){draft=false;}void setDeviceRouting(int a,int b,int c,bool v){s=a;p=b;r=c;parallel=v;}};
struct AudioPluginAudioProcessorEditor {Midi midiConnection;Ribbon effectChainRibbon;bool presetRestoreInProgress=false;std::uint64_t sprModeAfterSend=0;int sprSendStage=0;double sprSendDeadlineMs=0;gp200::RoutingOrder sprPendingOrder{};int sprPendingS=0,sprPendingP=0,sprPendingR=0;bool sprPendingParallel=false,parallelRoutingSelected=true;std::string effectsStatusText;int sprDeviceSlot=-2,sprDeviceMode=-1;bool sprWasConnected=false,sprAwaitingMode=false,sprAwaitingPreset=false;double sprModeQueryMs=0,sprConfirmationDeadlineMs=0;std::uint64_t sprPresetAfterMode=0,sprAppliedPresetRevision=0,sprAppliedModeRevision=0;void repaint(){}void updateSeriesParallelButtonText(){}void scheduleEditorHeightUpdate(){}void updateEffectChainRibbon(gp200::GP200Preset x){effectChainRibbon.order=x.routingOrder;}void sendFlexibleRouteFromRibbon();void processFlexibleRouteTransaction();void syncFlexibleRoutingFromDevice();};
void AudioPluginAudioProcessorEditor::sendFlexibleRouteFromRibbon ()
{
    if (!midiConnection.isConnected ())
    {
        effectsStatusText = "SPR routing NOT SENT: GP-200 disconnected";
        repaint (); return;
    }
    if (presetRestoreInProgress || midiConnection.isIRUploadInProgress () || midiConnection.isSoundCloneUploadInProgress ())
    {
        effectsStatusText = "SPR routing NOT SENT: preset restore or upload in progress";
        repaint (); return;
    }
    const auto state = midiConnection.getRoutingStateSnapshot ();
    const auto preset = gp200::GP200PresetCodec::decodeLivePresetDump (state.data);
    if (!state.connected || state.slot != midiConnection.getCurrentSlot () || !state.live || !preset.isValid)
    {
        effectsStatusText = "SPR routing NOT SENT: wait for the device preset to load";
        repaint (); return;
    }
    const auto order = effectChainRibbon.getLocalOrder ();
    const int send = effectChainRibbon.getSend ();
    const int boundary = effectChainRibbon.getBoundary ();
    const int ret = effectChainRibbon.getReturn ();
    const bool parallel = parallelRoutingSelected;
    if (!gp200::validFlexibleRouting (order, send, boundary, ret))
    {
        effectsStatusText = "SPR routing NOT SENT: invalid order or S/P/R";
        repaint (); return;
    }
    // Keep the previous transaction intact until the replacement can actually start.
    if (!midiConnection.sendRoutingModeValue (1))
    {
        effectsStatusText = "SPR routing NOT SENT: " + midiConnection.getLastMessageText ();
        repaint (); return;
    }
    sprSendStage = 0;
    sprAwaitingMode = false;
    sprAwaitingPreset = false;
    sprPendingOrder = order;
    sprPendingS = send;
    sprPendingP = boundary;
    sprPendingR = ret;
    sprPendingParallel = parallel;
    effectChainRibbon.keepRoutingDraft ();
    sprSendDeadlineMs = juce::Time::getMillisecondCounterHiRes () + 150.0;
    sprSendStage = 1;
    sprDeviceSlot = state.slot;
    sprWasConnected = true;
    sprAwaitingMode = true;
    sprConfirmationDeadlineMs = juce::Time::getMillisecondCounterHiRes () + 3000.0;
    effectsStatusText = "SPR sending: Series first, then order and P";
    repaint ();
}

void AudioPluginAudioProcessorEditor::processFlexibleRouteTransaction ()
{
    if (sprSendStage == 0) return;
    if (presetRestoreInProgress || midiConnection.isIRUploadInProgress () || midiConnection.isSoundCloneUploadInProgress ())
    {
        sprSendStage = 0; sprAwaitingMode = false; sprAwaitingPreset = false;
        effectsStatusText = "SPR transaction cancelled: preset restore or upload started";
        sprModeQueryMs = juce::Time::getMillisecondCounterHiRes () + 250.0;
        effectChainRibbon.releaseRoutingDraft ();
        repaint (); return;
    }
    if (!midiConnection.isConnected ())
    {
        sprSendStage = 0;
        effectsStatusText = "SPR transaction cancelled: disconnected";
        repaint (); return;
    }
    if (juce::Time::getMillisecondCounterHiRes () < sprSendDeadlineMs) return;
    if (sprSendStage == 1)
    {
        if (!midiConnection.sendReorderEffects (sprPendingOrder, sprPendingS, sprPendingR))
        {
            sprSendStage = 0; sprAwaitingMode = false; sprAwaitingPreset = false;
            effectsStatusText = "SPR order NOT SENT: " + midiConnection.getLastMessageText ();
        }
        else
        {
            sprSendStage = 2;
            sprSendDeadlineMs = juce::Time::getMillisecondCounterHiRes () + 150.0;
            effectsStatusText = "SPR sending: order sent, waiting before P";
        }
    }
    else
    {
        sprSendStage = 0;
        sprModeQueryMs = juce::Time::getMillisecondCounterHiRes () + 150.0;
        const auto mode = static_cast<juce::uint8> ((sprPendingParallel ? 0x80 : 0x90) | sprPendingP);
        sprModeAfterSend = midiConnection.getRoutingModeSnapshot ().revision;
        if (midiConnection.sendRoutingModeValue (mode))
            effectsStatusText = "SPR mode sent: 0x" + juce::String::toHexString (static_cast<int> (mode))
                + " (waiting for device readback)";
        else
        {
            sprAwaitingMode = false; sprAwaitingPreset = false;
            effectsStatusText = "SPR mode NOT SENT: " + midiConnection.getLastMessageText ();
        }
    }
    repaint ();
}

void AudioPluginAudioProcessorEditor::syncFlexibleRoutingFromDevice ()
{
    const auto now = juce::Time::getMillisecondCounterHiRes ();
    const auto state = midiConnection.getRoutingStateSnapshot ();
    const bool connected = state.connected;
    const int slot = state.slot;
    if (!connected || !sprWasConnected || slot != sprDeviceSlot)
    {
        sprWasConnected = connected; sprDeviceSlot = slot;
        sprSendStage = 0; sprAwaitingMode = false; sprAwaitingPreset = false;
        sprDeviceMode = -1; sprAppliedModeRevision = 0; sprAppliedPresetRevision = 0;
        effectChainRibbon.releaseRoutingDraft ();
        sprModeQueryMs = now + 250.0;
        if (!connected) return;
    }
    if ((sprAwaitingMode || sprAwaitingPreset) && now >= sprConfirmationDeadlineMs)
    {
        sprAwaitingMode = false; sprAwaitingPreset = false;
        effectsStatusText = "SPR: confirmation timed out; device routing unconfirmed";
        repaint ();
    }
    if (sprSendStage != 0 || !state.live) return;
    const bool busy = presetRestoreInProgress || midiConnection.isIRUploadInProgress () || midiConnection.isSoundCloneUploadInProgress () || midiConnection.isPresetNameScanRunning ();
    if (busy) return;
    if (!busy && now >= sprModeQueryMs)
    {
        midiConnection.requestRoutingModeFromGP200 ();
        sprModeQueryMs = now + 1200.0;
    }
    const auto mode = state.mode;
    if ((sprAwaitingMode || sprAwaitingPreset) && now >= sprConfirmationDeadlineMs
        && (mode.slot != slot || !gp200::validRoutingModeValue (mode.mode)))
    {
        sprAwaitingMode = false; sprAwaitingPreset = false;
        effectsStatusText = "SPR: no routing readback received; device state unconfirmed";
        repaint ();
    }
    if (mode.slot != slot || !gp200::validRoutingModeValue (mode.mode)) return;
    if (sprAwaitingMode)
    {
        if (mode.revision <= sprModeAfterSend) return;
        const int expected = (sprPendingParallel ? 0x80 : 0x90) | sprPendingP;
        if (mode.mode != expected)
        {
            if (now < sprConfirmationDeadlineMs) return;
            effectsStatusText = "SPR: device mode differs from requested mode";
            sprAwaitingMode = false;
        }
        else
        {
            sprAwaitingMode = false;
            sprAwaitingPreset = true;
            sprPresetAfterMode = state.liveRevision;
            midiConnection.requestCurrentPresetFromGP200 ();
            return;
        }
    }
    const auto revision = state.presetRevision;
    if (sprAwaitingPreset && state.liveRevision <= sprPresetAfterMode)
    {
        if (now >= sprConfirmationDeadlineMs)
        {
            sprAwaitingPreset = false;
            effectsStatusText = "SPR: preset readback timed out; order unconfirmed";
            repaint ();
        }
        return;
    }
    const auto preset = gp200::GP200PresetCodec::decodeLivePresetDump (state.data);
    if (!preset.isValid) return;
    if (preset.fxLoopSend < 0 || preset.fxLoopReturn < preset.fxLoopSend || preset.fxLoopReturn > 11)
    {
        effectsStatusText = "SPR: invalid Send/Return received from device";
        return;
    }
    const int boundary = gp200::isExtendedRoutingMode (mode.mode) ? mode.mode & 15
        : juce::jlimit (preset.fxLoopSend, preset.fxLoopReturn, effectChainRibbon.getBoundary ());
    if (!gp200::validFlexibleRouting (preset.routingOrder, preset.fxLoopSend, boundary, preset.fxLoopReturn)) return;
    if (sprAwaitingPreset && (preset.routingOrder != sprPendingOrder || preset.fxLoopSend != sprPendingS || preset.fxLoopReturn != sprPendingR
        || mode.mode != ((sprPendingParallel ? 0x80 : 0x90) | sprPendingP)))
    {
        if (now < sprConfirmationDeadlineMs) return;
        sprAwaitingPreset = false;
        effectsStatusText = "SPR: device routing differs from requested routing";
    }
    const bool newlyConfirmed = sprAwaitingPreset;
    sprAwaitingPreset = false;
    if (!newlyConfirmed && revision == sprAppliedPresetRevision && mode.mode == sprDeviceMode) return;
    sprAppliedPresetRevision = revision; sprAppliedModeRevision = mode.revision;
    sprDeviceMode = mode.mode;
    effectChainRibbon.releaseRoutingDraft ();
    updateEffectChainRibbon (preset);
    parallelRoutingSelected = gp200::routingModeIsParallel (mode.mode);
    effectChainRibbon.setDeviceRouting (preset.fxLoopSend, boundary, preset.fxLoopReturn, parallelRoutingSelected);
    updateSeriesParallelButtonText ();
    scheduleEditorHeightUpdate ();
    if (newlyConfirmed) effectsStatusText = "SPR routing confirmed by device (audio test pending)";
    repaint ();
}


std::vector<std::uint8_t> nativeReply(int mode){std::vector<std::uint8_t>b{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,8,0,0,0};for(int x:{6,0,4,0,5,0,mode,0}){b.push_back(x>>4);b.push_back(x&15);}b.push_back(0xf7);return b;}
int receivedMode(int mode){auto bytes=nativeReply(mode);return gp200::routingModeResponse(bytes.data(),bytes.size());}
int main(){using E=AudioPluginAudioProcessorEditor;E e;e.midiConnection.snap.mode=receivedMode(0x93);e.syncFlexibleRoutingFromDevice();assert(!e.parallelRoutingSelected&&e.effectChainRibbon.p==3);juce::Time::now=300;e.syncFlexibleRoutingFromDevice();assert(e.midiConnection.queries==1);
// A change on the GP must replace the VST's local mode and P.
e.midiConnection.snap.mode=receivedMode(0x85);e.midiConnection.snap.revision++;e.syncFlexibleRoutingFromDevice();assert(e.parallelRoutingSelected&&e.effectChainRibbon.p==5);
// Changing slots cancels an in-flight transaction and ignores old-slot replies.
e.sprSendStage=2;e.midiConnection.slot=1;e.syncFlexibleRoutingFromDevice();assert(e.sprSendStage==0&&e.sprDeviceMode==-1);e.midiConnection.snap={receivedMode(0x92),1,4};e.midiConnection.revision++;e.syncFlexibleRoutingFromDevice();assert(!e.parallelRoutingSelected&&e.effectChainRibbon.p==2);
// Edits remain provisional through intermediate legacy notifications, then require a fresh preset.
e.parallelRoutingSelected=true;e.effectChainRibbon.p=7;e.sendFlexibleRouteFromRibbon();assert(e.sprSendStage==1);juce::Time::now=450;e.processFlexibleRouteTransaction();juce::Time::now=600;e.processFlexibleRouteTransaction();assert(e.midiConnection.log==std::vector<int>({1,1000,0x87}));e.midiConnection.snap.mode=0x92;e.syncFlexibleRoutingFromDevice();assert(e.effectChainRibbon.p==7&&e.sprAwaitingMode);e.midiConnection.snap.mode=receivedMode(0x87);e.midiConnection.snap.revision++;e.syncFlexibleRoutingFromDevice();assert(e.sprAwaitingPreset&&e.midiConnection.reads==1);e.syncFlexibleRoutingFromDevice();assert(e.sprAwaitingPreset);e.midiConnection.revision++;e.midiConnection.liveRevision++;e.syncFlexibleRoutingFromDevice();assert(!e.sprAwaitingPreset&&e.sprDeviceMode==0x87&&e.effectChainRibbon.p==7&&!e.effectChainRibbon.draft);
// An output error and a missing response do not leave a pending Store lock indefinitely.
E f;f.midiConnection.snap.mode=0x83;f.syncFlexibleRoutingFromDevice();f.sendFlexibleRouteFromRibbon();f.midiConnection.fail=true;juce::Time::now=750;f.processFlexibleRouteTransaction();assert(f.sprSendStage==0&&!f.sprAwaitingMode);
E n;n.syncFlexibleRoutingFromDevice();n.sendFlexibleRouteFromRibbon();juce::Time::now=900;n.processFlexibleRouteTransaction();juce::Time::now=1050;n.processFlexibleRouteTransaction();juce::Time::now=4000;n.syncFlexibleRoutingFromDevice();assert(!n.sprAwaitingMode);
E cached;cached.midiConnection.snap.mode=0x85;cached.syncFlexibleRoutingFromDevice();cached.effectChainRibbon.p=5;cached.sendFlexibleRouteFromRibbon();juce::Time::now=4150;cached.processFlexibleRouteTransaction();juce::Time::now=4300;cached.processFlexibleRouteTransaction();cached.syncFlexibleRoutingFromDevice();assert(cached.sprAwaitingMode&&!cached.sprAwaitingPreset);cached.midiConnection.snap.revision++;cached.syncFlexibleRoutingFromDevice();assert(cached.sprAwaitingPreset);cached.midiConnection.revision++;cached.syncFlexibleRoutingFromDevice();assert(cached.sprAwaitingPreset);cached.midiConnection.liveRevision++;cached.syncFlexibleRoutingFromDevice();assert(!cached.sprAwaitingPreset);
E busy;busy.presetRestoreInProgress=true;busy.sendFlexibleRouteFromRibbon();assert(busy.midiConnection.log.empty());busy.presetRestoreInProgress=false;busy.midiConnection.ir=true;busy.sendFlexibleRouteFromRibbon();assert(busy.midiConnection.log.empty());busy.midiConnection.ir=false;busy.sendFlexibleRouteFromRibbon();assert(busy.sprSendStage==1);busy.midiConnection.ir=true;busy.processFlexibleRouteTransaction();assert(busy.sprSendStage==0&&!busy.sprAwaitingMode&&busy.midiConnection.log.size()==1);
E stale;stale.midiConnection.live=false;stale.sendFlexibleRouteFromRibbon();assert(stale.midiConnection.log.empty());
E invalid;invalid.effectChainRibbon.p=12;invalid.sendFlexibleRouteFromRibbon();assert(invalid.midiConnection.log.empty());
// Rejected replacement during a read must not strand an already sent Series step.
E replace;replace.midiConnection.snap.mode=0x85;replace.syncFlexibleRoutingFromDevice();replace.effectChainRibbon.p=5;replace.sendFlexibleRouteFromRibbon();assert(replace.sprSendStage==1);auto deadline=replace.sprSendDeadlineMs;
replace.midiConnection.live=false;replace.effectChainRibbon.p=6;replace.sendFlexibleRouteFromRibbon();assert(replace.sprSendStage==1&&replace.sprPendingP==5&&replace.sprAwaitingMode&&replace.sprSendDeadlineMs==deadline&&replace.midiConnection.log.size()==1);
replace.midiConnection.live=true;replace.effectChainRibbon.p=12;replace.sendFlexibleRouteFromRibbon();assert(replace.sprSendStage==1&&replace.sprPendingP==5&&replace.midiConnection.log.size()==1);
replace.effectChainRibbon.p=6;replace.midiConnection.fail=true;replace.sendFlexibleRouteFromRibbon();assert(replace.sprSendStage==1&&replace.sprPendingP==5);replace.midiConnection.fail=false;
juce::Time::now=deadline;replace.processFlexibleRouteTransaction();juce::Time::now=deadline+150;replace.processFlexibleRouteTransaction();assert(replace.midiConnection.log.back()==0x85);
// Successful replacement uses the new complete draft.
replace.effectChainRibbon.p=6;replace.sendFlexibleRouteFromRibbon();assert(replace.sprSendStage==1&&replace.sprPendingP==6);
std::cout<<"Actual editor method tests passed: invalid draft rejected before Series; freshness, optimistic-cache rejection, busy cancellation, cached-only refusal; device Series/Parallel/P, slot change, stale reply, staged sends, fresh preset confirmation, errors and timeout\n";}
