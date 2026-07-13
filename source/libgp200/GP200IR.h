#pragma once

#include <JuceHeader.h>
#include <vector>

namespace gp200
{
struct GP200IRUpload
{
    std::vector<juce::MidiMessage> chunks;
    juce::MidiMessage prepareMessage;
    juce::MidiMessage commitMessage;
    juce::String displayName;
};

class GP200IR final
{
public:
    static juce::Result buildUpload (const juce::File& wavFile,
                                     int zeroBasedUserIRSlot,
                                     GP200IRUpload& result);
};
} // namespace gp200
