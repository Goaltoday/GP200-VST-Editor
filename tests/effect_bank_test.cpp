#include "../source/libgp200/GP200ModSync.h"
#include "../source/libgp200/GP200EffectDatabase.h"
#include "../source/libgp200/GP200EffectParamDatabase.h"
#include <chrono>
#include <iostream>
#include <functional>
#include "nam2_transport_harness.h"
#include "effect_bank_midi_extracted.inc"
namespace gp200 {struct GP200EffectSlot{juce::uint32 effectId=0;std::array<float,15> params{};};}
struct EffectBlockComponent{
 gp200::GP200EffectSlot effect;bool blendForDisplay=false;juce::String module;std::function<juce::String(juce::uint32)> effectDisplayNameResolver;
 struct Slider{double value=0;double getValue()const{return value;}void setValue(double v,int){value=v;}};
 struct Label{juce::String text;void setText(juce::String t,int){text=t;}};
 struct Control{int paramIndex=0;bool usesDiscreteOptions=true;std::unique_ptr<Slider> slider=std::make_unique<Slider>();std::unique_ptr<Label> valueLabel=std::make_unique<Label>();};
 std::array<bool,15> delayTimeSyncDisplayed{};
 std::vector<Control> parameterControls;int rebuilds=0;void scheduleDelaySyncControlRebuild(){++rebuilds;}
 juce::String getBlockName()const{return module;}juce::String getEffectName()const;juce::String getEffectDisplayName(juce::uint32,const juce::String&)const;
 void setParameterValueForDisplay(int,float);
};
#include "effect_bank_ui_extracted.inc"
using namespace gp200;
int checks=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::abort();}++checks;}while(false)
juce::var read(const std::filesystem::path& p){return juce::JSON::parse(juce::File(p).loadFileAsString());}
void install(const juce::var& root){auto f=GP200ModSync::getEffectBankFile();f.getParentDirectory().createDirectory();CHECK(f.replaceWithText(juce::JSON::toString(root)));GP200ModSync::reloadIfChanged();}
void set(juce::var& x,juce::String k,juce::var v){x.getDynamicObject()->setProperty(k,std::move(v));}
void verify(const juce::var& row){
 auto id=static_cast<juce::uint32>(static_cast<juce::int64>(row.getProperty("destination_effect_id",{})));
 auto source=static_cast<juce::uint32>(static_cast<juce::int64>(row.getProperty("source_effect_id",{})));
 auto module=row.getProperty("destination_module",{}).toString();
 CHECK(GP200EffectDatabase::getEffectName(id,module)==row.getProperty("display_name",{}).toString());CHECK(GP200EffectParamDatabase::resolveEffectIdForModule(id,module)==source);
 auto entry=GP200ModSync::findEffectBankEntry(id,module);CHECK(entry&&entry->destinationEffectId==id&&entry->sourceEffectId==source);
 const auto* p=GP200EffectParamDatabase::findParamsForEffect(id,module);const auto* expected=row.getProperty("parameters",{}).getArray();CHECK(p&&p->effectId==id&&p->count==expected->size());
 for(int i=0;i<p->count;++i){auto& a=p->params[i];auto& b=(*expected)[i];CHECK(a.idx==static_cast<int>(b.getProperty("index",{})));CHECK(juce::String(a.name)==b.getProperty("name",{}).toString());CHECK(a.minValue==static_cast<float>(static_cast<double>(b.getProperty("minimum",{}))));CHECK(a.maxValue==static_cast<float>(static_cast<double>(b.getProperty("maximum",{}))));CHECK(a.defaultValue==static_cast<float>(static_cast<double>(b.getProperty("default",{}))));CHECK(a.step==static_cast<float>(static_cast<double>(b.getProperty("step",{}))));if(GP200EffectParamDatabase::hasDiscreteOptions(id,a.idx,module)){CHECK(GP200EffectParamDatabase::getDiscreteOptionMinimum(id,a.idx,module)==a.minValue);CHECK(GP200EffectParamDatabase::getDiscreteOptionMaximum(id,a.idx,module)==a.maxValue);CHECK(GP200EffectParamDatabase::getDiscreteOptionLabel(id,a.idx,a.defaultValue,module).isNotEmpty());}if(a.minValue==0&&a.maxValue==1&&a.step==1)CHECK(GP200EffectParamDatabase::hasDiscreteOptions(id,a.idx,module));}
}
int main(int argc,char**argv){if(argc==3){GP200ModSync::reloadIfChanged();CHECK(GP200ModSync::hasEffectBank());auto four=read(std::filesystem::path(argv[1])/"four_blocks.json");for(const auto& row:*four.getProperty("slots",{}).getArray())verify(row);std::cout<<checks<<" cold-process bank checks passed.\n";return 0;}CHECK(argc==2);std::filesystem::path fixtures=argv[1];auto all=read(fixtures/"all_destinations.json");install(all);CHECK(GP200ModSync::hasEffectBank());CHECK(all.getProperty("slots",{}).getArray()->size()==80);for(const auto& row:*all.getProperty("slots",{}).getArray())verify(row);
 // Every source used by at least one supported assignment (130/140 catalog entries).
 auto cases=read(fixtures/"source_cases.json");for(const auto& row:*cases.getArray()){auto doc=juce::JSON::parse(juce::JSON::toString(all));juce::Array<juce::var> one;one.add(row);set(doc,"slots",one);install(doc);verify(row);
 if(row.getProperty("source_module",{}).toString()=="DLY"){
  auto id=static_cast<juce::uint32>(static_cast<juce::int64>(row.getProperty("destination_effect_id",{})));auto set=GP200EffectParamDatabase::findParamsForEffect(id,row.getProperty("destination_module",{}).toString());
  for(int i=0;i<set->count;++i)if(isDelayTimeParameter(set->params[i])){int sync=findDelayTimeSyncParamIndex(*set,set->params[i].idx);if(sync>=0){GP200EffectSlot effect;effect.params[size_t(sync)]=1;CHECK(isDelayTimeSyncEnabled(effect,*set,set->params[i].idx));effect.params[size_t(sync)]=0;CHECK(!isDelayTimeSyncEnabled(effect,*set,set->params[i].idx));}}
 }
 }
 auto four=read(fixtures/"four_blocks.json");install(four);for(const auto& row:*four.getProperty("slots",{}).getArray()){
  verify(row);EffectBlockComponent block;block.effect.effectId=static_cast<juce::uint32>(static_cast<juce::int64>(row.getProperty("destination_effect_id",{})));block.module=row.getProperty("destination_module",{}).toString();block.effectDisplayNameResolver=[](juce::uint32){return juce::String("OLD_STOCK_NAME");};CHECK(block.getEffectName()==row.getProperty("display_name",{}).toString());CHECK(block.getEffectDisplayName(block.effect.effectId,"STOCK")==row.getProperty("display_name",{}).toString());CHECK(usesDelayTimeControls(block.effect,block.module)==(row.getProperty("source_module",{}).toString()=="DLY"));
  for(const auto& param:*row.getProperty("parameters",{}).getArray()){
   const int idx=static_cast<int>(param.getProperty("index",{}));if(!GP200EffectParamDatabase::hasDiscreteOptions(block.effect.effectId,idx,block.module))continue;
   block.parameterControls.clear();EffectBlockComponent::Control control;control.paramIndex=idx;block.parameterControls.push_back(std::move(control));const auto val=static_cast<float>(static_cast<double>(param.getProperty("default",{})));const int before=block.rebuilds;block.setParameterValueForDisplay(idx,val);if(block.rebuilds==before)CHECK(block.parameterControls[0].valueLabel->text==GP200EffectParamDatabase::getDiscreteOptionLabel(block.effect.effectId,idx,val,block.module));
  }
 }
 CHECK(GP200EffectDatabase::getEffectName(10,"PRE")!=GP200EffectDatabase::getEffectName(10,"DST"));CHECK(GP200EffectDatabase::getEffectName(10,"DIST")==GP200EffectDatabase::getEffectName(10,"DST"));CHECK(GP200EffectDatabase::getEffectName(10,"MOD")=="AC Boost");
 bool bounded=false;for(const auto& row:*four.getProperty("slots",{}).getArray()){if(row.getProperty("profile",{}).toString().contains("2000")){auto id=static_cast<juce::uint32>(static_cast<juce::int64>(row.getProperty("destination_effect_id",{})));auto p=GP200EffectParamDatabase::findParamsForEffect(id,"PRE");CHECK(p->params[1].maxValue==2000.0f);bounded=true;}}CHECK(bounded);
 // A fresh process loads the same persisted bank with no live MIDI.
 // Unchanged sync readback must not rebuild controls on each UI refresh;
 // a changed mode (including an echo after a local edit) still requests rebuild.
 for(const auto& row:*cases.getArray())if(row.getProperty("source_module",{}).toString()=="DLY"){
  auto doc=juce::JSON::parse(juce::JSON::toString(four));set(doc,"slots",juce::Array<juce::var>{row});install(doc);EffectBlockComponent block;block.module=row.getProperty("destination_module",{}).toString();block.effect.effectId=static_cast<juce::uint32>(static_cast<juce::int64>(row.getProperty("destination_effect_id",{})));const auto* p=GP200EffectParamDatabase::findParamsForEffect(block.effect.effectId,block.module);
  for(int i=0;i<p->count;++i)if(isDelayTimeParameter(p->params[i])){int sync=findDelayTimeSyncParamIndex(*p,p->params[i].idx);if(sync<0)continue;block.effect.params[size_t(sync)]=0;int before=block.rebuilds;for(int tick=0;tick<20;++tick)block.setParameterValueForDisplay(sync,0);CHECK(block.rebuilds==before);block.effect.params[size_t(sync)]=1;block.setParameterValueForDisplay(sync,1);CHECK(block.rebuilds>before);block.delayTimeSyncDisplayed[size_t(p->params[i].idx)]=true;}
 }
 install(four);
 // Default initialization and edits retain the physical destination ID in all four blocks.
 for(const auto& row:*four.getProperty("slots",{}).getArray()){
  const auto module=row.getProperty("destination_module",{}).toString();const int block=module=="PRE"?0:module=="WAH"?1:module=="DST"?2:6;
  const auto id=static_cast<juce::uint32>(static_cast<juce::int64>(row.getProperty("destination_effect_id",{})));
  MidiConnection midi;midi.currentPresetDecodedData.setSize(effectBlockStart+effectBlockCount*effectBlockSize);midi.updateCurrentPresetEffectId(block,id);
  CHECK(midi.output.messages.empty());
  for(const auto& param:*row.getProperty("parameters",{}).getArray()){
   const int idx=static_cast<int>(param.getProperty("index",{}));const float expected=static_cast<float>(static_cast<double>(param.getProperty("default",{})));
   float actual=0;const auto* data=static_cast<const unsigned char*>(midi.currentPresetDecodedData.getData());std::memcpy(&actual,data+effectBlockStart+size_t(block)*effectBlockSize+paramsOffset+size_t(idx)*4,4);CHECK(actual==expected);
   CHECK(midi.sendParamChange(block,idx,id,expected));const auto& frame=midi.output.messages.back().bytes;CHECK(frame.size()==62);std::array<unsigned char,24> decoded{};for(int i=0;i<24;++i)decoded[size_t(i)]=static_cast<unsigned char>((frame[size_t(13+i*2)]<<4)|frame[size_t(14+i*2)]);CHECK(decoded[12]==block&&decoded[13]==idx);CHECK(juce::ByteOrder::littleEndianInt(decoded.data()+16)==id);
  }
  CHECK(midi.output.messages.size()==size_t(row.getProperty("parameters",{}).getArray()->size()));
 }
 // No filesystem access or parse on warm getters, even across 100,000 iterations.
 auto stats=juce::File::statCalls,reads=juce::File::readCalls,parses=juce::JSON::parseCalls;
 auto start=std::chrono::steady_clock::now();for(int i=0;i<100000;++i){auto p=GP200EffectParamDatabase::findParamsForEffect(10,"PRE");CHECK(p&&p->count>0);(void)GP200EffectDatabase::getEffectName(10,"DST");(void)GP200ModSync::getRevision();(void)GP200ModSync::isNamAmp(0x07000001);}
 auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();CHECK(juce::File::statCalls==stats);CHECK(juce::File::readCalls==reads);CHECK(juce::JSON::parseCalls==parses);std::cout<<"100000 warm iterations (4 getters): "<<us<<" us; zero file stats, reads or JSON parses.\n";
 // Polling is bounded to once per second; unchanged files are never parsed again.
 juce::Time::now=0;GP200ModSync::pollForChanges();stats=juce::File::statCalls;parses=juce::JSON::parseCalls;for(int i=1;i<20;++i){juce::Time::now=i*50;GP200ModSync::pollForChanges();}CHECK(juce::File::statCalls==stats);CHECK(juce::JSON::parseCalls==parses);
 // Snapshot ownership survives replacement. Partial JSON retains the last good map.
 auto old=GP200ModSync::findEffectBankEntry(10,"PRE");auto oldName=old->displayName;auto file=GP200ModSync::getEffectBankFile();CHECK(file.replaceWithText("{broken"));GP200ModSync::reloadIfChanged();CHECK(GP200ModSync::getEffectBankStatus().contains("Invalid"));CHECK(GP200EffectDatabase::getEffectName(10,"PRE")==oldName);
 for(int mode=0;mode<8;++mode){auto bad=juce::JSON::parse(juce::JSON::toString(four));auto* rows=bad.getProperty("slots",{}).getArray();auto& row=(*rows)[0];auto& param=(*row.getProperty("parameters",{}).getArray())[0];switch(mode){case 0:set(bad,"firmware_sha256","bad");break;case 1:rows->add(row);break;case 2:set(row,"destination_module","AMP");break;case 3:set(param,"index",15);break;case 4:set(param,"maximum",-1000);break;case 5:set(param,"step",0);break;case 6:set(row,"source_effect_id",juce::int64(0xdeadbeef));break;case 7:set(bad,"product","GP-200LT");break;}install(bad);CHECK(GP200ModSync::getEffectBankStatus().contains("Invalid"));CHECK(GP200EffectDatabase::getEffectName(10,"PRE")==oldName);}
 // An explicit empty bank restores stock and suppresses stale legacy overlays.
 auto empty=juce::JSON::parse(juce::JSON::toString(four));set(empty,"slots",juce::Array<juce::var>{});install(empty);CHECK(GP200ModSync::hasEffectBank());CHECK(!GP200ModSync::findEffectBankEntry(10,"PRE"));CHECK(GP200EffectDatabase::getEffectName(10,"PRE")=="AC Boost");CHECK(juce::String(old->parameterSet.params[0].name).isNotEmpty());
 // Pair matching: reject use of a bank from another generated BIN.
 auto manifest=juce::JSON::parse(R"({"schema":"gp200-mod-sync/v1","product":"GP-200","firmware":"V182","factory_amp_overrides":[]})");set(manifest,"firmware_sha256",juce::String(std::string(64,'b')));CHECK(GP200ModSync::getManifestFile().replaceWithText(juce::JSON::toString(manifest)));GP200ModSync::reloadIfChanged();install(four);CHECK(!GP200ModSync::hasEffectBank());CHECK(GP200ModSync::getEffectBankStatus().contains("metadata differ"));set(manifest,"firmware_sha256",juce::String(std::string(64,'a')));CHECK(GP200ModSync::getManifestFile().replaceWithText(juce::JSON::toString(manifest)));GP200ModSync::reloadIfChanged();CHECK(GP200ModSync::hasEffectBank());CHECK(GP200EffectDatabase::getEffectName(10,"PRE")==oldName);
 // Device PRE refresh must not overwrite the authoritative generic bank.
 auto pre=juce::JSON::parse(R"({"slots":[{"pre_effect_id":10,"source_effect_id":184549376,"source_module":"DLY","display_name":"WRONG"}]})");CHECK(GP200ModSync::replacePreBankFromDevice(pre));CHECK(GP200EffectDatabase::getEffectName(10,"PRE")==oldName);
 std::cout<<checks<<" generic-bank checks passed (production metadata and databases; JUCE modeled).\n";
}
