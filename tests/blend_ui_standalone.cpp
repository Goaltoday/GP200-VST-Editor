#include <algorithm>
#include <cassert>
#include <iostream>
namespace juce {constexpr int dontSendNotification=0;struct Time{static inline double now=0;static double getMillisecondCounterHiRes(){return now;}};}
struct Effect{float params[15]{};};struct Preset{bool isValid=true;Effect effects[11];};
struct State{bool connected=true;int slot=0;bool live=true,modeFresh=true;Preset data;};
namespace gp200 {bool routingModeIsChain(int m){return m>=128&&m<=139;}struct GP200PresetCodec{static Preset decodeLivePresetDump(Preset p){return p;}};bool hasIndependentBlend(Preset){return true;}bool legacyVolumeIsBlend(Preset){return false;}}
struct Midi{State state;bool pending=false,busy=false;int sends=0;float sent=0;State getRoutingStateSnapshot(){return state;}bool isBlendWritePending(){return pending;}bool isRoutingTransactionBusy(){return busy;}bool sendIndependentBlend(float v,bool,int){sends++;sent=v;pending=true;return true;}};
struct Slider{bool enabled=false,down=false,visible=false;double value=50;void setEnabled(bool b){enabled=b;}void setVisible(bool b){visible=b;}bool isMouseButtonDown(){return down;}void setValue(double v,int){value=v;}};
struct Label{void setVisible(bool){}void setTooltip(const char*){}};
struct AudioPluginAudioProcessorEditor{Midi midiConnection;Slider chainBlendSlider;Label chainBlendLabel;bool chainBlendQueued=false,chainBlendEditSessionReady=false,presetRestoreInProgress=false;int chainBlendSlot=-1,sprDeviceSlot=0,sprDeviceMode=128;float chainBlendQueuedValue=50;double chainBlendSendDueMs=0;void syncChainBlendControls();};
void AudioPluginAudioProcessorEditor::syncChainBlendControls ()
{
    const auto state = midiConnection.getRoutingStateSnapshot ();
    if (chainBlendSlot != state.slot || !state.connected)
    { chainBlendQueued = false; chainBlendEditSessionReady = false; chainBlendSlot = state.slot; }
    const bool chain = state.connected && state.slot == sprDeviceSlot
        && gp200::routingModeIsChain (sprDeviceMode);
    const auto preset = gp200::GP200PresetCodec::decodeLivePresetDump (state.data);
    const bool marked = gp200::hasIndependentBlend (preset);
    const bool pending = midiConnection.isBlendWritePending ();
    chainBlendLabel.setVisible (chain);
    chainBlendSlider.setVisible (chain);
    const bool ready = chain && state.live && state.modeFresh && preset.isValid
        && !presetRestoreInProgress && !midiConnection.isRoutingTransactionBusy ();
    // A same-slot refresh temporarily clears live/modeFresh. Retain the
    // confirmed editing session, but never send until fresh state returns.
    const bool interrupted = !chain || presetRestoreInProgress
        || midiConnection.isRoutingTransactionBusy ();
    if (interrupted)
    {
        chainBlendQueued = false;
        chainBlendEditSessionReady = false;
    }
    else if (ready)
        chainBlendEditSessionReady = true;
    chainBlendSlider.setEnabled (!interrupted && (ready || pending || chainBlendEditSessionReady));
    // Coalesce rapid drag changes; releasing the mouse sends the final gesture
    // immediately once the transport is ready.
    const bool gestureDue = !chainBlendSlider.isMouseButtonDown ()
        || juce::Time::getMillisecondCounterHiRes () >= chainBlendSendDueMs;
    if (ready && !pending && chainBlendQueued && gestureDue)
    {
        // The first user edit initializes independent BLEND; merely loading does not.
        if (midiConnection.sendIndependentBlend (chainBlendQueuedValue, !marked, chainBlendSlot))
            chainBlendQueued = false;
    }
    if (ready && !chainBlendQueued && !midiConnection.isBlendWritePending ()
        && !chainBlendSlider.isMouseButtonDown () && preset.isValid)
    {
        const float value = marked ? preset.effects[10].params[14]
            : gp200::legacyVolumeIsBlend (preset) ? preset.effects[10].params[0] : 50.0f;
        chainBlendSlider.setValue (std::clamp (value, 0.0f, 100.0f), juce::dontSendNotification);
    }
    chainBlendLabel.setTooltip ("A / B blend position. The first edit initializes independent BLEND. VOL retains volume. Requires FIX35 firmware.");
}
int main(){
 AudioPluginAudioProcessorEditor e;e.syncChainBlendControls();assert(e.chainBlendSlider.enabled);
 e.midiConnection.state.live=false;e.midiConnection.state.modeFresh=false;e.midiConnection.state.data.isValid=false;e.chainBlendQueued=true;e.chainBlendQueuedValue=81;e.chainBlendSlider.value=81;e.syncChainBlendControls();assert(e.chainBlendSlider.enabled&&e.chainBlendQueued&&e.midiConnection.sends==0&&e.chainBlendSlider.value==81);
 e.midiConnection.state.live=true;e.midiConnection.state.modeFresh=true;e.midiConnection.state.data.isValid=true;e.syncChainBlendControls();assert(e.midiConnection.sent==81&&e.midiConnection.pending);
 e.midiConnection.pending=false;e.midiConnection.state.live=false;e.syncChainBlendControls();assert(e.chainBlendSlider.enabled);
 e.midiConnection.busy=true;e.syncChainBlendControls();assert(!e.chainBlendSlider.enabled&&!e.chainBlendQueued&&!e.chainBlendEditSessionReady);
 AudioPluginAudioProcessorEditor first;first.midiConnection.state.live=false;first.syncChainBlendControls();assert(!first.chainBlendSlider.enabled);
 AudioPluginAudioProcessorEditor drag;drag.syncChainBlendControls();drag.chainBlendSlider.down=true;drag.chainBlendQueued=true;drag.chainBlendQueuedValue=64;drag.chainBlendSendDueMs=150;drag.syncChainBlendControls();assert(drag.midiConnection.sends==0);drag.chainBlendSlider.down=false;drag.syncChainBlendControls();assert(drag.midiConnection.sent==64);
 drag.midiConnection.state.slot=1;drag.syncChainBlendControls();assert(!drag.chainBlendSlider.enabled&&!drag.chainBlendQueued&&!drag.chainBlendEditSessionReady);
 std::cout<<"8 BLEND UI scenarios passed (production method; transport/JUCE mocked)\n";
}
