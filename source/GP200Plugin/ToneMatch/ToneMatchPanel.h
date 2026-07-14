#pragma once

#include "../PluginProcessor.h"
#include "ToneMatchCurveComponent.h"

class ToneMatchPanel final : public juce::Component,
                             private juce::Timer
{
  public:
    explicit ToneMatchPanel (AudioPluginAudioProcessor& processor);
    ~ToneMatchPanel() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

  private:
  ToneMatchCurveComponent matchCurveComponent;
    void timerCallback() override;

    void handleSourceCapture();
    void handleTargetCapture();

    void startCapture (tonematch::CaptureRole role);
    void stopCapture();

    void updateControls();
    void updateCaptureLabels();

    static juce::String formatDuration (double seconds);
    static juce::String formatPeak (double peakDb);

    AudioPluginAudioProcessor& processorRef;

    juce::TextButton sourceCaptureButton{"Capture Source"};
    juce::TextButton sourceClearButton{"Clear"};

    juce::TextButton targetCaptureButton{"Capture GP-200"};
    juce::TextButton targetClearButton{"Clear"};

    juce::TextButton analyseButton{"Analyse"};
    juce::TextButton closeButton{"Close"};

    juce::Label sourceStatusLabel;
    juce::Label sourceDetailsLabel;

    juce::Label targetStatusLabel;
    juce::Label targetDetailsLabel;

    juce::Label globalStatusLabel;

    std::function<void()> closeRequested;

  public:
    void setCloseCallback (std::function<void()> callback)
    {
        closeRequested = std::move (callback);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneMatchPanel)
};