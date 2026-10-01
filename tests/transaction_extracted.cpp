
#include <string>
#include <array>
#include <vector>
#include <cassert>
#include <sstream>
#include <iostream>
namespace juce { using uint8=unsigned char; struct Time { static inline double now=0; static double getMillisecondCounterHiRes(){return now;} }; struct String:std::string {using std::string::string; String(std::string s):std::string(s){} static String toHexString(int x){std::ostringstream s;s<<std::hex<<x;return s.str();}}; }
namespace gp200 { using RoutingOrder=std::array<int,11>; struct GP200PresetCodec {struct Result {bool isValid;};static Result decodeLivePresetDump(bool x){return {x};}}; }
struct Midi {bool connected=true,valid=true,fail=false;std::vector<int> log; bool isConnected(){return connected;}bool getCurrentPresetDumpDataCopy(){return valid;}std::string getLastMessageText(){return "failed";}bool sendRoutingModeValue(int x){log.push_back(x);return !fail;}bool sendReorderEffects(gp200::RoutingOrder,int,int){log.push_back(1000);return !fail;}};
struct Ribbon {int p=5;gp200::RoutingOrder getLocalOrder(){return {};}int getSend(){return 2;}int getBoundary(){return p;}int getReturn(){return 8;}void keepRoutingDraft(){}};
struct AudioPluginAudioProcessorEditor {Midi midiConnection;Ribbon effectChainRibbon;int sprSendStage=0;double sprSendDeadlineMs=0;gp200::RoutingOrder sprPendingOrder{};int sprPendingS=0,sprPendingP=0,sprPendingR=0;bool sprPendingParallel=false,parallelRoutingSelected=true;std::string effectsStatusText;void repaint(){}void sendFlexibleRouteFromRibbon();void processFlexibleRouteTransaction();};
void AudioPluginAudioProcessorEditor::sendFlexibleRouteFromRibbon ()
{
    sprSendStage = 0; // A new edit replaces any pending route transaction.
    if (!midiConnection.isConnected ())
    {
        effectsStatusText = "SPR routing NOT SENT: GP-200 disconnected";
        repaint (); return;
    }
    const auto dump = midiConnection.getCurrentPresetDumpDataCopy ();
    const auto preset = gp200::GP200PresetCodec::decodeLivePresetDump (dump);
    if (!preset.isValid)
    {
        effectsStatusText = "SPR routing NOT SENT: wait for the device preset to load";
        repaint (); return;
    }
    sprPendingOrder = effectChainRibbon.getLocalOrder ();
    sprPendingS = effectChainRibbon.getSend ();
    sprPendingP = effectChainRibbon.getBoundary ();
    sprPendingR = effectChainRibbon.getReturn ();
    sprPendingParallel = parallelRoutingSelected;
    effectChainRibbon.keepRoutingDraft ();
    if (!midiConnection.sendRoutingModeValue (1))
    {
        effectsStatusText = "SPR routing NOT SENT: " + midiConnection.getLastMessageText ();
        repaint (); return;
    }
    sprSendDeadlineMs = juce::Time::getMillisecondCounterHiRes () + 150.0;
    sprSendStage = 1;
    effectsStatusText = "SPR sending: Series first, then order and P";
    repaint ();
}

void AudioPluginAudioProcessorEditor::processFlexibleRouteTransaction ()
{
    if (sprSendStage == 0) return;
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
            sprSendStage = 0;
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
        const auto mode = static_cast<juce::uint8> ((sprPendingParallel ? 0x80 : 0x90) | sprPendingP);
        if (midiConnection.sendRoutingModeValue (mode))
            effectsStatusText = "SPR mode sent: 0x" + juce::String::toHexString (static_cast<int> (mode))
                + " (audio confirmation pending)";
        else
            effectsStatusText = "SPR mode NOT SENT: " + midiConnection.getLastMessageText ();
    }
    repaint ();
}


int main(){using E=AudioPluginAudioProcessorEditor;E e;e.sendFlexibleRouteFromRibbon();assert(e.midiConnection.log==std::vector<int>{1});juce::Time::now=149;e.processFlexibleRouteTransaction();assert(e.midiConnection.log.size()==1);juce::Time::now=150;e.processFlexibleRouteTransaction();assert(e.midiConnection.log==std::vector<int>({1,1000}));juce::Time::now=299;e.processFlexibleRouteTransaction();assert(e.midiConnection.log.size()==2);juce::Time::now=300;e.processFlexibleRouteTransaction();assert(e.midiConnection.log==std::vector<int>({1,1000,0x85}));
E n;n.sendFlexibleRouteFromRibbon();n.effectChainRibbon.p=7;n.sendFlexibleRouteFromRibbon();juce::Time::now=450;n.processFlexibleRouteTransaction();juce::Time::now=600;n.processFlexibleRouteTransaction();assert(n.midiConnection.log==std::vector<int>({1,1,1000,0x87}));
E d;d.midiConnection.connected=false;d.sendFlexibleRouteFromRibbon();assert(d.midiConnection.log.empty());E v;v.midiConnection.valid=false;v.sendFlexibleRouteFromRibbon();assert(v.midiConnection.log.empty());E c;c.sendFlexibleRouteFromRibbon();c.midiConnection.connected=false;c.processFlexibleRouteTransaction();assert(c.sprSendStage==0&&c.midiConnection.log.size()==1);E f;f.sendFlexibleRouteFromRibbon();f.midiConnection.fail=true;juce::Time::now=750;f.processFlexibleRouteTransaction();assert(f.sprSendStage==0&&f.midiConnection.log.size()==2);
E z;z.parallelRoutingSelected=false;z.sendFlexibleRouteFromRibbon();juce::Time::now=900;z.processFlexibleRouteTransaction();juce::Time::now=1050;z.processFlexibleRouteTransaction();assert(z.midiConnection.log.back()==0x95);std::cout<<"Transaction source tests passed: timing, order, replacement, guards, cancellation, failure, Series\n";
}
