#pragma once

#include "GP200Constants.h"

#include <JuceHeader.h>
#include <array>

namespace gp200
{
using EffectParameters = std::array<float, effectParamCount>;
using RoutingOrder = std::array<int, effectBlockCount>;

struct GP200EffectSlot
{
    // blockIndex identifies the protocol block (PRE, WAH, DST, ...).
    // slotIndex is retained because it is present in the preset dump.
    int blockIndex{-1};
    int slotIndex{-1};
    bool enabled{false};
    juce::uint32 effectId{0};
    EffectParameters params{};

    juce::String getSummaryLine () const;
    juce::StringArray getDetailedLines () const;
};

struct GP200Preset
{
    bool isValid{false};

    juce::String patchName{"unknown"};
    juce::String author;

    int fxLoopSend{4};
    int fxLoopReturn{4};

    RoutingOrder routingOrder{};
    std::array<GP200EffectSlot, effectBlockCount> effects{};

    juce::String getSignalChainText () const;
    juce::StringArray getEffectSummaryLines () const;

    const GP200EffectSlot* findEffectBySlotIndex (int slotIndex) const;
};

class GP200PresetCodec
{
  public:
    static GP200Preset decodeLivePresetDump (
        const juce::MemoryBlock& presetData);

    static GP200Preset decodePrstFile (
        const juce::MemoryBlock& fileData);

    static juce::MemoryBlock makeLivePresetDumpFromPrst (
        const GP200Preset& preset,
        const juce::MemoryBlock& livePresetTemplate);

    static juce::String blockNameForSlotIndex (int slotIndex);
    static juce::String effectNameForId (juce::uint32 effectId);
    static juce::String effectIdToHex (juce::uint32 effectId);

  private:
    static juce::String readAsciiString (
        const juce::uint8* data,
        std::size_t dataSize,
        std::size_t offset,
        std::size_t maxLength);

    static juce::uint32 readUInt32LE (
        const juce::uint8* data,
        std::size_t dataSize,
        std::size_t offset);

    static float readFloat32LE (
        const juce::uint8* data,
        std::size_t dataSize,
        std::size_t offset);

    static void writeAsciiString (
        juce::uint8* data,
        std::size_t dataSize,
        std::size_t offset,
        std::size_t maxLength,
        const juce::String& text);

    static void writeUInt32LE (
        juce::uint8* data,
        std::size_t dataSize,
        std::size_t offset,
        juce::uint32 value);

    static void writeFloat32LE (
        juce::uint8* data,
        std::size_t dataSize,
        std::size_t offset,
        float value);
};


static GP200Preset decodePrstFile (
    const juce::MemoryBlock& fileData);

static juce::MemoryBlock makeLivePresetDumpFromPrst (
    const GP200Preset& preset,
    const juce::MemoryBlock& livePresetTemplate);

} // namespace gp200
