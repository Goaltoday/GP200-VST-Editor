#pragma once
#include <array>
#include <cstdint>
namespace gp200 {
// Read-only scene event, in the native type-5 notification envelope. No request or parameter setter uses P15.
inline int sceneChangeSlot(const std::uint8_t* data, int size) {
 if (!data || size!=46) return -1;
 constexpr std::array<std::uint8_t,13> header{0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,16,0,0,0};
 for(int i=0;i<13;++i)if(data[i]!=header[static_cast<std::size_t>(i)])return -1;
 if(data[45]!=0xf7)return -1;
 std::array<std::uint8_t,16> p{};
 for(int i=0;i<16;++i){if(data[13+2*i]>15 || data[14+2*i]>15)return -1;p[static_cast<std::size_t>(i)]=static_cast<std::uint8_t>((data[13+2*i]<<4)|data[14+2*i]);}
 if(p[0]!=5 || p[1]!=0 || p[2]!=12 || p[3]!=0 || p[4]!=10 || p[5]!=15 || p[6]!=0 || p[7]!=0)return -1;
 for(int i=8;i<12;++i)if(p[static_cast<std::size_t>(i)]!=0)return -1;
 if(p[14]!=0x43 || p[15]!=0x53 || p[12]>3)return -1;
 return p[13];
}
}
