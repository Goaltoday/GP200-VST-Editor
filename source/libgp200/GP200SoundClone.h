#pragma once

#include <JuceHeader.h>
#include <vector>

namespace gp200
{
struct GP200SoundCloneUpload
{
    std::vector<juce::MidiMessage> chunks;
    juce::MidiMessage prepareMessage;
    juce::MidiMessage commitMessage;
    juce::String displayName;
    int globalSlot{-1};
};

class GP200SoundClone final
{
public:
    // Global slots: AMP 1..5 = 0..4, DIST 1..5 = 5..9.
    static juce::Result buildUpload (const juce::File& cloFile,
                                     int globalSlot,
                                     GP200SoundCloneUpload& result);
};
} // namespace gp200
