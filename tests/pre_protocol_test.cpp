#include "../source/libgp200/GP200ModSyncProtocol.h"
#include <fstream>
#include <iterator>
#include <cassert>
#include <iostream>
using namespace gp200::modsync;
std::vector<std::uint8_t> frame(std::vector<std::uint8_t> raw){unsigned sum=0;raw[23]=0;for(auto b:raw)sum+=b;raw[23]=static_cast<std::uint8_t>(-sum);std::vector<std::uint8_t> f{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,40,1,0,0};for(auto b:raw){f.push_back(b>>4);f.push_back(b&15);}f.push_back(0xf7);return f;}
int main(int argc,char** argv){assert(argc==2);int tests=0;for(int page=0;page<23;page++){std::ifstream f(std::string(argv[1])+"/response_"+std::to_string(page)+".bin",std::ios::binary);std::vector<std::uint8_t> raw{std::istreambuf_iterator<char>(f),{}};assert(raw.size()==168);auto wire=frame(raw);Page result;assert(decode(wire.data(),static_cast<int>(wire.size()),page,0x12345678,result)==Decode::valid);tests++;assert(decode(wire.data(),static_cast<int>(wire.size()),page,0x87654321,result)==Decode::invalid);tests++;if(page>=19){for(int idx:{24,32,33,34,35,36}){auto bad=raw;bad[idx]^=0xff;auto b=frame(bad);assert(decode(b.data(),static_cast<int>(b.size()),page,0x12345678,result)==Decode::invalid);tests++;}auto b=wire;b[90]^=1;assert(decode(b.data(),static_cast<int>(b.size()),page,0x12345678,result)==Decode::invalid);tests++;}}
std::cout<<tests<<" checks of actual Thumb responses with production decoder passed: legacy CAB/AMP, PRE pages, malformed records, nonce and checksums.\n";
}
