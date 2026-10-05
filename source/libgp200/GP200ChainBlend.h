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
inline float blendTagFloat() { return std::bit_cast<float>(chainBlendTag); }
}
