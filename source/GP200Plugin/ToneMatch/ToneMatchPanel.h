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

    void setCloseCallback (std::function<void()> callback)
    {
        closeRequested = std::move (callback);
    }

  private:
    void timerCallback() override;

    void handleSourceCapture();
    void handleTargetCapture();
    void startCapture (tonematch::CaptureRole role);
    void stopCapture();

    void analyseCaptures();
    void generateIR();
    void saveIR();

    void updateControls();
    void updateCaptureLabels();

    static juce::String formatDuration (double seconds);
    static juce::String formatPeak (double peakDb);

    AudioPluginAudioProcessor& processorRef;

    ToneMatchCurveComponent matchCurveComponent;

    juce::TextButton sourceCaptureButton{"Capture Source"};
    juce::TextButton sourceClearButton{"Clear"};

    juce::TextButton targetCaptureButton{"Capture Target"};
    juce::TextButton targetClearButton{"Clear"};

    juce::TextButton analyseButton{"Analyse"};
    juce::TextButton generateIRButton{"Generate IR"};
    juce::TextButton saveIRButton{"Save IR WAV"};
    juce::TextButton closeButton{"Close"};

    juce::Label sourceStatusLabel;
    juce::Label sourceDetailsLabel;
    juce::Label targetStatusLabel;
    juce::Label targetDetailsLabel;
    juce::Label globalStatusLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::function<void()> closeRequested;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneMatchPanel)
};
