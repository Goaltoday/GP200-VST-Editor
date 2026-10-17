#include "GP200NAM.h"
#include "GP200ModSyncProtocol.h"
#include <cmath>
#include <cstring>
namespace gp200 { namespace {
const char* configJSON=R"NAMCFG({"layers":[{"input_size":1,"condition_size":1,"head":{"out_channels":1,"kernel_size":16,"bias":true},"channels":3,"kernel_sizes":[6,6,6,6,6,6,6,6,6,6,6,6,6,6,15,15,6,6,6,6,6,6,6],"dilations":[1,3,7,17,41,101,239,1,3,7,17,41,101,239,1,13,1,3,7,17,41,101,239],"activation":[{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01},{"type":"LeakyReLU","negative_slope":0.01}],"bottleneck":3,"head1x1":{"active":false,"out_channels":1,"groups":1},"layer1x1":{"active":true,"groups":1},"groups_input":1,"groups_input_mixin":1,"conv_pre_film":{"active":false,"shift":true,"groups":1},"conv_post_film":{"active":false,"shift":true,"groups":1},"input_mixin_pre_film":{"active":false,"shift":true,"groups":1},"input_mixin_post_film":{"active":false,"shift":true,"groups":1},"activation_pre_film":{"active":false,"shift":true,"groups":1},"activation_post_film":{"active":false,"shift":true,"groups":1},"layer1x1_post_film":{"active":false,"shift":true,"groups":1},"head1x1_post_film":{"active":false,"shift":true,"groups":1},"gating_mode":["none","none","none","none","none","none","none","none","none","none","none","none","none","none","none","none","none","none","none","none","none","none","none"],"secondary_activation":[null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null],"slimmable":null}],"head":null})NAMCFG";
bool numeric(const juce::var& v){return v.isInt()||v.isInt64()||v.isDouble();}
bool same(const juce::var&a,const juce::var&b,bool config=false){
 if(auto obj=b.getDynamicObject()){
  auto actual=a.getDynamicObject();if(!actual)return false;
  const auto& expected=obj->getProperties();
  if(actual->getProperties().size()!=expected.size()+(config?1:0))return false;
  for(int i=0;i<expected.size();++i){const auto key=expected.getName(i);if(!actual->hasProperty(key)||!same(a.getProperty(key,{}),b.getProperty(key,{})))return false;}return true;
 }
 if(auto list=b.getArray()){auto actual=a.getArray();if(!actual||actual->size()!=list->size())return false;for(int i=0;i<list->size();++i)if(!same((*actual)[i],(*list)[i]))return false;return true;}
 if(b.isVoid())return a.isVoid();
 if(b.isBool())return a.isBool()&&a.toString()==b.toString();
 if(b.isString())return a.isString()&&a.toString()==b.toString();
 return numeric(a)&&numeric(b)&&static_cast<double>(a)==static_cast<double>(b);
}
bool lite(const juce::var&model,std::array<float,1871>&out){
 static const auto expected=juce::JSON::parse(configJSON);
 if(model.getProperty("architecture",{}).toString()!="WaveNet")return false;
 const auto sr=model.getProperty("sample_rate",{});if(!numeric(sr)||static_cast<double>(sr)!=48000)return false;
 const auto cfg=model.getProperty("config",{});if(!same(cfg,expected,true))return false;
 const auto scale=cfg.getProperty("head_scale",{});if(!numeric(scale))return false;
 const float hs=static_cast<float>(static_cast<double>(scale));if(!std::isfinite(hs)||hs<=0)return false;
 const auto weights=model.getProperty("weights",{});const auto a=weights.getArray();if(!a||a->size()!=1871)return false;
 for(int i=0;i<1871;++i){if(!numeric((*a)[i]))return false;out[static_cast<std::size_t>(i)]=static_cast<float>(static_cast<double>((*a)[i]));if(!std::isfinite(out[static_cast<std::size_t>(i)]))return false;}
 return out[1870]>0 && std::abs(hs-out[1870])<=1e-6f*std::max(std::abs(hs),std::abs(out[1870]));
}
unsigned crc(const std::vector<juce::uint8>&b){unsigned c=~0u;for(auto v:b){c^=v;for(int i=0;i<8;++i)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
juce::MidiMessage midi(const std::vector<juce::uint8>&v){return juce::MidiMessage::createSysExMessage(v.data()+1,static_cast<int>(v.size())-2);}
GP200IRUpload stage(const std::vector<juce::uint8>&blob){
 GP200IRUpload result;result.displayName="NAM HOT";
 result.prepareMessage=midi({0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,0x14,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,8,11,0,0,1,6,0,0,0,8,0,0,0,5,15,4,8,15,0,0,0,0,0,0,1,0,0,10,0xf7});
 // HQ/HN executes on the final data packet; no separate commit is sent.
 const int n=static_cast<int>(blob.size());for(int offset=0;offset<n;offset+=183){std::vector<juce::uint8> full{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,static_cast<juce::uint8>(n&127),static_cast<juce::uint8>(n>>7),static_cast<juce::uint8>(offset&127),static_cast<juce::uint8>(offset>>7)};for(int i=offset;i<std::min(n,offset+183);++i){full.push_back(blob[static_cast<std::size_t>(i)]>>4);full.push_back(blob[static_cast<std::size_t>(i)]&15);}full.push_back(0xf7);result.chunks.push_back(midi(full));}return result;
}
std::vector<juce::uint8> wrapper(int index,unsigned nonce,unsigned command){std::vector<juce::uint8>b(28);modsync::write32(b.data(),0x2018100a);modsync::write16(b.data()+4,nonce);modsync::write16(b.data()+6,command);b[10]=static_cast<juce::uint8>(index);b[11]=0xb1;return b;}
} // namespace
juce::Result GP200NAM::readWeights(const juce::File&file,std::vector<juce::uint8>&bytes){
 bytes.clear();if(!file.existsAsFile()||file.getSize()>8*1024*1024)return juce::Result::fail("NAM missing or larger than 8 MiB.");
 const auto model=juce::JSON::parse(file.loadFileAsString());std::array<float,1871>w{};bool ok=false;
 if(model.getProperty("architecture",{}).toString()=="SlimmableContainer"){
  const auto cfg=model.getProperty("config",{});const auto children=cfg.getProperty("submodels",{});if(auto a=children.getArray()){int count=0;for(const auto& child:*a){std::array<float,1871> candidate{};if(lite(child.getProperty("model",{}),candidate)){w=candidate;++count;}}ok=count==1;}
 }else ok=lite(model,w);
 if(!ok)return juce::Result::fail("Expected exactly one compatible WaveNet A2 Lite at 48 kHz, 1871 finite weights and matching head_scale.");
 std::vector<float> ordered(w.begin(),w.begin()+3);int at=3;
 for(int layer=0;layer<23;++layer){const int k=(layer==14||layer==15)?15:6;for(int t=0;t<k;++t)for(int i=0;i<3;++i)for(int o=0;o<3;++o)ordered.push_back(w[static_cast<std::size_t>(at+(o*3+i)*k+t)]);at+=9*k;ordered.insert(ordered.end(),w.begin()+at,w.begin()+at+6);for(int i=0;i<3;++i)for(int o=0;o<3;++o)ordered.push_back(w[static_cast<std::size_t>(at+6+o*3+i)]);ordered.insert(ordered.end(),w.begin()+at+15,w.begin()+at+18);at+=18;}
 for(int t=0;t<16;++t) { for(int i=0;i<3;++i) ordered.push_back(w[static_cast<std::size_t>(at+i*16+t)]); }
 ordered.insert(ordered.end(),w.begin()+at+48,w.end());
 if(ordered.size()!=1871)return juce::Result::fail("NAM weight layout error.");
 bytes.resize(weightBytes);for(std::size_t i=0;i<ordered.size();++i){unsigned bits=0;static_assert(sizeof(float)==4);std::memcpy(&bits,&ordered[i],4);modsync::write32(bytes.data()+i*4,bits);}return juce::Result::ok();
}
GP200IRUpload GP200NAM::query(int index,unsigned nonce){return stage(wrapper(index,nonce,0x5148));}
GP200IRUpload GP200NAM::upload(const std::vector<juce::uint8>&weights,int index,unsigned nonce){auto b=wrapper(index,nonce,0x4e48);if(weights.size()!=weightBytes||index<0||index>=71||nonce==0)return {};modsync::write32(b.data()+12,crc(weights));modsync::write32(b.data()+16,binding);modsync::write32(b.data()+20,modsync::ampIds[static_cast<std::size_t>(index)]);modsync::write32(b.data()+24,weightBytes);b.insert(b.end(),weights.begin(),weights.end());return stage(b);}
bool GP200NAM::decodeAck(const juce::uint8*data,int n,Ack&out){
 if(n<38||data[0]!=0xf0||data[8]!=0x12)return false;
 std::array<juce::uint8,28> b{};for(int i=0;i<12;++i){if(data[13+2*i]>15||data[14+2*i]>15)return false;b[static_cast<std::size_t>(i)]=static_cast<juce::uint8>((data[13+2*i]<<4)|data[14+2*i]);}
 if(modsync::read16(b.data())!=0x104b||modsync::read32(b.data()+8)!=ackMagic)return false;
 out={};if(n!=70||data[9]!=28||data[10]||data[11]||data[12]||data[n-1]!=0xf7)return true;
 for(int i=12;i<28;++i){if(data[13+2*i]>15||data[14+2*i]>15)return true;b[static_cast<std::size_t>(i)]=static_cast<juce::uint8>((data[13+2*i]<<4)|data[14+2*i]);}
 if(modsync::read16(b.data()+2)!=24||modsync::read32(b.data()+12)!=binding||modsync::read32(b.data()+24)!=weightBytes)return true;
 out.nonce=modsync::read16(b.data()+4);out.status=modsync::read16(b.data()+6);out.id=modsync::read32(b.data()+16);out.physical=modsync::read32(b.data()+20);return true;
}
} // namespace gp200
