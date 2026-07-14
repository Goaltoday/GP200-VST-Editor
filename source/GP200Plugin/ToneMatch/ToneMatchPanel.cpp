#include "ToneMatchPanel.h"

namespace
{
const juce::Colour backgroundColour{0xff181d20};
const juce::Colour panelColour{0xff252b2f};
const juce::Colour outlineColour{0xffffa42a};
const juce::Colour textColour{0xffffffff};
const juce::Colour mutedTextColour{0xffb8b8b8};
const juce::Colour readyColour{0xff57f05f};
const juce::Colour errorColour{0xffd84545};
}

ToneMatchPanel::ToneMatchPanel (
    AudioPluginAudioProcessor& processor)
    : processorRef (processor)
{
    addAndMakeVisible (sourceCaptureButton);
    addAndMakeVisible (sourceClearButton);

    addAndMakeVisible (targetCaptureButton);
    addAndMakeVisible (targetClearButton);

    addAndMakeVisible (analyseButton);
    addAndMakeVisible (closeButton);

    addAndMakeVisible (sourceStatusLabel);
    addAndMakeVisible (sourceDetailsLabel);

    addAndMakeVisible (targetStatusLabel);
    addAndMakeVisible (targetDetailsLabel);

    addAndMakeVisible (globalStatusLabel);
	addAndMakeVisible (matchCurveComponent);

    auto setupButton = [] (juce::TextButton& button)
    {
        button.setColour (
            juce::TextButton::buttonColourId,
            panelColour);

        button.setColour (
            juce::TextButton::buttonOnColourId,
            panelColour.brighter (0.15f));

        button.setColour (
            juce::TextButton::textColourOffId,
            outlineColour);

        button.setColour (
            juce::TextButton::textColourOnId,
            outlineColour.brighter (0.15f));
    };

    setupButton (sourceCaptureButton);
    setupButton (sourceClearButton);
    setupButton (targetCaptureButton);
    setupButton (targetClearButton);
    setupButton (analyseButton);
    setupButton (closeButton);

    sourceStatusLabel.setColour (
        juce::Label::textColourId,
        textColour);

    sourceDetailsLabel.setColour (
        juce::Label::textColourId,
        mutedTextColour);

    targetStatusLabel.setColour (
        juce::Label::textColourId,
        textColour);

    targetDetailsLabel.setColour (
        juce::Label::textColourId,
        mutedTextColour);

    globalStatusLabel.setColour (
        juce::Label::textColourId,
        mutedTextColour);

    sourceStatusLabel.setJustificationType (
        juce::Justification::centredLeft);

    sourceDetailsLabel.setJustificationType (
        juce::Justification::centredLeft);

    targetStatusLabel.setJustificationType (
        juce::Justification::centredLeft);

    targetDetailsLabel.setJustificationType (
        juce::Justification::centredLeft);

    globalStatusLabel.setJustificationType (
        juce::Justification::centred);

    sourceCaptureButton.onClick =
        [this]
        {
            handleSourceCapture();
        };

    targetCaptureButton.onClick =
        [this]
        {
            handleTargetCapture();
        };

    sourceClearButton.onClick =
        [this]
        {
            if (processorRef.isToneMatchCapturing())
                return;

            processorRef.clearToneMatchCapture (
    tonematch::CaptureRole::source);

processorRef.clearToneMatchComparison();
matchCurveComponent.clear();

globalStatusLabel.setText (
    "SOURCE capture cleared",
    juce::dontSendNotification);

updateControls();
        };

    targetClearButton.onClick =
        [this]
        {
            if (processorRef.isToneMatchCapturing())
                return;

            processorRef.clearToneMatchCapture (
    tonematch::CaptureRole::target);

processorRef.clearToneMatchComparison();
matchCurveComponent.clear();

globalStatusLabel.setText (
    "GP-200 capture cleared",
    juce::dontSendNotification);

updateControls();
        };

    analyseButton.onClick =
    [this]
    {
        analyseButton.setEnabled (false);
		
		  processorRef.clearToneMatchComparison();
        matchCurveComponent.clear();

        globalStatusLabel.setText (
            "Analysing SOURCE...",
            juce::dontSendNotification);

        const auto sourceAnalysed =
            processorRef.analyseToneMatchCapture (
                tonematch::CaptureRole::source);

        if (!sourceAnalysed)
        {
            globalStatusLabel.setText (
                "SOURCE analysis failed",
                juce::dontSendNotification);

            updateControls();
            return;
        }

        globalStatusLabel.setText (
            "Analysing GP-200...",
            juce::dontSendNotification);

        const auto targetAnalysed =
            processorRef.analyseToneMatchCapture (
                tonematch::CaptureRole::target);

        if (!targetAnalysed)
        {
            globalStatusLabel.setText (
                "GP-200 analysis failed",
                juce::dontSendNotification);

            updateControls();
            return;
        }

        globalStatusLabel.setText (
            "Calculating TARGET - SOURCE...",
            juce::dontSendNotification);

        if (!processorRef.compareToneMatchProfiles())
        {
            globalStatusLabel.setText (
                "Spectrum comparison failed",
                juce::dontSendNotification);

            updateControls();
            return;
        }

        const auto comparison =
            processorRef.getToneMatchComparisonCopy();
			matchCurveComponent.setComparison (
    comparison);

        const auto minimumText =
            juce::String (
                comparison.minimumCorrectionDb,
                1);

        const auto maximumText =
            (comparison.maximumCorrectionDb >= 0.0
                 ? "+"
                 : "")
            + juce::String (
                comparison.maximumCorrectionDb,
                1);

        const auto offsetText =
            (comparison.removedLevelOffsetDb >= 0.0
                 ? "+"
                 : "")
            + juce::String (
                comparison.removedLevelOffsetDb,
                1);

        globalStatusLabel.setText (
            "Comparison ready | "
                + minimumText
                + " to "
                + maximumText
                + " dB | level offset "
                + offsetText
                + " dB",
            juce::dontSendNotification);

        updateControls();
        repaint();
    };

    closeButton.onClick =
        [this]
        {
            if (processorRef.isToneMatchCapturing())
                stopCapture();

            if (closeRequested)
                closeRequested();
        };

    startTimerHz (20);
    updateControls();
}

void ToneMatchPanel::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    g.setColour (outlineColour);
    g.drawRoundedRectangle (
        getLocalBounds().toFloat().reduced (0.5f),
        8.0f,
        1.0f);

    g.setColour (textColour);
    g.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));

    g.drawText (
        "TONE MATCH",
        20,
        12,
        getWidth() - 40,
        32,
        juce::Justification::centredLeft);

    g.setColour (mutedTextColour);
    g.setFont (juce::FontOptions (13.0f));

    g.drawText (
        "Capture the reference and the converted GP-200 signal separately.",
        20,
        44,
        getWidth() - 40,
        24,
        juce::Justification::centredLeft);

    const auto sourceBox =
        juce::Rectangle<int> (
            20,
            82,
            getWidth() - 40,
            118);

    const auto targetBox =
        juce::Rectangle<int> (
            20,
            214,
            getWidth() - 40,
            118);

    g.setColour (panelColour);
    g.fillRoundedRectangle (sourceBox.toFloat(), 6.0f);
    g.fillRoundedRectangle (targetBox.toFloat(), 6.0f);

    g.setColour (outlineColour.withAlpha (0.7f));
    g.drawRoundedRectangle (
        sourceBox.toFloat().reduced (0.5f),
        6.0f,
        1.0f);

    g.drawRoundedRectangle (
        targetBox.toFloat().reduced (0.5f),
        6.0f,
        1.0f);

    g.setColour (outlineColour);
    g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));

    g.drawText (
        "SOURCE — NAM ORIGINAL + IR",
        sourceBox.getX() + 14,
        sourceBox.getY() + 8,
        sourceBox.getWidth() - 28,
        22,
        juce::Justification::centredLeft);

    g.drawText (
        "TARGET — GP-200 NAM, CAB OFF",
        targetBox.getX() + 14,
        targetBox.getY() + 8,
        targetBox.getWidth() - 28,
        22,
        juce::Justification::centredLeft);
}

void ToneMatchPanel::resized()
{
    const int left = 34;
    const int right = getWidth() - 34;
    const int buttonWidth = 150;
    const int clearWidth = 70;
	
	matchCurveComponent.setBounds (
    20,
    382,
    getWidth() - 40,
    getHeight() - 462);

    sourceStatusLabel.setBounds (
        left,
        116,
        right - left - buttonWidth - clearWidth - 20,
        24);

    sourceDetailsLabel.setBounds (
        left,
        142,
        right - left - buttonWidth - clearWidth - 20,
        24);

    sourceCaptureButton.setBounds (
        right - buttonWidth - clearWidth - 10,
        126,
        buttonWidth,
        34);

    sourceClearButton.setBounds (
        right - clearWidth,
        126,
        clearWidth,
        34);

    targetStatusLabel.setBounds (
        left,
        248,
        right - left - buttonWidth - clearWidth - 20,
        24);

    targetDetailsLabel.setBounds (
        left,
        274,
        right - left - buttonWidth - clearWidth - 20,
        24);

    targetCaptureButton.setBounds (
        right - buttonWidth - clearWidth - 10,
        258,
        buttonWidth,
        34);

    targetClearButton.setBounds (
        right - clearWidth,
        258,
        clearWidth,
        34);

    globalStatusLabel.setBounds (
        20,
        346,
        getWidth() - 40,
        26);

    analyseButton.setBounds (
    getWidth() / 2 - 160,
    getHeight() - 58,
    150,
    36);

closeButton.setBounds (
    getWidth() / 2 + 10,
    getHeight() - 58,
    150,
    36);
}

void ToneMatchPanel::timerCallback()
{
    updateControls();
}

void ToneMatchPanel::handleSourceCapture()
{
    if (processorRef.isToneMatchCapturing())
    {
        stopCapture();
        return;
    }

    startCapture (tonematch::CaptureRole::source);
}

void ToneMatchPanel::handleTargetCapture()
{
    if (processorRef.isToneMatchCapturing())
    {
        stopCapture();
        return;
    }

    startCapture (tonematch::CaptureRole::target);
}

void ToneMatchPanel::startCapture (
    tonematch::CaptureRole role)
{
	 processorRef.clearToneMatchComparison();
    matchCurveComponent.clear();
    if (!processorRef.startToneMatchCapture (role))
    {
        globalStatusLabel.setText (
            "Could not start capture",
            juce::dontSendNotification);

        return;
    }

    globalStatusLabel.setText (
        role == tonematch::CaptureRole::source
            ? "Capturing SOURCE..."
            : "Capturing GP-200...",
        juce::dontSendNotification);

    updateControls();
}

void ToneMatchPanel::stopCapture()
{
    const auto capture =
        processorRef.stopToneMatchCapture();
		
		

    if (!capture.isValid())
    {
        globalStatusLabel.setText (
            "Capture failed: " + capture.errorMessage,
            juce::dontSendNotification);
    }
    else
    {
        globalStatusLabel.setText (
            capture.role == tonematch::CaptureRole::source
                ? "SOURCE captured successfully"
                : "GP-200 captured successfully",
            juce::dontSendNotification);
    }

    updateControls();
}

void ToneMatchPanel::updateControls()
{
    const auto capturing =
        processorRef.isToneMatchCapturing();

  const auto currentRole =
    processorRef.getToneMatchCaptureRole();

    sourceCaptureButton.setButtonText (
    capturing &&
            currentRole == tonematch::CaptureRole::source
        ? "Stop Source"
        : "Capture Source");

targetCaptureButton.setButtonText (
    capturing &&
            currentRole == tonematch::CaptureRole::target
        ? "Stop GP-200"
        : "Capture GP-200");
		
		sourceCaptureButton.setEnabled (
    !capturing ||
    currentRole == tonematch::CaptureRole::source);

targetCaptureButton.setEnabled (
    !capturing ||
    currentRole == tonematch::CaptureRole::target);

    sourceClearButton.setEnabled (!capturing);
    targetClearButton.setEnabled (!capturing);

    const auto hasSource =
        processorRef.hasToneMatchCapture (
            tonematch::CaptureRole::source);

    const auto hasTarget =
        processorRef.hasToneMatchCapture (
            tonematch::CaptureRole::target);

    analyseButton.setEnabled (
        !capturing &&
        hasSource &&
        hasTarget);

    updateCaptureLabels();
}

void ToneMatchPanel::updateCaptureLabels()
{
    const auto sourceCapture =
        processorRef.getToneMatchCaptureCopy (
            tonematch::CaptureRole::source);

    const auto targetCapture =
        processorRef.getToneMatchCaptureCopy (
            tonematch::CaptureRole::target);

    if (sourceCapture.isValid())
    {
        sourceStatusLabel.setText (
            "Ready",
            juce::dontSendNotification);

        sourceStatusLabel.setColour (
            juce::Label::textColourId,
            readyColour);

        sourceDetailsLabel.setText (
            formatDuration (sourceCapture.durationSeconds)
                + "    Peak "
                + formatPeak (sourceCapture.peakDb)
                + (sourceCapture.wasClipped
                       ? "    CLIPPING"
                       : ""),
            juce::dontSendNotification);
    }
    else
    {
        sourceStatusLabel.setText (
            "Empty",
            juce::dontSendNotification);

        sourceStatusLabel.setColour (
            juce::Label::textColourId,
            mutedTextColour);

        sourceDetailsLabel.setText (
            "No source audio captured",
            juce::dontSendNotification);
    }

    if (targetCapture.isValid())
    {
        targetStatusLabel.setText (
            "Ready",
            juce::dontSendNotification);

        targetStatusLabel.setColour (
            juce::Label::textColourId,
            readyColour);

        targetDetailsLabel.setText (
            formatDuration (targetCapture.durationSeconds)
                + "    Peak "
                + formatPeak (targetCapture.peakDb)
                + (targetCapture.wasClipped
                       ? "    CLIPPING"
                       : ""),
            juce::dontSendNotification);
    }
    else
    {
        targetStatusLabel.setText (
            "Empty",
            juce::dontSendNotification);

        targetStatusLabel.setColour (
            juce::Label::textColourId,
            mutedTextColour);

        targetDetailsLabel.setText (
            "No GP-200 audio captured",
            juce::dontSendNotification);
    }

    if (processorRef.isToneMatchCapturing())
    {
        const auto duration =
            processorRef.getToneMatchCapturedDurationSeconds();

        const auto peakLinear =
            processorRef.getToneMatchCapturePeakLinear();

        const auto peakDb =
            juce::Decibels::gainToDecibels (
                static_cast<double> (peakLinear),
                -100.0);

        globalStatusLabel.setText (
            "Capturing — "
                + formatDuration (duration)
                + " — Peak "
                + formatPeak (peakDb),
            juce::dontSendNotification);
    }
}

juce::String ToneMatchPanel::formatDuration (
    double seconds)
{
    const auto totalSeconds =
        juce::jmax (0, static_cast<int> (seconds));

    const auto minutes = totalSeconds / 60;
    const auto remainingSeconds = totalSeconds % 60;

    return juce::String (minutes).paddedLeft ('0', 2)
        + ":"
        + juce::String (remainingSeconds).paddedLeft ('0', 2);
}

juce::String ToneMatchPanel::formatPeak (
    double peakDb)
{
    return juce::String (peakDb, 1) + " dBFS";
}