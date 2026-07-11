#pragma once

#include <JuceHeader.h>

namespace gp200
{
struct MidiDeviceList
{
    juce::StringArray inputs;
    juce::StringArray outputs;
};

class MidiDeviceScanner
{
  public:
    static MidiDeviceList scan ();

    static bool looksLikeGP200 (const juce::String& deviceName);
    static bool looksLikeGP200 (const juce::MidiDeviceInfo& device);

    static bool findFirstGP200Input (juce::MidiDeviceInfo& result);
    static bool findFirstGP200Output (juce::MidiDeviceInfo& result);
};
} // namespace gp200
