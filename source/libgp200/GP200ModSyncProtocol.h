#pragma once
// STEP11 wire codec. No JUCE dependency: shared by the parser's native tests.
#include <array>
#include <cstdint>
#include <vector>
#include <cstddef>
namespace gp200 { namespace modsync {
constexpr int pageCount = 19;
constexpr int responseBytes = 168;
constexpr std::array<std::uint32_t,71> ampIds {{0x07000001u,0x07000003u,0x07000004u,0x07000005u,0x07000009u,0x0700000du,0x0700000fu,0x07000010u,0x07000011u,0x07000014u,0x07000015u,0x07000019u,0x0700001au,0x0700001bu,0x0700001fu,0x07000022u,0x07000023u,0x07000024u,0x07000027u,0x07000028u,0x0700002au,0x0700002bu,0x0700002cu,0x0700002du,0x0700002eu,0x0700002fu,0x07000030u,0x07000035u,0x07000039u,0x0700003au,0x0700003bu,0x0700003du,0x0700003eu,0x07000040u,0x07000041u,0x07000043u,0x07000044u,0x07000047u,0x07000048u,0x07000049u,0x0700004au,0x0700004bu,0x0700004eu,0x07000053u,0x07000055u,0x07000056u,0x07000057u,0x07000059u,0x0700005au,0x0700005du,0x0700005eu,0x0700005fu,0x07000060u,0x07000063u,0x07000065u,0x07000066u,0x07000068u,0x07000069u,0x0700006au,0x0700006bu,0x0700006du,0x0700006eu,0x07000073u,0x07000075u,0x07000077u,0x0700007bu,0x0700007cu,0x08000075u,0x08000076u,0x0800007au,0x0800007bu}};
inline unsigned read16(const std::uint8_t* p) { return unsigned(p[0]) | (unsigned(p[1])<<8); }
inline std::uint32_t read32(const std::uint8_t* p) { return read16(p) | (std::uint32_t(read16(p+2))<<16); }
inline void write16(std::uint8_t* p,unsigned v) { p[0]=std::uint8_t(v); p[1]=std::uint8_t(v>>8); }
inline void write32(std::uint8_t* p,std::uint32_t v) { write16(p,v); write16(p+2,v>>16); }
inline std::vector<std::uint8_t> request(int page, std::uint32_t nonce) {
 std::array<std::uint8_t,28> raw{};
 write16(raw.data(),0x1009); write16(raw.data()+2,24);
 write16(raw.data()+4,0x4d53); write16(raw.data()+6,unsigned(page));
 write32(raw.data()+8,0x3131534d); write32(raw.data()+12,nonce); raw[16]=1;
 std::vector<std::uint8_t> out{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x11,28,0,0,0};
 for(auto b:raw) {out.push_back(b>>4);out.push_back(b&15);} out.push_back(0xf7);return out;
}
struct Page { std::array<std::uint8_t,responseBytes> bytes{}; };
enum class Decode { unrelated, invalid, valid };
inline Decode decode(const std::uint8_t* data,int size,int expectedPage,std::uint32_t nonce,Page& out) {
 // Check our reserved query index and signature before claiming a response.
 if(size<46 || data[0]!=0xf0 || data[8]!=0x12) return Decode::unrelated;
 const std::uint8_t header[]{0x21,0x25,0x7e,0x47,0x50,0x2d,0x32};
 for(int i=0;i<7;++i) if(data[i+1]!=header[i]) return Decode::unrelated;
 std::array<std::uint8_t,16> prefix{};
 for(int i=0;i<16;++i) { if(data[13+i*2]>15||data[14+i*2]>15)return Decode::unrelated;prefix[i]=std::uint8_t((data[13+i*2]<<4)|data[14+i*2]); }
 if(read16(prefix.data()+4)!=0x4d53 || read32(prefix.data()+8)!=0x3131534d) return Decode::unrelated;
 if(size!=14+2*responseBytes || data[9]!=(responseBytes&127)||data[10]!=(responseBytes>>7)||data[11]!=0||data[12]!=0||data[size-1]!=0xf7) return Decode::invalid;
 unsigned checksum=0;
 for(int i=0;i<responseBytes;++i) {if(data[13+i*2]>15||data[14+i*2]>15)return Decode::invalid;out.bytes[i]=std::uint8_t((data[13+i*2]<<4)|data[14+i*2]);checksum+=out.bytes[i];}
 auto p=out.bytes.data();
 if((checksum&255)!=0 || read16(p)!=0x1009 || read16(p+2)!=164 || read16(p+6)!=unsigned(expectedPage) || read32(p+12)!=nonce || p[16]!=1 || p[17]!=0 || (p[19]&~7)!=0 || p[20]!=70 || p[21]!=71 || p[22]!=1) return Decode::invalid;
 int count=expectedPage==0?0:expectedPage==9?6:expectedPage==18?7:8;
 if(p[18]!=count)return Decode::invalid;
 for(int i=0;i<8;++i) {
  const auto rec=p+24+i*18;
  if(i>=count) {for(int k=0;k<18;++k)if(rec[k]!=0)return Decode::invalid;continue;}
  bool amp=expectedPage>=10;
  if(!amp && (rec[0]!=0||rec[1]!=0))return Decode::invalid;
  if(amp && !((rec[0]==0&&rec[1]==0)||(rec[0]==1&&rec[1]==5)))return Decode::invalid;
  if(amp && rec[0]==1 && !(p[19]&1))return Decode::invalid;
  for(int k=2;k<18;++k)if(rec[k]!=0 && (rec[k]<32||rec[k]>126))return Decode::invalid;
  if(!amp && rec[2]==0)return Decode::invalid;
 }
 return Decode::valid;
}
}} // namespace
