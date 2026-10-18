// Actual MidiConnection methods, with MIDI endpoints bridged to native ARM firmware.
#include "nam2_transport_harness.h"
#include "nam2_transport_extracted.inc"
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace gp200;
std::vector<juce::uint8> fromHex(const std::string&s){std::vector<juce::uint8>b;for(std::size_t i=0;i+1<s.size();i+=2)b.push_back(static_cast<juce::uint8>(std::stoul(s.substr(i,2),nullptr,16)));return b;}
int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return 2;
 const int index=argc==3?std::stoi(argv[2]):0;
 auto mf=GP200ModSync::getManifestFile();mf.getParentDirectory().createDirectory();
 mf.replaceWithText("{\"schema\":\"gp200-mod-sync/v1\",\"product\":\"GP-200\",\"firmware\":\"V182\",\"firmware_sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"nam_amp_overrides\":[{\"effect_id\":117440513,\"display_name\":\"NAM\",\"parameter_profile\":\"NAM2\",\"parameter_count\":2,\"controls\":[\"Input\",\"Output\"],\"neutral\":50}],\"factory_amp_overrides\":[],\"factory_cab_overrides\":[]}");
 GP200ModSync::reloadIfChanged();
 MidiConnection midi;midi.currentPresetDecodedData.setSize(effectBlockStart+effectBlockCount*effectBlockSize);
 int replies=0;
 midi.output.onSend=[&](const juce::MidiMessage&message){
  std::ostringstream h;for(int i=0;i<message.getRawDataSize();++i)h<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(message.getRawData()[i]);
  std::cout<<"FRAME "<<h.str()<<std::endl;
  std::string line;if(!std::getline(std::cin,line))std::exit(3);
  const auto obj=juce::JSON::parse(juce::String(line));const auto response=obj.getProperty("responses",{});
  if(auto a=response.getArray())for(const auto& item:*a){auto b=fromHex(item.toString().s);if(midi.handleNamHotAck(b.data(),int(b.size())))++replies;}
 };
 if(!midi.startNamHotUpload(juce::File{juce::String(argv[1])},index)){std::cerr<<midi.irUploadStatusText.s;return 4;}
 for(int i=0;i<100&&midi.namHotStage!=MidiConnection::NamHotStage::None;++i){juce::Time::now+=1000;midi.processIRUpload();}
 const bool ok=midi.irUploadStatusText.contains("flash verified")&&GP200ModSync::getDisplayName(modsync::ampIds[static_cast<std::size_t>(index)])==juce::File{juce::String(argv[1])}.getFileNameWithoutExtension()&&replies==2&&midi.output.messages.size()==45;
 std::cout<<"RESULT "<<(ok?"PASS ":"FAIL ")<<midi.irUploadStatusText.s<<" replies="<<replies<<" sent="<<midi.output.messages.size()<<std::endl;
 return ok?0:5;
}
