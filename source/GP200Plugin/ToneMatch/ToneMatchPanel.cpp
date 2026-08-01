/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed under GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "ToneMatchPanel.h"
#include "../GP200Typography.h"

namespace
{
const juce::Colour backgroundColour{0xff181d20};
const juce::Colour panelColour{0xff252b2f};
const juce::Colour outlineColour{0xffffa42a};
const juce::Colour textColour{0xffffffff};
const juce::Colour mutedTextColour{0xffb8b8b8};
const juce::Colour readyColour{0xff57f05f};
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
    addAndMakeVisible (generateIRButton);
    addAndMakeVisible (closeButton);
    addAndMakeVisible (sourceStatusLabel);
    addAndMakeVisible (sourceDetailsLabel);
    addAndMakeVisible (targetStatusLabel);
    addAndMakeVisible (targetDetailsLabel);
    addAndMakeVisible (globalStatusLabel);
    addAndMakeVisible (matchCurveComponent);

    auto setupButton = [] (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, panelColour);
        button.setColour (
            juce::TextButton::buttonOnColourId,
            panelColour.brighter (0.15f));
        button.setColour (juce::TextButton::textColourOffId, outlineColour);
        button.setColour (
            juce::TextButton::textColourOnId,
            outlineColour.brighter (0.15f));
    };

    setupButton (sourceCaptureButton);
    setupButton (sourceClearButton);
    setupButton (targetCaptureButton);
    setupButton (targetClearButton);
    setupButton (analyseButton);
    setupButton (generateIRButton);
    setupButton (closeButton);

    sourceStatusLabel.setColour (juce::Label::textColourId, textColour);
    sourceDetailsLabel.setColour (juce::Label::textColourId, mutedTextColour);
    targetStatusLabel.setColour (juce::Label::textColourId, textColour);
    targetDetailsLabel.setColour (juce::Label::textColourId, mutedTextColour);
    globalStatusLabel.setColour (juce::Label::textColourId, mutedTextColour);

    sourceStatusLabel.setJustificationType (juce::Justification::centredLeft);
    sourceDetailsLabel.setJustificationType (juce::Justification::centredLeft);
    targetStatusLabel.setJustificationType (juce::Justification::centredLeft);
    targetDetailsLabel.setJustificationType (juce::Justification::centredLeft);
    globalStatusLabel.setJustificationType (juce::Justification::centred);

    // Keep the secondary Tone Match text readable with Space Grotesk.
    // These labels do not use the button LookAndFeel, so set their sizes
    // explicitly instead of relying on JUCE's smaller default label font.
    sourceStatusLabel.setFont (gp200ui::medium (15.25f));
    targetStatusLabel.setFont (gp200ui::medium (15.25f));
    sourceDetailsLabel.setFont (gp200ui::regular (14.75f));
    targetDetailsLabel.setFont (gp200ui::regular (14.75f));
    globalStatusLabel.setFont (gp200ui::regular (14.5f));

    sourceCaptureButton.onClick = [this] { handleSourceCapture(); };
    targetCaptureButton.onClick = [this] { handleTargetCapture(); };

    sourceClearButton.onClick = [this]
    {
        if (processorRef.isToneMatchCapturing())
            return;

        processorRef.clearToneMatchCapture (tonematch::CaptureRole::source);
        processorRef.clearToneMatchComparison();
        processorRef.clearToneMatchResult();
        matchCurveComponent.clear();
        globalStatusLabel.setText (
            "SOURCE capture cleared",
            juce::dontSendNotification);
        updateControls();
    };

    targetClearButton.onClick = [this]
    {
        if (processorRef.isToneMatchCapturing())
            return;

        processorRef.clearToneMatchCapture (tonematch::CaptureRole::target);
        processorRef.clearToneMatchComparison();
        processorRef.clearToneMatchResult();
        matchCurveComponent.clear();
        globalStatusLabel.setText (
            "TARGET capture cleared",
            juce::dontSendNotification);
        updateControls();
    };

    analyseButton.onClick = [this] { analyseCaptures(); };
    generateIRButton.onClick = [this] { generateAndSaveIR(); };

    closeButton.onClick = [this]
    {
        if (processorRef.isToneMatchCapturing())
            stopCapture();

        if (closeRequested)
            closeRequested();
    };

    startTimerHz (20);
    updateControls();
}

void ToneMatchPanel::analyseCaptures()
{
    analyseButton.setEnabled (false);
    processorRef.clearToneMatchComparison();
    processorRef.clearToneMatchResult();
    matchCurveComponent.clear();

    globalStatusLabel.setText (
        "Analysing SOURCE...",
        juce::dontSendNotification);

    if (!processorRef.analyseToneMatchCapture (tonematch::CaptureRole::source))
    {
        globalStatusLabel.setText (
            "SOURCE analysis failed",
            juce::dontSendNotification);
        updateControls();
        return;
    }

    globalStatusLabel.setText (
        "Analysing TARGET...",
        juce::dontSendNotification);

    if (!processorRef.analyseToneMatchCapture (tonematch::CaptureRole::target))
    {
        globalStatusLabel.setText (
            "TARGET analysis failed",
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

    const auto comparison = processorRef.getToneMatchComparisonCopy();
    matchCurveComponent.setComparison (comparison);

    const auto minimumText = juce::String (
        comparison.minimumCorrectionDb,
        1);

    const auto maximumText =
        (comparison.maximumCorrectionDb >= 0.0 ? "+" : "")
        + juce::String (comparison.maximumCorrectionDb, 1);

    globalStatusLabel.setText (
        "RAW comparison ready | "
            + minimumText
            + " to "
            + maximumText
            + " dB",
        juce::dontSendNotification);

    updateControls();
    repaint();
}

bool ToneMatchPanel::generateIR()
{
    generateIRButton.setEnabled (false);
    globalStatusLabel.setText (
        "Generating 44.1 kHz / 1024 sample IR...",
        juce::dontSendNotification);

    if (!processorRef.generateToneMatchIR())
    {
        globalStatusLabel.setText (
            "IR generation failed",
            juce::dontSendNotification);
        updateControls();
        return false;
    }

    const auto result = processorRef.getToneMatchResultCopy();
    const auto peak = result.impulseResponse.getMagnitude (
        0,
        result.impulseResponse.getNumSamples());
    const auto peakDb = juce::Decibels::gainToDecibels (
        static_cast<double> (peak),
        -160.0);

    auto status =
        "IR ready | 44.1 kHz | 1024 samples | peak "
        + juce::String (peakDb, 1)
        + " dBFS | estimated error "
        + juce::String (result.errorBeforeDb, 1)
        + " -> "
        + juce::String (result.errorAfterDb, 1)
        + " dB";

    if (result.warning.isNotEmpty())
        status += " | " + result.warning;

    globalStatusLabel.setText (status, juce::dontSendNotification);
    updateControls();
    return true;
}

void ToneMatchPanel::generateAndSaveIR()
{
    if (!generateIR())
        return;

    saveIR();
}

void ToneMatchPanel::saveIR()
{
    if (!processorRef.hasToneMatchResult())
        return;

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save tone-match IR",
        juce::File::getSpecialLocation (
            juce::File::userDocumentsDirectory)
            .getChildFile ("GP200_ToneMatch_IR.wav"),
        "*.wav");

    const auto flags =
        juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync (
        flags,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();

            if (file == juce::File{})
            {
                globalStatusLabel.setText (
                    "IR generated - save cancelled",
                    juce::dontSendNotification);
                return;
            }

            if (!file.hasFileExtension ("wav"))
                file = file.withFileExtension ("wav");

            juce::String errorMessage;

            if (processorRef.saveToneMatchIRToFile (file, errorMessage))
            {
                globalStatusLabel.setText (
                    "IR saved: " + file.getFileName(),
                    juce::dontSendNotification);
            }
            else
            {
                globalStatusLabel.setText (
                    "IR save failed: " + errorMessage,
                    juce::dontSendNotification);
            }
        });
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
    g.setFont (gp200ui::semibold (23.0f));
    g.drawText (
        "TONE MATCH",
        20,
        12,
        getWidth() - 40,
        32,
        juce::Justification::centredLeft);

    g.setColour (mutedTextColour);
    g.setFont (gp200ui::regular (15.75f));
    g.drawText (
        "Capture the GP-200 source and the desired target separately.",
        20,
        44,
        getWidth() - 40,
        24,
        juce::Justification::centredLeft);

    const auto sourceBox = juce::Rectangle<int> (
        20,
        82,
        getWidth() - 40,
        78);

    const auto targetBox = juce::Rectangle<int> (
        20,
        170,
        getWidth() - 40,
        78);

    g.setColour (panelColour);
    g.fillRoundedRectangle (sourceBox.toFloat(), 6.0f);
    g.fillRoundedRectangle (targetBox.toFloat(), 6.0f);

    g.setColour (outlineColour.withAlpha (0.7f));
    g.drawRoundedRectangle (sourceBox.toFloat().reduced (0.5f), 6.0f, 1.0f);
    g.drawRoundedRectangle (targetBox.toFloat().reduced (0.5f), 6.0f, 1.0f);

    g.setColour (outlineColour);
    g.setFont (gp200ui::semibold (15.5f));
    g.drawText (
        "SOURCE | GP-200 NAM, CAB OFF",
        sourceBox.getX() + 14,
        sourceBox.getY() + 8,
        sourceBox.getWidth() - 28,
        22,
        juce::Justification::centredLeft);

    g.drawText (
        "TARGET | REFERENCE NAM + IR",
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
    const int textWidth =
        right - left - buttonWidth - clearWidth - 20;

    // Las zonas de captura se mantienen compactas para dedicar
    // la mayor parte de la ventana a la gráfica de comparación.
    sourceStatusLabel.setBounds (
        left,
        108,
        textWidth,
        20);
    sourceDetailsLabel.setBounds (
        left,
        130,
        textWidth,
        18);
    sourceCaptureButton.setBounds (
        right - buttonWidth - clearWidth - 10,
        112,
        buttonWidth,
        30);
    sourceClearButton.setBounds (
        right - clearWidth,
        112,
        clearWidth,
        30);

    targetStatusLabel.setBounds (
        left,
        196,
        textWidth,
        20);
    targetDetailsLabel.setBounds (
        left,
        218,
        textWidth,
        18);
    targetCaptureButton.setBounds (
        right - buttonWidth - clearWidth - 10,
        200,
        buttonWidth,
        30);
    targetClearButton.setBounds (
        right - clearWidth,
        200,
        clearWidth,
        30);

    globalStatusLabel.setBounds (
        20,
        256,
        getWidth() - 40,
        24);

    const int graphTop = 288;
    const int graphBottom = getHeight() - 72;

    matchCurveComponent.setBounds (
        20,
        graphTop,
        getWidth() - 40,
        juce::jmax (120, graphBottom - graphTop));

    constexpr int analyseWidth = 140;
    constexpr int generateWidth = 190;
    constexpr int closeWidth = 140;
    constexpr int gap = 10;

    const auto totalWidth = analyseWidth + generateWidth + closeWidth + 2 * gap;
    const auto startX = (getWidth() - totalWidth) / 2;
    const auto buttonY = getHeight() - 58;

    analyseButton.setBounds (startX, buttonY, analyseWidth, 36);
    generateIRButton.setBounds (startX + analyseWidth + gap, buttonY, generateWidth, 36);
    closeButton.setBounds (startX + analyseWidth + gap + generateWidth + gap, buttonY, closeWidth, 36);
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

void ToneMatchPanel::startCapture (tonematch::CaptureRole role)
{
    processorRef.clearToneMatchComparison();
    processorRef.clearToneMatchResult();
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
            : "Capturing TARGET...",
        juce::dontSendNotification);

    updateControls();
}

void ToneMatchPanel::stopCapture()
{
    const auto capture = processorRef.stopToneMatchCapture();

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
                : "TARGET captured successfully",
            juce::dontSendNotification);
    }

    updateControls();
}

void ToneMatchPanel::updateControls()
{
    const auto capturing = processorRef.isToneMatchCapturing();
    const auto currentRole = processorRef.getToneMatchCaptureRole();

    sourceCaptureButton.setButtonText (
        capturing && currentRole == tonematch::CaptureRole::source
            ? "Stop Source"
            : "Capture Source");

    targetCaptureButton.setButtonText (
        capturing && currentRole == tonematch::CaptureRole::target
            ? "Stop Target"
            : "Capture Target");

    sourceCaptureButton.setEnabled (
        !capturing || currentRole == tonematch::CaptureRole::source);
    targetCaptureButton.setEnabled (
        !capturing || currentRole == tonematch::CaptureRole::target);

    sourceClearButton.setEnabled (!capturing);
    targetClearButton.setEnabled (!capturing);

    const auto hasSource = processorRef.hasToneMatchCapture (
        tonematch::CaptureRole::source);
    const auto hasTarget = processorRef.hasToneMatchCapture (
        tonematch::CaptureRole::target);

    analyseButton.setEnabled (!capturing && hasSource && hasTarget);
    generateIRButton.setEnabled (
        !capturing && processorRef.hasToneMatchComparison());

    updateCaptureLabels();
}

void ToneMatchPanel::updateCaptureLabels()
{
    const auto sourceCapture = processorRef.getToneMatchCaptureCopy (
        tonematch::CaptureRole::source);
    const auto targetCapture = processorRef.getToneMatchCaptureCopy (
        tonematch::CaptureRole::target);

    if (sourceCapture.isValid())
    {
        sourceStatusLabel.setText ("Ready", juce::dontSendNotification);
        sourceStatusLabel.setColour (juce::Label::textColourId, readyColour);
        sourceDetailsLabel.setText (
            formatDuration (sourceCapture.durationSeconds)
                + "    Peak "
                + formatPeak (sourceCapture.peakDb)
                + (sourceCapture.wasClipped ? "    CLIPPING" : ""),
            juce::dontSendNotification);
    }
    else
    {
        sourceStatusLabel.setText ("Empty", juce::dontSendNotification);
        sourceStatusLabel.setColour (
            juce::Label::textColourId,
            mutedTextColour);
        sourceDetailsLabel.setText (
            "No GP-200 source audio captured",
            juce::dontSendNotification);
    }

    if (targetCapture.isValid())
    {
        targetStatusLabel.setText ("Ready", juce::dontSendNotification);
        targetStatusLabel.setColour (juce::Label::textColourId, readyColour);
        targetDetailsLabel.setText (
            formatDuration (targetCapture.durationSeconds)
                + "    Peak "
                + formatPeak (targetCapture.peakDb)
                + (targetCapture.wasClipped ? "    CLIPPING" : ""),
            juce::dontSendNotification);
    }
    else
    {
        targetStatusLabel.setText ("Empty", juce::dontSendNotification);
        targetStatusLabel.setColour (
            juce::Label::textColourId,
            mutedTextColour);
        targetDetailsLabel.setText (
            "No reference target audio captured",
            juce::dontSendNotification);
    }

    if (processorRef.isToneMatchCapturing())
    {
        const auto duration =
            processorRef.getToneMatchCapturedDurationSeconds();
        const auto peakLinear =
            processorRef.getToneMatchCapturePeakLinear();
        const auto peakDb = juce::Decibels::gainToDecibels (
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

juce::String ToneMatchPanel::formatDuration (double seconds)
{
    const auto totalSeconds = juce::jmax (0, static_cast<int> (seconds));
    const auto minutes = totalSeconds / 60;
    const auto remainingSeconds = totalSeconds % 60;

    return juce::String (minutes).paddedLeft ('0', 2)
        + ":"
        + juce::String (remainingSeconds).paddedLeft ('0', 2);
}

juce::String ToneMatchPanel::formatPeak (double peakDb)
{
    return juce::String (peakDb, 1) + " dBFS";
}
