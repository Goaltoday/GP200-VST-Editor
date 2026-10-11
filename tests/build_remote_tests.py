from pathlib import Path
r=Path(__file__).parent
base=(r/'connection_regressions.cpp').read_text().split('struct Ribbon {')[0]
base+=r'''
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
'''
(r/'remote_test.cpp').write_text(base)
