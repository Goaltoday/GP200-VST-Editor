#include "nam2_transport_harness.h"
#include "nam2_transport_extracted.inc"
#include "../source/libgp200/GP200ModSyncProtocol.h"
#include <iostream>
using namespace gp200;
int checks=0;
#define CHECK(x) do { if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";std::abort();}++checks;}while(false)
void install(bool nam){
 std::string row=nam?R"("nam_amp_overrides":[{"effect_id":117440513,"display_name":"NAM","parameter_profile":"NAM2","parameter_count":2,"controls":["Input","Output"],"neutral":50}],"factory_amp_overrides":[{"effect_id":117440529,"display_name":"CLO9"}])":R"("nam_amp_overrides":[],"factory_amp_overrides":[{"effect_id":117440513,"display_name":"CLO1"},{"effect_id":117440529,"display_name":"CLO9"}])";
 auto text=std::string(R"({"schema":"gp200-mod-sync/v1","product":"GP-200","firmware":"V182","firmware_sha256":")")+std::string(64,'a')+"\","+row+"}";
 auto f=GP200ModSync::getManifestFile();f.getParentDirectory().createDirectory();CHECK(f.replaceWithText(text));GP200ModSync::reloadIfChanged();
}
void init(MidiConnection& m){m.currentPresetDecodedData.setSize(effectBlockStart+effectBlockCount*effectBlockSize);}
void drainToCommit(MidiConnection& m){for(int i=0;i<100&&m.irUploadPhase!=MidiConnection::IRUploadPhase::WaitingBeforeCommit;++i){juce::Time::now+=5000;m.processIRUpload();}CHECK(m.irUploadPhase==MidiConnection::IRUploadPhase::WaitingBeforeCommit);}
int main(){
 auto file=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("fixture.clo");
 std::vector<unsigned char> blob(0x2288);std::memcpy(blob.data(),"VTSI",4);auto w=[&](int at,unsigned v){for(int i=0;i<4;++i)blob[size_t(at+i)]=static_cast<unsigned char>(v>>(8*i));};w(4,0x1288);w(0x14,0x1200);w(0x80,128);w(0x84,1024);{std::ofstream f(file.p,std::ios::binary);f.write(reinterpret_cast<char*>(blob.data()),std::streamsize(blob.size()));}
 install(true);
 for(int index:{-1,0,6,71}){
  MidiConnection m;init(m);CHECK(!m.startFactoryAmpUpload(file,index,"BAD"));CHECK(m.output.messages.empty());CHECK(!m.startFactoryAmpRename(index,"BAD"));CHECK(m.output.messages.empty());
  GP200IRUpload built;CHECK(GP200SoundClone::buildFactoryAmpUpload(file,index,"BAD",built).failed());CHECK(built.chunks.empty());
 }
 for(bool rename:{false,true}){
  MidiConnection m;init(m);CHECK(rename?m.startFactoryAmpRename(8,"GOOD"):m.startFactoryAmpUpload(file,8,"GOOD"));CHECK(m.output.messages.size()==1);drainToCommit(m);auto before=m.output.messages.size();juce::Time::now+=5000;m.processIRUpload();CHECK(m.output.messages.size()==before+1);CHECK(m.irUploadPhase==MidiConnection::IRUploadPhase::WaitingAfterCommit);juce::Time::now+=5000;m.processIRUpload();CHECK(m.irUploadPhase==MidiConnection::IRUploadPhase::Idle);CHECK(GP200ModSync::isNamAmp(modsync::ampIds[0]));
 }
 // Change valid metadata between prepare and commit. Both HOT1/HOT2 stop.
 for(bool rename:{false,true}){
  install(false);MidiConnection m;init(m);CHECK(rename?m.startFactoryAmpRename(0,"STALE"):m.startFactoryAmpUpload(file,0,"STALE"));drainToCommit(m);auto before=m.output.messages.size();install(true);juce::Time::now+=5000;m.processIRUpload();CHECK(m.irUploadPhase==MidiConnection::IRUploadPhase::Idle);CHECK(m.output.messages.size()==before);CHECK(m.irUploadStatusText.contains("cancelled before commit"));CHECK(GP200ModSync::isNamAmp(modsync::ampIds[0]));
 }
 // Existing inactive/live guards still apply to HOT1.
 {MidiConnection m;init(m);m.currentPresetDataIsLive=false;CHECK(!m.startFactoryAmpUpload(file,8,"BAD"));CHECK(m.output.messages.empty());}
 {MidiConnection m;init(m);auto p=static_cast<unsigned char*>(m.currentPresetDecodedData.getData());auto id=modsync::ampIds[8];for(int i=0;i<4;++i)p[effectBlockStart+effectIdOffset+size_t(i)]=static_cast<unsigned char>(id>>(8*i));CHECK(!m.startFactoryAmpUpload(file,8,"BAD"));CHECK(m.output.messages.empty());}
 {MidiConnection m;init(m);CHECK(m.startFactoryAmpUpload(file,8,"GOOD"));drainToCommit(m);m.currentPresetDataIsLive=false;auto before=m.output.messages.size();juce::Time::now+=5000;m.processIRUpload();CHECK(m.irUploadPhase==MidiConnection::IRUploadPhase::Idle);CHECK(m.output.messages.size()==before);}
 // Input/Output MIDI values and reserved-parameter rejection use real encoding.
 {MidiConnection m;init(m);for(int param:{0,1})for(float value:{0.0f,50.0f,100.0f}){
   CHECK(m.sendParamChange(5,param,modsync::ampIds[0],value));auto bytes=m.output.messages.back().bytes;
   CHECK(bytes.size()==62);std::array<unsigned char,24> raw{};for(int i=0;i<24;++i)raw[size_t(i)]=static_cast<unsigned char>((bytes[size_t(13+2*i)]<<4)|bytes[size_t(14+2*i)]);
   CHECK(raw[12]==5&&raw[13]==param);float sent=0;std::memcpy(&sent,raw.data()+20,4);CHECK(sent==value);CHECK(juce::ByteOrder::littleEndianInt(raw.data()+16)==modsync::ampIds[0]);
  }auto before=m.output.messages.size();for(int param:{2,5,14})CHECK(!m.sendParamChange(5,param,modsync::ampIds[0],50));for(float value:{-1.0f,101.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})CHECK(!m.sendParamChange(5,0,modsync::ampIds[0],value));CHECK(m.output.messages.size()==before);
  m.updateCurrentPresetEffectId(5,modsync::ampIds[0]);auto data=static_cast<const unsigned char*>(m.currentPresetDecodedData.getData());for(int i=0;i<15;++i){float value=0;std::memcpy(&value,data+effectBlockStart+5*effectBlockSize+paramsOffset+size_t(i)*4,4);CHECK(value==(i<2?50.0f:0.0f));}
 }
 std::cout<<checks<<" NAM2 transport checks passed: actual MidiConnection start/commit bodies and SoundClone builders; ports/timing modeled.\n";
}
