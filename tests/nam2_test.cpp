#include "../source/libgp200/GP200ModSync.h"
#include "../source/libgp200/GP200EffectParamDatabase.h"
#include "../source/libgp200/GP200EffectDatabase.h"
#include "../source/libgp200/GP200ModSyncProtocol.h"
#include "../source/libgp200/GP200SoundClone.h"
#include <iostream>
using namespace gp200;
int checks=0;
#define CHECK(x) do { if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";std::abort();}++checks;}while(false)
juce::var object(){return new juce::DynamicObject();}
void set(juce::var& o,juce::String k,juce::var v){o.getDynamicObject()->setProperty(k,std::move(v));}
juce::var entry(std::uint32_t id,const char* name){auto x=object();set(x,"effect_id",juce::int64(id));set(x,"display_name",name);return x;}
juce::var manifest(int count=1,juce::String firmware="V182"){
 auto root=object();set(root,"schema","gp200-mod-sync/v1");set(root,"product","GP-200");set(root,"firmware",firmware);set(root,"firmware_sha256",juce::String(std::string(64,'a')));
 juce::Array<juce::var> nam,amp,cab;
 for(int i=0;i<count;++i){auto x=entry(modsync::ampIds[size_t(i)],("NAM"+std::to_string(i+1)).c_str());set(x,"parameter_profile","NAM2");set(x,"parameter_count",2);set(x,"neutral",50);juce::Array<juce::var> names;names.add("Input");names.add("Output");set(x,"controls",names);nam.add(x);}
 amp.add(entry(modsync::ampIds[8],"CLO9"));cab.add(entry(0x0a000000,"CAB1"));set(root,"nam_amp_overrides",nam);set(root,"factory_amp_overrides",amp);set(root,"factory_cab_overrides",cab);return root;
}
void install(const juce::var& m){auto f=GP200ModSync::getManifestFile();f.getParentDirectory().createDirectory();CHECK(f.replaceWithText(juce::JSON::toString(m)));GP200ModSync::reloadIfChanged();}
juce::var live(bool clo=true){auto x=object();juce::Array<juce::var> amp,cab;if(clo)amp.add(entry(modsync::ampIds[8],"LIVE_CLO"));cab.add(entry(0x0a000000,"LIVE_CAB"));set(x,"factory_amp_overrides",amp);set(x,"factory_cab_overrides",cab);return x;}
void verifyProfile(std::uint32_t id){CHECK(GP200ModSync::isNamAmp(id));CHECK(!GP200ModSync::isCustomCloAmp(id));auto p=GP200EffectParamDatabase::findParamsForEffect(id,"AMP");CHECK(p&&p->count==2);for(int i=0;i<2;++i){CHECK(p->params[i].idx==i);CHECK(std::string(p->params[i].name)==(i==0?"Input":"Output"));CHECK(p->params[i].minValue==0&&p->params[i].maxValue==100&&p->params[i].defaultValue==50);CHECK(!GP200EffectParamDatabase::hasDiscreteOptions(id,i));}CHECK(GP200EffectParamDatabase::findParam(id,2)==nullptr);}
int main(int argc,char**){
 if(argc>1){GP200ModSync::reloadIfChanged();verifyProfile(modsync::ampIds[0]);CHECK(GP200ModSync::getDisplayName(modsync::ampIds[0])=="NAM1");CHECK(GP200ModSync::canWriteFactoryClo(8));std::cout<<checks<<" cold-process NAM2 cache checks passed.\n";return 0;}
 // Import before any getter: MIDI must not erase the NAM startup import.
 auto f=GP200ModSync::getManifestFile();f.getParentDirectory().createDirectory();CHECK(f.replaceWithText(juce::JSON::toString(manifest(5))));CHECK(GP200ModSync::replaceFromDevice(live(),1024));
 for(int i=0;i<5;++i){verifyProfile(modsync::ampIds[size_t(i)]);CHECK(!GP200ModSync::canWriteFactoryClo(i));}
 CHECK(GP200ModSync::canWriteFactoryClo(8));CHECK(!GP200ModSync::canWriteFactoryClo(-1));CHECK(!GP200ModSync::canWriteFactoryClo(71));CHECK(!GP200ModSync::canWriteFactoryClo(6));
 CHECK(GP200EffectDatabase::getEffectName(modsync::ampIds[0])=="NAM1");CHECK(GP200EffectDatabase::getEffectDescription(modsync::ampIds[0]).contains("not verified by MIDI"));
 auto cached=juce::JSON::parse(f.loadFileAsString());CHECK(cached.getProperty("nam_amp_overrides",{}).getArray()->size()==5);CHECK(cached.getProperty("firmware",{}).toString()=="V182");CHECK(cached.getProperty("firmware_sha256",{}).toString().length()==64);
 GP200ModSync::recordFactoryCabOverride(0x0a000000,"NEWCAB","a.wav");GP200ModSync::recordFactoryAmpOverride(modsync::ampIds[8],"NEWCLO","a.clo");verifyProfile(modsync::ampIds[0]);CHECK(GP200ModSync::getDisplayName(0x0a000000)=="NEWCAB");
 GP200ModSync::recordFactoryAmpOverride(modsync::ampIds[0],"BAD","bad.clo");CHECK(GP200ModSync::getDisplayName(modsync::ampIds[0])=="NAM1");
 // A real cold cache reload, not just in-memory retention.
 CHECK(f.replaceWithText(f.loadFileAsString()+"\n"));GP200ModSync::reloadIfChanged();verifyProfile(modsync::ampIds[0]);
 // Destination indices must match the public Factory AMP enumeration.
 int n=0;for(auto effect:GP200EffectDatabase::getEffectsForModule("AMP")){if((effect.effectId>>24)==15)continue;CHECK(effect.effectId==modsync::ampIds[size_t(n++)]);}CHECK(n==71);
 // Invalid/mixed metadata may not remove NAM protection or enable CLO writes.
 for(int mode=0;mode<14;++mode){auto bad=manifest();auto& row=(*bad.getProperty("nam_amp_overrides",{}).getArray())[0];
  switch(mode){case 0:set(row,"parameter_count",5);break;case 1:set(row,"neutral",0);break;case 2:set(row,"effect_id",juce::int64(0xdeadbeef));break;case 3:set(row,"parameter_profile","CLO5");break;case 4:set(bad,"firmware_sha256","short");break;case 5:set(bad,"product","GP-200LT");break;case 6:bad.getProperty("nam_amp_overrides",{}).getArray()->add(row);break;case 7:set(row,"display_name","Too long NAM model name");break;case 8:set(row,"controls",juce::Array<juce::var>{"Output","Input"});break;case 9:bad.getProperty("factory_amp_overrides",{}).getArray()->add(entry(modsync::ampIds[0],"COLLISION"));break;
  case 10:set((*bad.getProperty("factory_amp_overrides",{}).getArray())[0],"parameter_profile","NAM2");break;
  case 11:set(row,"display_name","bad\nname");break;
  case 12:set(bad,"nam_amp_overrides",object());break;
  case 13:{const auto serialized=juce::JSON::toString(row);for(int i=1;i<6;++i){auto copy=juce::JSON::parse(serialized);set(copy,"effect_id",juce::int64(modsync::ampIds[size_t(i)]));bad.getProperty("nam_amp_overrides",{}).getArray()->add(copy);}}break;}
  install(bad);CHECK(GP200ModSync::isNamAmp(modsync::ampIds[0]));CHECK(!GP200ModSync::canWriteFactoryClo(8));CHECK(GP200ModSync::getNamMetadataStatus().contains("Invalid"));CHECK(!GP200ModSync::replaceFromDevice(live(),1024));install(manifest(5));CHECK(GP200ModSync::canWriteFactoryClo(8));
 }
 // Live CLO contradiction must reject atomically and retain NAM.
 auto conflict=live();conflict.getProperty("factory_amp_overrides",{}).getArray()->add(entry(modsync::ampIds[0],"WRONG"));CHECK(!GP200ModSync::replaceFromDevice(conflict,1024));verifyProfile(modsync::ampIds[0]);CHECK(!GP200ModSync::canWriteFactoryClo(8));install(manifest());
 // Simulated disk failure: accepted live metadata and NAM stay available.
 juce::File::failWrites=true;CHECK(!GP200ModSync::replaceFromDevice(live(),1024));verifyProfile(modsync::ampIds[0]);CHECK(GP200ModSync::getDisplayName(modsync::ampIds[8])=="LIVE_CLO");juce::File::failWrites=false;install(manifest(3));CHECK(GP200ModSync::isNamAmp(modsync::ampIds[2]));CHECK(GP200ModSync::replaceFromDevice(live(),1024));
 // Stock manifest is an explicit replacement: remove NAM and restore stock ABI.
 install(manifest(0,"V180"));CHECK(!GP200ModSync::isNamAmp(modsync::ampIds[0]));CHECK(GP200EffectParamDatabase::findParamsForEffect(modsync::ampIds[0])->count!=2);CHECK(GP200EffectParamDatabase::findParamsForEffect(modsync::ampIds[8])->count==5);
 for(auto id:modsync::ampIds){auto one=manifest();one.getProperty("factory_amp_overrides",{}).getArray()->clear();auto& row=(*one.getProperty("nam_amp_overrides",{}).getArray())[0];set(row,"effect_id",juce::int64(id));install(one);verifyProfile(id);CHECK(GP200EffectDatabase::getEffectName(id)=="NAM1");}
 // Builders themselves reject NAM, stock and invalid indices before staging.
 install(manifest());GP200IRUpload out;CHECK(GP200SoundClone::buildFactoryAmpRename(0,"Oops",out).failed());CHECK(out.chunks.empty());CHECK(GP200SoundClone::buildFactoryAmpRename(6,"Oops",out).failed());CHECK(GP200SoundClone::buildFactoryAmpRename(8,"GOOD",out).wasOk());CHECK(!out.chunks.empty());
 std::cout<<checks<<" NAM2 checks passed: actual ModSync, parameter/catalog DB and SoundClone builders; JUCE/filesystem/JSON modeled.\n";
}
