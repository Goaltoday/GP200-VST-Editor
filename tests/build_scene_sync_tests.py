from pathlib import Path
r=Path(__file__).resolve().parent
base=(r/'connection_regressions.cpp').read_text().split('struct Ribbon {')[0]
base+=r'''
using namespace gp200;
juce::MemoryBlock syncData(){juce::MemoryBlock b;b.setSize(1176);b.v[106]=2;b.v[107]=8;for(int i=0;i<11;i++)b.v[108+i]=i;return b;}
std::array<std::uint8_t,46> notice(int slot,int scene){
 std::array<std::uint8_t,46> d{};const std::array<std::uint8_t,13> h{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,16,0,0,0};std::copy(h.begin(),h.end(),d.begin());
 std::array<std::uint8_t,16> p{5,0,12,0,10,15,0,0,0,0,0,0,static_cast<std::uint8_t>(scene),static_cast<std::uint8_t>(slot),0x43,0x53};
 for(int i=0;i<16;i++){d[13+2*i]=p[i]>>4;d[14+2*i]=p[i]&15;}d[45]=0xf7;return d;
}
int main(){int checks=0;
 for(int d=-32;d<=32;d++)for(bool inv:{false,true}){
  auto w=alignWord(d,inv);assert(validAlignWord(w)&&alignFrames(w)==d&&alignInverted(w)==inv);
  MidiConnection m;m.currentPresetDecodedData=syncData();assert(m.sendChainAdjustment(d,inv,0));assert(m.parameterWrites.size()==1&&m.parameterWrites[0].first==12);
  assert(std::bit_cast<std::uint32_t>(m.parameterWrites[0].second)==w);assert(!m.canSaveCurrentPreset());
  assert(!m.sendIndependentBlend(50,true,0)&&!m.sendChainAdjustment(0,false,0));m.processBlendReadback();assert(m.blendWritePending);
  ++m.livePresetRevision;m.processBlendReadback();assert(!m.blendWritePending&&m.lastMessageText=="ALIGN/PHASE confirmed by pedal");checks++;
 }
 for(int slot=0;slot<256;slot++)for(int scene=0;scene<4;scene++){
  auto n=notice(slot,scene);assert(sceneChangeSlot(n.data(),int(n.size()))==slot);MidiConnection m;m.currentSlot=slot;
  assert(m.handleSceneChangeNotification(n.data(),46)&&m.sceneChangeRevision==1&&m.sceneRefreshPending&&!m.currentPresetDataIsLive&&m.liveRefreshPending);checks++;
 }
 auto n=notice(0,1);
 for(int i=0;i<46;i++){auto bad=n;bad[i]^=0x80;assert(sceneChangeSlot(bad.data(),46)==-1);checks++;}
 for(int len=0;len<46;len++){assert(sceneChangeSlot(n.data(),len)==-1);checks++;}
 {auto old=notice(1,1);MidiConnection m;assert(m.handleSceneChangeNotification(old.data(),46));assert(!m.liveRefreshPending&&m.sceneChangeRevision==0);checks++;}
 {MidiConnection m;m.currentPresetDecodedData=syncData();m.presetDumpSlot=0;m.presetReadChunks={{1,2,3}};
  assert(m.handleSceneChangeNotification(n.data(),46));assert(m.presetDumpSlot==-1&&m.presetReadChunks.empty());assert(m.presetReadResumeMs==juce::Time::now+300);checks++;}
 {MidiConnection m;m.currentPresetDecodedData=syncData();m.currentPresetDataIsLive=false;assert(!m.sendChainAdjustment(0,false,0));m.currentPresetDataIsLive=true;m.routingStage=1;assert(!m.sendChainAdjustment(0,false,0));m.routingStage=0;assert(!m.sendChainAdjustment(33,false,0)&&!m.sendChainAdjustment(-33,false,0)&&!m.sendChainAdjustment(0,false,1));checks++;}
 {MidiConnection m;m.currentPresetDecodedData=syncData();assert(m.sendChainAdjustment(7,true,0));auto zero=std::bit_cast<float>(alignWord(0,false));std::memcpy(m.currentPresetDecodedData.v.data()+effectBlockStart+10*effectBlockSize+paramsOffset+48,&zero,4);++m.livePresetRevision;m.processBlendReadback();assert(!m.blendWritePending&&m.lastMessageText.find("not confirmed")!=std::string::npos&&m.parameterWrites.size()==1);checks++;}
 {MidiConnection m;m.currentPresetDecodedData=syncData();m.liveRefreshPending=true;m.liveRefreshDueMs=0;juce::Time::now=0;m.processPendingLivePresetRefresh();assert(m.output.messages.size()==1);m.presetDumpSlot=-1;m.liveRefreshPending=true;juce::Time::now=349;m.processPendingLivePresetRefresh();assert(m.output.messages.size()==1);juce::Time::now=350;m.processPendingLivePresetRefresh();assert(m.output.messages.size()==2);checks++;}
 std::cout<<checks<<" scene notification, ALIGN/PHASE readback, guard and traffic-budget production checks passed (simulated JUCE ports/codec)\n";
}
'''
(r/'scene_sync_test.cpp').write_text(base)
