#pragma once

#include <JuceHeader.h>
#include "GP200IR.h"
#include <vector>

namespace gp200
{
struct GP200SoundCloneUpload
{
    std::vector<juce::MidiMessage> chunks;
    juce::MidiMessage prepareMessage;
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

    // Custom-firmware path: Factory AMP 1..71. The compact 0x1288 CLO is
    // stored in the persistent former-DRUM pool and the firmware activates
    // the matching package-local callback in RAM.
    static juce::Result buildFactoryAmpUpload (const juce::File& cloFile,
                                               int zeroBasedFactoryAmpIndex,
                                               GP200IRUpload& result);
};
} // namespace gp200
