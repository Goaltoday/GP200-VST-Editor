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

    // Custom-firmware path: Factory CAB 1..70, always 1024 samples.
    // Uses the same staged SysEx transport as User IR, but marker 0xCAFE
    // is intercepted by the unified firmware before the User IR writer.
    static juce::Result buildFactoryCabUpload (const juce::File& wavFile,
                                               int zeroBasedFactoryCabIndex,
                                               GP200IRUpload& result);
};
} // namespace gp200
