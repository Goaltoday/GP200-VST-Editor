#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class TunerDisplayComponent final : public juce::Component
{
public:
    TunerDisplayComponent() = default;

    void paint(juce::Graphics& g) override;

    void setTunerReading(const juce::String& noteName,
                         float frequencyHz,
                         float cents,
                         bool valid);

    void clearReading();

private:
    juce::String noteText{"--"};
    float frequency{0.0f};
    float centsOffset{0.0f};
    bool hasValidReading{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerDisplayComponent)
};