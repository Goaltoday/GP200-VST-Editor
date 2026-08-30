#pragma once

#include <JuceHeader.h>
#include <cstdint>

namespace gp200
{
class GP200ModSync final
{
  public:
    static void reloadIfChanged ();
    static std::uint64_t getRevision ();

    static bool isCustomCloAmp (juce::uint32 effectId);
    static juce::String getDisplayName (juce::uint32 effectId);
    static juce::String getSourceFile (juce::uint32 effectId);
    static juce::String getDescription (juce::uint32 effectId);

    static void recordFactoryAmpOverride (juce::uint32 effectId,
                                          const juce::String& displayName,
                                          const juce::String& sourceFile);
    static void recordFactoryCabOverride (juce::uint32 effectId,
                                          const juce::String& displayName,
                                          const juce::String& sourceFile);

    static juce::File getManifestFile ();
};
} // namespace gp200
