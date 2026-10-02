from pathlib import Path
import subprocess
r=Path(__file__).resolve().parent
subprocess.run(['python3',str(r/'build_regressions.py')],check=True)
base=(r/'connection_regressions.cpp').read_text().split('struct Ribbon {')[0]
base=base.replace('#include <thread>', '#include <thread>\n#include <map>\n#include <sstream>\n#include <iomanip>\n#include <cmath>')
base=base.replace('using uint8=std::uint8_t;', 'using uint8=std::uint8_t;using uint32=std::uint32_t;constexpr int dontSendNotification=0;')
a=base.index('struct String:');b=base.index('struct CriticalSection',a)
base=base[:a]+r'''
struct String:std::string {using std::string::string;using std::string::operator=;String(int n):std::string(std::to_string(n)){}String(std::string s):std::string(s){}bool isEmpty()const{return empty();}bool isNotEmpty()const{return !empty();}String trim()const{auto a=find_first_not_of(" \r\n\t");return a==npos?String{}:String(substr(a,find_last_not_of(" \r\n\t")-a+1));}String substring(int a,int b)const{return substr(a,std::max(0,b-a));}};
struct MemoryBlock {std::vector<uint8> v;void setSize(size_t n){v.resize(n);}size_t getSize()const{return v.size();}void* getData(){return v.data();}const void* getData()const{return v.data();}void append(const void*p,size_t n){auto b=(const uint8*)p;v.insert(v.end(),b,b+n);}String toBase64Encoding()const{static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";String s;unsigned buffer=0;int bits=-6;for(auto c:v){buffer=(buffer<<8)|c;bits+=8;while(bits>=0){s+=chars[(buffer>>bits)&63];bits-=6;}}if(bits>-6)s+=chars[((buffer<<8)>>(bits+8))&63];while(s.size()%4)s+='=';return s;}bool fromBase64Encoding(const String& s){static const std::string chars="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";v.clear();unsigned buffer=0;int bits=-8;for(char c:s){if(c=='=')break;auto i=chars.find(c);if(i==std::string::npos){v.clear();return false;}buffer=(buffer<<6)|unsigned(i);bits+=6;if(bits>=0){v.push_back((buffer>>bits)&255);bits-=8;}}return true;}};
struct XmlElement {String tag;std::map<std::string,String> attrs;XmlElement(String s):tag(s){}void setAttribute(String k,String v){attrs[k]=v;}void setAttribute(String k,int v){attrs[k]=String(v);}int getIntAttribute(String k,int d)const{auto i=attrs.find(k);if(i==attrs.end())return d;try{return std::stoi(i->second);}catch(...){return d;}}String getStringAttribute(String k,String d)const{auto i=attrs.find(k);return i==attrs.end()?d:i->second;}bool getBoolAttribute(String k,bool d)const{return getIntAttribute(k,int(d))!=0;}bool hasAttribute(String k)const{return attrs.count(k)>0;}bool hasTagName(String t)const{return tag==t;}};
void copyXmlToBinary(const XmlElement& xml,MemoryBlock& b){std::ostringstream out;out<<std::quoted(std::string(xml.tag))<<'\n';for(auto& [k,v]:xml.attrs)out<<std::quoted(k)<<' '<<std::quoted(std::string(v))<<'\n';auto text=out.str();b.v.assign(text.begin(),text.end());}
std::unique_ptr<XmlElement> getXmlFromBinary(const void* p,int n){std::string s((const char*)p,n);std::istringstream in(s);std::string tag,k,v;if(!(in>>std::quoted(tag)))return {};auto xml=std::make_unique<XmlElement>(tag);while(in>>std::quoted(k)>>std::quoted(v))xml->attrs[k]=v;return xml;}
''' + base[b:]
# Xml conversion primitives are substitutes; tested production methods are unmodified bodies.
base=base.replace('struct GP200Preset {bool', 'struct GP200Preset {juce::String patchName{"preset"};bool')
base=base.replace('bool isConnected()const{return midiOutput!=nullptr;}', '''int getCurrentSlot()const{return currentSlot;}
bool sendPatchVolume(int){return true;}bool sendPatchTempoBpm(int){return true;}bool sendEffectChange(int,juce::uint32){return true;}bool sendParamChange(int,int,juce::uint32,float){return true;}bool sendEffectOnOff(int,bool){return true;}
void adoptCurrentPresetSnapshot(int slot,juce::String name,juce::MemoryBlock data){currentSlot=slot;currentPresetName=name;currentPresetDecodedData=data;currentPresetDataIsLive=false;++presetRevision;}
bool isConnected()const{return midiOutput!=nullptr;}''')
processor=(r.parent/'source/GP200Plugin/PluginProcessor.cpp').read_text();editor=(r.parent/'source/GP200Plugin/PluginEditor.cpp').read_text()
def extract(s,name,ret):
 a=s.index(name);b=s.index('\n}',a)+2;return ret+s[a:b]+'\n'
pnames=[('setGP200PresetSnapshotState','void '),('getGP200PresetRecallSnapshot','AudioPluginAudioProcessor::GP200PresetRecallSnapshot '),('getStateInformation','void '),('setStateInformation','void '),('hasSavedGP200PresetData','bool ')]
pbody=''.join(extract(processor,'AudioPluginAudioProcessor::'+n+' (',ret) for n,ret in pnames)
ebody=''.join(extract(editor,'AudioPluginAudioProcessorEditor::'+n+' (','void ') for n in ['saveCurrentPresetToProject','startFullPresetRestoreFromSnapshot','processFullPresetRestoreStep','finishFullPresetRestore'])
h=r'''
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
'''
t=r'''
using namespace gp200;
juce::MemoryBlock data(int s=2,int r=8){juce::MemoryBlock b;b.setSize(1176);auto p=(juce::uint8*)b.getData();p[106]=s;p[107]=r;for(int i=0;i<11;i++)p[108+i]=i;p[16]=71;return b;}
void init(MidiConnection& m,int mode=0x85,int s=2,int r=8){m.currentPresetDecodedData=data(s,r);m.routingModeSnapshot.mode=mode;juce::Time::now=0;}
void fresh(MidiConnection& m,const juce::MemoryBlock& wanted,int value){m.currentPresetDecodedData=wanted;m.currentPresetDataIsLive=true;++m.livePresetRevision;++m.presetRevision;m.processRoutingTransaction();assert(m.routingStage==5&&!m.canSaveCurrentPreset());m.routingModeSnapshot.mode=value;m.routingModeSnapshot.slot=m.currentSlot;++m.routingModeSnapshot.revision;m.processRoutingTransaction();assert(m.routingStage==0&&m.canSaveCurrentPreset());}
int main(){int count=0;
for(int slot:{0,1})for(int mode=0;mode<256;mode++) if(validRoutingModeValue(mode)){AudioPluginAudioProcessor p;MidiConnection m;init(m,mode,0,11);AudioPluginAudioProcessorEditor e(p,m);e.selected=slot;e.saveCurrentPresetToProject();auto saved=p.getGP200PresetRecallSnapshot(slot);assert(saved.routingMode==mode&&saved.data.v==m.currentPresetDecodedData.v);assert(p.getGP200PresetRecallSnapshot(1-slot).routingMode==-1);juce::MemoryBlock state;p.getStateInformation(state);AudioPluginAudioProcessor reopened;reopened.setStateInformation(state.getData(),state.getSize());auto recovered=reopened.getGP200PresetRecallSnapshot(slot);assert(recovered.routingMode==mode&&recovered.data.v==saved.data.v);m.routingModeSnapshot.mode=mode==0x85?0x95:0x85;AudioPluginAudioProcessorEditor recalled(reopened,m);recalled.selected=slot;recalled.startFullPresetRestoreFromSnapshot();assert(recalled.presetRestoreInProgress&&recalled.presetRestoreSteps.size()==2);assert(recalled.presetRestoreSteps.back().routingMode==mode);recalled.processFullPresetRestoreStep();auto n=m.output.messages.size();recalled.processFullPresetRestoreStep();assert(m.output.messages.size()==n&&recalled.presetRestoreInProgress);juce::Time::now=150;recalled.processFullPresetRestoreStep();assert(!recalled.presetRestoreInProgress&&m.routingStage==4&&!m.canSaveCurrentPreset());auto frame=m.output.messages.back().bytes;assert(routingModeResponse(frame.data(),frame.size())==mode);fresh(m,recovered.data,mode<2?(mode==0?0x85:0x95):mode);count++;}
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
'''
(r/'snapshot_regressions.cpp').write_text(base+h+pbody+ebody+t)
print('Save/Recall/DAW production methods extracted')
