#pragma once

#include <JuceHeader.h>
#include <vector>

namespace gp200
{
struct GP200EffectInfo
{
    juce::uint32 effectId{0};
    const char* name{"Unknown"};
    const char* module{"Unknown"};
};

class GP200EffectDatabase
{
  public:
    static const GP200EffectInfo* findEffect (juce::uint32 effectId);
    static juce::String getEffectName (juce::uint32 effectId);
    static juce::String getEffectDescription (juce::uint32 effectId);
    static juce::String getModuleName (juce::uint32 effectId);
    static std::vector<GP200EffectInfo> getEffectsForModule (const juce::String& moduleName);
    static juce::String effectIdToHex (juce::uint32 effectId);
};
} // namespace gp200
