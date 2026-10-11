#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <iterator>
namespace gp200 {
inline constexpr std::uint32_t chainBlendTag = 0x4b435031;
inline bool routingModeIsChain(int mode) { return mode >= 0x80 && mode <= 0x8b; }
inline bool validBlendMetadata(float tag, float value) {
 return std::bit_cast<std::uint32_t>(tag)==chainBlendTag && std::isfinite(value) && value>=0.0f && value<=100.0f;
}
template<class Preset> bool hasIndependentBlend(const Preset& p) {
 return p.isValid && validBlendMetadata(p.effects[10].params[13],p.effects[10].params[14]);
}
template<class Preset> bool legacyVolumeIsBlend(const Preset& p) {
 const auto it=std::find(p.routingOrder.begin(),p.routingOrder.end(),10);
 return p.effects[10].enabled && it!=p.routingOrder.end() && std::distance(p.routingOrder.begin(),it)>=p.fxLoopReturn;
}
inline constexpr std::uint32_t chainAlignTag = 0x414c0000;
inline bool validAlignWord(std::uint32_t word) { return (word & 0xffffff00u)==chainAlignTag && (word & 127u)<=64; }
inline std::uint32_t alignWord(int frames, bool inverted) { return chainAlignTag | static_cast<std::uint32_t>(std::clamp(frames,-32,32)+32) | (inverted?128u:0u); }
inline int alignFrames(std::uint32_t word) { return validAlignWord(word)?static_cast<int>(word&127u)-32:0; }
inline bool alignInverted(std::uint32_t word) { return validAlignWord(word) && (word&128u)!=0; }
inline float blendTagFloat() { return std::bit_cast<float>(chainBlendTag); }
}
