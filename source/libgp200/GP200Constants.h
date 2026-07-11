#pragma once

#include <cstddef>

namespace gp200
{
inline constexpr std::size_t effectBlockCount = 11;
inline constexpr std::size_t effectParamCount = 15;
inline constexpr std::size_t userIRCount = 20;
inline constexpr std::size_t snapToneCount = 10;
inline constexpr int presetsPerBank = 4;
inline constexpr int presetNameMaxLength = 16;

inline constexpr std::size_t presetNameOffset = 28;
inline constexpr std::size_t authorOffset = 44;
inline constexpr std::size_t patchVolumeOffset = 16;
inline constexpr std::size_t patchTempoOffset = 14;
inline constexpr std::size_t fxLoopSendOffset = 106;
inline constexpr std::size_t fxLoopReturnOffset = 107;
inline constexpr std::size_t routingOrderOffset = 108;
inline constexpr std::size_t effectBlockStart = 120;
inline constexpr std::size_t effectBlockSize = 72;

inline constexpr std::size_t slotIndexOffset = 4;
inline constexpr std::size_t enabledOffset = 5;
inline constexpr std::size_t effectIdOffset = 8;
inline constexpr std::size_t paramsOffset = 12;
} // namespace gp200
