from pathlib import Path
r=Path(__file__).resolve().parent
base=(r/'connection_regressions.cpp').read_text().split('struct Ribbon {')[0]
base=base.replace('struct GP200PresetCodec {','struct GP200PresetCodec {static juce::String blockNameForSlotIndex(int){return "BLOCK";}')
base+='''namespace gp200 {struct Param {int idx;};struct ParamSet {int count;const Param* params;};struct GP200EffectParamDatabase {static const ParamSet* findParamsForEffect(juce::uint32,juce::String){static const Param p[]{ {0} };static const ParamSet s{1,p};return &s;}};}
namespace juce { inline constexpr int dontSendNotification=0; }
struct BlendSlider { double value=50; bool enabled=false,visible=false,down=false; void setVisible(bool x){visible=x;} void setEnabled(bool x){enabled=x;} bool isMouseButtonDown()const{return down;} void setValue(double x,int){value=x;} };
struct BlendLabel { bool visible=false;void setVisible(bool x){visible=x;}void setTooltip(const char*){} };
struct AudioPluginAudioProcessorEditor {
 gp200::MidiConnection midiConnection;
 BlendSlider chainBlendSlider;BlendLabel chainBlendLabel;
 bool chainBlendQueued=false,chainBlendEditSessionReady=false,presetRestoreInProgress=false;double chainBlendSendDueMs=0;int chainBlendSlot=-1,sprDeviceSlot=0,sprDeviceMode=0x85;float chainBlendQueuedValue=50;
 gp200::GP200Preset chainBlendDecodedPreset;std::uint64_t chainBlendDecodedRevision=0;int chainBlendDecodedSlot=-2;bool chainBlendDecodedValid=false,chainBlendDecodedConnected=false;
 void syncChainBlendControls();
 enum class PresetRestoreStepType {PatchVolume,PatchTempo,EffectChange,ParamChange,ToggleEffect,ReorderEffects,RoutingMode};
 struct PresetRestoreStep {PresetRestoreStepType type=PresetRestoreStepType::ParamChange;int blockIndex=-1,paramIndex=-1;juce::uint32 effectId=0;float value=0;bool shouldBeOn=false;gp200::RoutingOrder routingOrder{};int fxLoopSend=4,fxLoopReturn=4,routingMode=-1;};
 std::vector<PresetRestoreStep> presetRestoreSteps;
 void buildFullPresetRestoreSteps(const gp200::GP200Preset&,const juce::MemoryBlock&);
};
'''
s=(r.parent/'source/GP200Plugin/PluginEditor.cpp').read_text();a=s.index('AudioPluginAudioProcessorEditor::buildFullPresetRestoreSteps (');b=s.index('\n}',a)+2;base+='void '+s[a:b]+'\n'
a=s.index('AudioPluginAudioProcessorEditor::syncChainBlendControls ()');b=s.index('\n}',a)+2;base+='void '+s[a:b]+'\n'
base+=r'''
using namespace gp200;
juce::MemoryBlock data(){juce::MemoryBlock b;b.setSize(1176);b.v[106]=2;b.v[107]=8;for(int i=0;i<11;i++)b.v[108+i]=i;return b;}
void field(juce::MemoryBlock& b,int param,float value){std::memcpy(b.v.data()+effectBlockStart+10*effectBlockSize+paramsOffset+param*4,&value,4);}
int main(){int count=0;
for(int m=0;m<256;m++){assert(routingModeIsChain(m)==(m>=128&&m<=139));count++;}
for(int v=0;v<=100;v++){
 auto b=data();field(b,13,blendTagFloat());field(b,14,float(v));field(b,0,73);
 auto p=GP200PresetCodec::decodeLivePresetDump(b);assert(hasIndependentBlend(p));
 AudioPluginAudioProcessorEditor e;e.buildFullPresetRestoreSteps(p,b);
 auto n=e.presetRestoreSteps.size();assert(n>3);auto& value=e.presetRestoreSteps[n-3];auto& tag=e.presetRestoreSteps[n-2];
 assert(value.blockIndex==10&&value.paramIndex==14&&value.value==v);assert(tag.paramIndex==13&&std::bit_cast<std::uint32_t>(tag.value)==chainBlendTag);
 assert(e.presetRestoreSteps.back().type==AudioPluginAudioProcessorEditor::PresetRestoreStepType::ReorderEffects);
 MidiConnection m;m.currentPresetDecodedData=b;assert(m.sendIndependentBlend(float(v),false,0));assert(m.blendWritePending&&!m.canSaveCurrentPreset());
 assert(m.parameterWrites.size()==2&&m.parameterWrites[0].first==14&&m.parameterWrites[1].first==13);
 // An optimistic local edit must not count as physical confirmation.
 m.processBlendReadback();assert(m.blendWritePending);
 ++m.livePresetRevision;m.processBlendReadback();assert(!m.blendWritePending&&m.lastMessageText=="BLEND confirmed by pedal");count++;
}
for(float bad:{-1.f,101.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
 auto b=data();field(b,13,blendTagFloat());field(b,14,bad);auto p=GP200PresetCodec::decodeLivePresetDump(b);assert(!hasIndependentBlend(p));
 AudioPluginAudioProcessorEditor e;e.buildFullPresetRestoreSteps(p,b);auto n=e.presetRestoreSteps.size();assert(e.presetRestoreSteps[n-2].value==0&&e.presetRestoreSteps[n-3].value==0);count++;
}
{MidiConnection m;m.currentPresetDecodedData=data();field(m.currentPresetDecodedData,0,17);assert(!m.sendIndependentBlend(17,false,0));assert(m.sendIndependentBlend(50,true,0));assert(m.blendWriteExpected==50);assert(m.parameterWrites[0].first==0&&m.parameterWrites[0].second==100);count++;}
{MidiConnection m;m.currentPresetDecodedData=data();m.currentPresetDecodedData.v[107]=11;assert(m.sendIndependentBlend(50,true,0));assert(m.blendWriteExpected==50&&m.parameterWrites.size()==2);count++;}
{MidiConnection m;m.currentPresetDecodedData=data();assert(!m.sendIndependentBlend(50,true,1));m.routingModeSnapshot.mode=0;assert(!m.sendIndependentBlend(50,true,0));m.nativeRoutingMode=0;assert(m.sendFlexibleRouting(RoutingOrder{0,1,2,3,4,5,6,7,8,9,10},2,5,8,false,0));assert(m.routingValue==0);count++;}
{MidiConnection m;m.currentPresetDecodedData=data();assert(m.sendIndependentBlend(50,true,0));m.currentPresetDecodedData=data();++m.livePresetRevision;m.processBlendReadback();assert(!m.blendWritePending&&m.lastMessageText.find("not confirmed")!=std::string::npos);count++;}
{MidiConnection m;m.currentPresetDecodedData=data();assert(m.sendIndependentBlend(50,true,0));juce::Time::now=7000;m.processBlendReadback();assert(!m.blendWritePending&&!m.currentPresetDataIsLive);count++;}

// A loaded CHAIN preset has no historical native mode: use Parallel, not silent external Series.
{MidiConnection m;m.currentPresetDecodedData=data();assert(m.nativeRoutingMode==0);assert(m.sendFlexibleRouting(RoutingOrder{0,1,2,3,4,5,6,7,8,9,10},2,5,8,false,0));assert(m.routingValue==0);count++;}
{MidiConnection m;m.currentPresetDecodedData=data();m.nativeRoutingMode=1;assert(m.sendFlexibleRouting(RoutingOrder{0,1,2,3,4,5,6,7,8,9,10},2,5,8,false,0));assert(m.routingValue==1);count++;}
// First gesture sets requested position; loading and repaint alone do not convert.
{AudioPluginAudioProcessorEditor e;auto& m=e.midiConnection;m.currentPresetDecodedData=data();field(m.currentPresetDecodedData,0,17);e.syncChainBlendControls();assert(e.chainBlendSlider.enabled&&e.chainBlendSlider.value==17&&m.parameterWrites.empty());
 e.chainBlendQueued=true;e.chainBlendQueuedValue=31;e.chainBlendSlider.value=31;e.syncChainBlendControls();assert(m.blendWriteExpected==31&&m.blendWritePending);assert(m.parameterWrites[0].first==0&&m.parameterWrites[0].second==100);assert(e.chainBlendSlider.value==31);
 // An in-flight read may hide the live flag; keep the drag available and its latest value queued.
 m.currentPresetDataIsLive=false;e.chainBlendQueued=true;e.chainBlendQueuedValue=73;e.chainBlendSlider.value=73;e.syncChainBlendControls();assert(e.chainBlendSlider.enabled&&e.chainBlendQueued&&e.chainBlendSlider.value==73);
 m.currentPresetDataIsLive=true;++m.livePresetRevision;m.processBlendReadback();e.syncChainBlendControls();assert(m.blendWritePending&&m.blendWriteExpected==73&&!e.chainBlendQueued&&e.chainBlendSlider.value==73);count++;}
// Same-slot refresh and timeout recovery preserve editing without unsafe writes.
{AudioPluginAudioProcessorEditor e;auto& m=e.midiConnection;m.currentPresetDecodedData=data();e.syncChainBlendControls();assert(e.chainBlendEditSessionReady);
 m.currentPresetDataIsLive=false;m.currentPresetDecodedData.setSize(0);e.chainBlendQueued=true;e.chainBlendQueuedValue=81;e.chainBlendSlider.value=81;e.syncChainBlendControls();assert(e.chainBlendSlider.enabled&&e.chainBlendQueued&&m.parameterWrites.empty()&&e.chainBlendSlider.value==81);
 m.currentPresetDecodedData=data();m.currentPresetDataIsLive=true;e.syncChainBlendControls();assert(m.blendWriteExpected==81&&m.blendWritePending);count++;}
// No editing session may be enabled before the first valid live read.
{AudioPluginAudioProcessorEditor e;auto& m=e.midiConnection;m.currentPresetDataIsLive=false;e.syncChainBlendControls();assert(!e.chainBlendSlider.enabled);count++;}
// Rapid drag gestures coalesce; release sends the latest value.
{AudioPluginAudioProcessorEditor e;auto& m=e.midiConnection;m.currentPresetDecodedData=data();e.syncChainBlendControls();e.chainBlendSlider.down=true;e.chainBlendQueued=true;e.chainBlendQueuedValue=64;e.chainBlendSendDueMs=juce::Time::now+150;e.syncChainBlendControls();assert(e.chainBlendQueued&&m.parameterWrites.empty());e.chainBlendSlider.down=false;e.syncChainBlendControls();assert(m.blendWriteExpected==64&&m.blendWritePending);count++;}
// Switching slot drops an unsent gesture rather than applying it to the new preset.
{AudioPluginAudioProcessorEditor e;auto& m=e.midiConnection;m.currentPresetDecodedData=data();e.syncChainBlendControls();e.chainBlendQueued=true;e.chainBlendQueuedValue=99;m.currentSlot=1;e.syncChainBlendControls();assert(!e.chainBlendQueued&&m.parameterWrites.empty());count++;}
// Routing/Recall cancel editing and never initialize metadata incidentally.
{AudioPluginAudioProcessorEditor e;auto& m=e.midiConnection;m.currentPresetDecodedData=data();e.chainBlendSlot=0;e.chainBlendQueued=true;m.routingStage=1;e.syncChainBlendControls();assert(!e.chainBlendQueued&&!e.chainBlendSlider.enabled&&m.parameterWrites.empty());count++;}
std::cout<<count<<" CHAIN/BLEND production restore and transport checks passed; JUCE ports/codec/known-parameter DB simulated\n";
}
'''
base=base.replace('struct GP200PresetCodec {', 'struct GP200PresetCodec {static inline int decodeCalls=0;')
base=base.replace('decodeLivePresetDump(const juce::MemoryBlock& b){', 'decodeLivePresetDump(const juce::MemoryBlock& b){++decodeCalls;')
base=base.replace('int main(){int count=0;', '''int main(){int count=0;
{ AudioPluginAudioProcessorEditor e;auto& m=e.midiConnection;
 m.currentPresetDecodedData=data();field(m.currentPresetDecodedData,13,blendTagFloat());field(m.currentPresetDecodedData,14,21);
 const auto before=GP200PresetCodec::decodeCalls;e.syncChainBlendControls();assert(e.chainBlendSlider.value==21);
 for(int i=0;i<120;i++){e.syncChainBlendControls();}assert(GP200PresetCodec::decodeCalls==before+1);
 field(m.currentPresetDecodedData,14,79);++m.presetRevision;e.syncChainBlendControls();assert(e.chainBlendSlider.value==79);assert(GP200PresetCodec::decodeCalls==before+2);
 m.currentSlot=1;e.syncChainBlendControls();assert(GP200PresetCodec::decodeCalls==before+3);count+=123;
}
''')
base=base.replace('#include <thread>','#include <thread>\n#include <limits>')
(r/'chain_blend_test.cpp').write_text(base)
