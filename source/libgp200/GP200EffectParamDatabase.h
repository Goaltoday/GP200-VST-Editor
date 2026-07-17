#pragma once

#include "GP200Constants.h"

#include <JuceHeader.h>
#include <array>

namespace gp200
{
enum class GP200ParamKind
{
    continuous,
    toggle,
    choice
};

struct GP200EffectParamInfo
{
    int idx{-1};
    const char* name{""};
    GP200ParamKind kind{GP200ParamKind::continuous};
    float defaultValue{0.0f};
    float minValue{0.0f};
    float maxValue{0.0f};
    float step{1.0f};
};

struct GP200EffectParamSet
{
    juce::uint32 effectId{0};
    const GP200EffectParamInfo* params{nullptr};
    int count{0};
};

class GP200EffectParamDatabase
{
  public:
    static const GP200EffectParamSet* findParamsForEffect (juce::uint32 effectId);
    static const GP200EffectParamInfo* findParam (juce::uint32 effectId, int paramIndex);

    static juce::String getParamName (juce::uint32 effectId, int paramIndex);

    // Optional display metadata for discrete parameters. The numeric values
    // used by presets and MIDI remain unchanged.
    static bool hasDiscreteOptions (juce::uint32 effectId, int paramIndex);
    static float getDiscreteOptionMinimum (juce::uint32 effectId, int paramIndex);
    static float getDiscreteOptionMaximum (juce::uint32 effectId, int paramIndex);
    static juce::String getDiscreteOptionLabel (juce::uint32 effectId,
                                                int paramIndex,
                                                float value);

    static juce::String formatKnownParams (juce::uint32 effectId,
                                           const std::array<float, effectParamCount>& values,
                                           int maxParamsToShow = 5);
};
} // namespace gp200
