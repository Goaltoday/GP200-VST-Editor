#include "nam2_transport_harness.h"
#include "nam2_transport_extracted.inc"
#include <iostream>
#include <fstream>
#include <cstring>
using namespace gp200;
int checks=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::abort();}++checks;}while(false)
static std::vector<juce::uint8> unstage(const GP200IRUpload&u){std::vector<juce::uint8>b;for(auto&m:u.chunks){auto p=m.getRawData();const int n=m.getRawDataSize();CHECK(n>=14);CHECK(((p[12]<<7)|p[11])==int(b.size()));for(int i=13;i<n-1;i+=2){CHECK(p[i]<=15&&p[i+1]<=15);b.push_back(juce::uint8((p[i]<<4)|p[i+1]));}}return b;}
static std::vector<juce::uint8> response(unsigned nonce,unsigned status=1,unsigned physical=1){std::array<juce::uint8,28>b{};modsync::write16(b.data(),0x104b);modsync::write16(b.data()+2,24);modsync::write16(b.data()+4,nonce);modsync::write16(b.data()+6,status);modsync::write32(b.data()+8,GP200NAM::inplaceAckMagic);modsync::write32(b.data()+12,GP200NAM::binding);modsync::write32(b.data()+16,modsync::ampIds[1]);modsync::write32(b.data()+20,physical);modsync::write32(b.data()+24,7484);std::vector<juce::uint8>o{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,28,0,0,0};for(auto v:b){o.push_back(v>>4);o.push_back(v&15);}o.push_back(0xf7);return o;}
int main(int argc,char**argv){
 CHECK(argc==3);juce::File file{juce::String(argv[1])};std::vector<juce::uint8>w;CHECK(GP200NAM::readWeights(file,w).wasOk());CHECK(w.size()==7484);
 std::ifstream f(argv[2],std::ios::binary);std::vector<juce::uint8>expected((std::istreambuf_iterator<char>(f)),{});CHECK(expected==w);
 auto q=GP200NAM::query(0,123);CHECK(unstage(q).size()==28);auto upload=GP200NAM::upload(w,0,124);auto b=unstage(upload);CHECK(b.size()==7512);CHECK(modsync::read16(b.data()+6)==0x4e48);CHECK(modsync::read16(b.data()+4)==124);CHECK(std::vector<juce::uint8>(b.begin()+28,b.end())==w);CHECK(upload.chunks.size()==42);
 // Prepare/commit must exactly match the proven, unchanged staging transport.
 const std::vector<juce::uint8> prepare={0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,0x14,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,8,11,0,0,1,6,0,0,0,8,0,0,0,5,15,4,8,15,0,0,0,0,0,0,1,0,0,10,0xf7};CHECK(std::vector<juce::uint8>(upload.prepareMessage.getRawData(),upload.prepareMessage.getRawData()+upload.prepareMessage.getRawDataSize())==prepare);
 // Malformed configs (including missing null fields), scales, rates, containers.
 const auto original=juce::JSON::parse(file.loadFileAsString());const auto testFile=file.getParentDirectory().getChildFile("invalid.nam");
 for(int mode=0;mode<9;++mode){auto model=juce::JSON::parse(juce::JSON::toString(original));auto cfg=model.getProperty("config",{});switch(mode){case 0:model.getDynamicObject()->setProperty("sample_rate",44100);break;case 1:cfg.getDynamicObject()->setProperty("head_scale",0);break;case 2:cfg.getDynamicObject()->setProperty("extra",1);break;case 3:cfg.getDynamicObject()->fields.erase("head");cfg.getDynamicObject()->setProperty("extra",1);break;case 4:(*model.getProperty("weights",{}).getArray())[0]="NaN";break;case 5:(*model.getProperty("weights",{}).getArray())[1870]=1;break;case 6:model.getDynamicObject()->setProperty("architecture","LSTM");break;case 7:model.getProperty("weights",{}).getArray()->pop_back();break;case 8:cfg.getProperty("layers",{}).getArray()->pop_back();break;}
 CHECK(testFile.replaceWithText(juce::JSON::toString(model)));CHECK(GP200NAM::readWeights(testFile,w).failed());CHECK(w.empty());}
 auto container=new juce::DynamicObject();container->setProperty("architecture","SlimmableContainer");auto c=new juce::DynamicObject();juce::Array<juce::var> children;auto child=new juce::DynamicObject();child->setProperty("model",original);children.add(juce::var(child));c->setProperty("submodels",children);container->setProperty("config",c);juce::var cv(container);CHECK(testFile.replaceWithText(juce::JSON::toString(cv)));CHECK(GP200NAM::readWeights(testFile,w).wasOk());children.add(children[0]);c->setProperty("submodels",children);CHECK(testFile.replaceWithText(juce::JSON::toString(cv)));CHECK(GP200NAM::readWeights(testFile,w).failed());
 // Typed NAM2 metadata and an actual live inactive destination.
 auto root=new juce::DynamicObject();root->setProperty("schema","gp200-mod-sync/v1");root->setProperty("product","GP-200");root->setProperty("firmware","V182");root->setProperty("firmware_sha256",juce::String(std::string(64,'a')));auto row=new juce::DynamicObject();row->setProperty("effect_id",juce::int64(modsync::ampIds[1]));row->setProperty("display_name","NAM");row->setProperty("parameter_profile","NAM2");row->setProperty("parameter_count",2);row->setProperty("controls",juce::Array<juce::var>{"Input","Output"});row->setProperty("neutral",50);delete row;root->setProperty("nam_amp_overrides",juce::Array<juce::var>{});auto features=new juce::DynamicObject();features->setProperty("nam_hot_protocol","NH04");root->setProperty("features",features);auto eligible=new juce::DynamicObject();eligible->setProperty("logical_index",1);eligible->setProperty("effect_id",juce::int64(modsync::ampIds[1]));eligible->setProperty("package_bytes",9272);eligible->setProperty("package_offset",600000);root->setProperty("nam_inplace_destinations",juce::Array<juce::var>{eligible});root->setProperty("factory_amp_overrides",juce::Array<juce::var>{});root->setProperty("factory_cab_overrides",juce::Array<juce::var>{});auto mf=GP200ModSync::getManifestFile();mf.getParentDirectory().createDirectory();CHECK(mf.replaceWithText(juce::JSON::toString(root)));GP200ModSync::reloadIfChanged();CHECK(!GP200ModSync::isNamAmp(modsync::ampIds[1]));CHECK(GP200ModSync::canUseNamInPlace(1));CHECK(!GP200ModSync::canUseNamInPlace(0));
 const auto validManifest=mf.loadFileAsString();
 for(int mode=0;mode<5;++mode){auto bad=juce::JSON::parse(validManifest);auto a=bad.getProperty("nam_inplace_destinations",{}).getArray();auto row=(*a)[0];
  if(mode==0){a->add(row);}if(mode==1)row.getDynamicObject()->setProperty("package_bytes",7695);
  if(mode==2)row.getDynamicObject()->setProperty("effect_id",juce::int64(modsync::ampIds[0]));
  if(mode==3)row.getDynamicObject()->setProperty("package_offset",0x1fffff);
  if(mode==4)bad.getProperty("features",{}).getDynamicObject()->setProperty("nam_hot_protocol","NH03");
  CHECK(mf.replaceWithText(juce::JSON::toString(bad)));GP200ModSync::reloadIfChanged();CHECK(!GP200ModSync::canUseNamInPlace(1));CHECK(GP200ModSync::getNamMetadataStatus().contains("Invalid"));
  CHECK(mf.replaceWithText(validManifest));GP200ModSync::reloadIfChanged();CHECK(GP200ModSync::canUseNamInPlace(1));}
 MidiConnection midi;midi.currentPresetDecodedData.setSize(effectBlockStart+effectBlockCount*effectBlockSize);CHECK(midi.startNamHotUpload(file,1));CHECK(midi.namHotStage==MidiConnection::NamHotStage::Query);CHECK(midi.output.messages.size()==1);
 auto advance=[&](){for(int i=0;i<60&&midi.irUploadPhase!=MidiConnection::IRUploadPhase::WaitingForNamAck;++i){juce::Time::now+=1000;midi.processIRUpload();}CHECK(midi.irUploadPhase==MidiConnection::IRUploadPhase::WaitingForNamAck);};
 advance();const auto queryNonce=midi.namHotNonce;auto ack=response(queryNonce+99);CHECK(midi.handleNamHotAck(ack.data(),int(ack.size())));CHECK(midi.namHotStage==MidiConnection::NamHotStage::Query);
 ack=response(queryNonce);CHECK(midi.handleNamHotAck(ack.data(),int(ack.size())));CHECK(midi.namHotStage==MidiConnection::NamHotStage::Write);CHECK(midi.namHotNonce!=queryNonce);advance();CHECK(midi.irUploadStatusText.contains("readback"));
 ack=response(queryNonce);midi.handleNamHotAck(ack.data(),int(ack.size()));CHECK(midi.namHotStage==MidiConnection::NamHotStage::Write);
 ack=response(midi.namHotNonce,1,0);midi.handleNamHotAck(ack.data(),int(ack.size()));CHECK(midi.namHotStage==MidiConnection::NamHotStage::Write);
 ack=response(midi.namHotNonce);CHECK(midi.handleNamHotAck(ack.data(),int(ack.size())));CHECK(midi.namHotStage==MidiConnection::NamHotStage::None);CHECK(midi.irUploadStatusText.contains("flash verified"));
 CHECK(midi.startNamHotUpload(file,1));advance();juce::Time::now+=31000;midi.processIRUpload();CHECK(midi.namHotStage==MidiConnection::NamHotStage::None);CHECK(midi.irUploadStatusText.contains("write was not sent"));
 CHECK(midi.startNamHotUpload(file,1));advance();ack=response(midi.namHotNonce);midi.handleNamHotAck(ack.data(),int(ack.size()));advance();juce::Time::now+=31000;midi.processIRUpload();CHECK(midi.irUploadStatusText.contains("unconfirmed"));
 CHECK(midi.startNamHotUpload(file,1));midi.currentPresetDataIsLive=false;for(int i=0;i<10&&midi.namHotStage!=MidiConnection::NamHotStage::None;++i){juce::Time::now+=1000;midi.processIRUpload();}CHECK(midi.irUploadStatusText.contains("cancelled before final packet"));
 // An immediate native ACK arrives during the final send, before it returns.
 MidiConnection immediate;immediate.currentPresetDecodedData.setSize(effectBlockStart+effectBlockCount*effectBlockSize);
 int immediateAcks=0;
 immediate.output.onSend=[&](const juce::MidiMessage& message){
  const auto* data=message.getRawData();const int size=message.getRawDataSize();
  if(size<14)return;
  const int total=data[9]|(data[10]<<7),offset=data[11]|(data[12]<<7);
  const int amount=(size-14)/2;
  if((total==28||total==7528)&&offset+amount==total){
   CHECK(immediate.irUploadPhase==MidiConnection::IRUploadPhase::WaitingForNamAck);
   auto reply=response(immediate.namHotNonce);++immediateAcks;
   CHECK(immediate.handleNamHotAck(reply.data(),int(reply.size())));
  }
 };
 CHECK(immediate.startNamHotUpload(file,1));
 for(int i=0;i<60&&immediate.namHotStage!=MidiConnection::NamHotStage::None;++i){juce::Time::now+=1000;immediate.processIRUpload();}
 CHECK(immediateAcks==2);CHECK(immediate.namHotStage==MidiConnection::NamHotStage::None);CHECK(immediate.irUploadStatusText.contains("flash verified"));
 CHECK(immediate.output.messages.size()==45); // 2 prepare, 1 HQ, 42 HN; no commit.
 for(const auto& message:immediate.output.messages){auto p=message.getRawData();CHECK(!(p[9]==12&&message.getRawDataSize()==38));}
 // Cancel before the final model packet can dispatch the write.
 MidiConnection cancelled;cancelled.currentPresetDecodedData.setSize(effectBlockStart+effectBlockCount*effectBlockSize);
 CHECK(cancelled.startNamHotUpload(file,1));juce::Time::now+=1000;cancelled.processIRUpload();
 auto ready=response(cancelled.namHotNonce);CHECK(cancelled.handleNamHotAck(ready.data(),int(ready.size())));
 for(int i=0;i<41;++i){juce::Time::now+=1000;cancelled.processIRUpload();}
 CHECK(cancelled.irUploadChunkIndex==41);auto sent=cancelled.output.messages.size();cancelled.currentPresetDataIsLive=false;
 juce::Time::now+=1000;cancelled.processIRUpload();CHECK(cancelled.output.messages.size()==sent);CHECK(cancelled.irUploadStatusText.contains("cancelled before final packet"));
 // Explicit NH01 errors may have ID zero and must not be mistaken for silence.
 MidiConnection rejected;rejected.currentPresetDecodedData.setSize(effectBlockStart+effectBlockCount*effectBlockSize);
 CHECK(rejected.startNamHotUpload(file,1));juce::Time::now+=1000;rejected.processIRUpload();
 auto invalidDestination=response(rejected.namHotNonce,4,0xffffffffu);
 for(int i=0;i<8;++i)invalidDestination[13+16*2+i]=0;
 CHECK(rejected.handleNamHotAck(invalidDestination.data(),int(invalidDestination.size())));
 CHECK(rejected.namHotStage==MidiConnection::NamHotStage::None);CHECK(rejected.irUploadStatusText.contains("excluded"));
 // Names are committed to metadata only after the matching NH02 write ACK.
 CHECK(GP200ModSync::getDisplayName(modsync::ampIds[1])=="sentinel");
 MidiConnection names;names.currentPresetDecodedData.setSize(effectBlockStart+effectBlockCount*effectBlockSize);
 CHECK(names.startNamHotUpload(file,1,"NEW_NAM"));CHECK(GP200ModSync::getDisplayName(modsync::ampIds[1])!="NEW_NAM");
 juce::Time::now+=1000;names.processIRUpload();auto namesAck=response(names.namHotNonce);names.handleNamHotAck(namesAck.data(),int(namesAck.size()));
 for(int i=0;i<42;++i){juce::Time::now+=1000;names.processIRUpload();}
 CHECK(GP200ModSync::getDisplayName(modsync::ampIds[1])!="NEW_NAM");
 namesAck=response(names.namHotNonce);names.handleNamHotAck(namesAck.data(),int(namesAck.size()));
 CHECK(GP200ModSync::getDisplayName(modsync::ampIds[1])=="NEW_NAM");CHECK(GP200ModSync::isNamAmp(modsync::ampIds[1]));
 auto namedBlob=unstage(GP200NAM::upload(expected,0,7,"TWELVE_CHARS"));CHECK(namedBlob.size()==7528);CHECK(namedBlob[8]==2);
 CHECK(std::memcmp(namedBlob.data()+7512,"TWELVE_CHARS",12)==0);CHECK(namedBlob.back()==0);
 CHECK(GP200NAM::upload(expected,0,7,"TOO_LONG_12345").chunks.empty());
 MidiConnection old;old.currentPresetDecodedData.setSize(effectBlockStart+effectBlockCount*effectBlockSize);CHECK(old.startNamHotUpload(file,1));
 juce::Time::now+=1000;old.processIRUpload();auto legacy=response(old.namHotNonce);
 auto oldRaw=GP200NAM::ackMagic;for(int i=0;i<4;++i){auto byte=(oldRaw>>(8*i))&255;legacy[13+(8+i)*2]=byte>>4;legacy[14+(8+i)*2]=byte&15;}
 CHECK(old.handleNamHotAck(legacy.data(),int(legacy.size())));CHECK(old.namHotStage==MidiConnection::NamHotStage::None);CHECK(old.output.messages.size()==2);CHECK(old.irUploadStatusText.contains("update firmware"));
 auto iq=unstage(GP200NAM::query(1,23,true,true));CHECK(iq[8]==4);
 auto iw=unstage(GP200NAM::upload(expected,1,24,"INPLACE",true));CHECK(iw[8]==4);CHECK(iw.size()==7528);
 GP200ModSync::reloadIfChanged();CHECK(GP200ModSync::canUseNamInPlace(1));CHECK(!GP200ModSync::canUseNamInPlace(0));
 std::cout<<checks<<" NAM import, weight order, wire and production transfer checks passed\n";
}
