/*
    GP200 VST

    Portions adapted from phash/gp200edit or and its contributors.
    Those portions are licensed under GPL-3.0-or-later.

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "ToneMatchCurveComponent.h"
#include "../GP200Typography.h"

#include <algorithm>
#include <cmath>

namespace
{
const juce::Colour graphBackground{0xff151a1d};
const juce::Colour graphGrid{0xff394146};
const juce::Colour graphText{0xffb8b8b8};
const juce::Colour rawCurveColour{0xff57f05f};
const juce::Colour appliedCurveColour{0xffffa42a};
const juce::Colour graphOutline{0xffffa42a};
}

void ToneMatchCurveComponent::setComparison (
    const tonematch::ToneMatchComparisonResult& comparison)
{
    if (!comparison.isValid())
    {
        clear();
        return;
    }

    rawFrequencyHz = comparison.frequencyHz;
    rawCurveDb = comparison.rawCorrectionDb;
    updateDisplayRange();
    repaint();
}

void ToneMatchCurveComponent::setResult (
    const tonematch::ToneMatchResult& result)
{
    if (!result.hasValidImpulseResponse()
        || result.frequencyHz.size()
            != result.smoothedCorrectionDb.size())
    {
        clearAppliedCurve();
        return;
    }

    appliedFrequencyHz = result.frequencyHz;
    appliedCurveDb = result.smoothedCorrectionDb;
    updateDisplayRange();
    repaint();
}

void ToneMatchCurveComponent::clear()
{
    rawFrequencyHz.clear();
    rawCurveDb.clear();
    clearAppliedCurve();
}

void ToneMatchCurveComponent::clearAppliedCurve()
{
    appliedFrequencyHz.clear();
    appliedCurveDb.clear();
    updateDisplayRange();
    repaint();
}

void ToneMatchCurveComponent::updateDisplayRange()
{
    displayMinimumDb = -18.0;
    displayMaximumDb = 18.0;

    auto includeCurve = [this] (const std::vector<double>& values)
    {
        if (values.empty())
            return;

        const auto [minimumIt, maximumIt] =
            std::minmax_element (
                values.begin(),
                values.end());

        constexpr double gridStepDb = 6.0;

        displayMinimumDb = std::min (
            displayMinimumDb,
            std::floor (*minimumIt / gridStepDb)
                * gridStepDb);

        displayMaximumDb = std::max (
            displayMaximumDb,
            std::ceil (*maximumIt / gridStepDb)
                * gridStepDb);
    };

    includeCurve (rawCurveDb);
    includeCurve (appliedCurveDb);
}

float ToneMatchCurveComponent::frequencyToX (
    double frequencyHz,
    juce::Rectangle<float> graphBounds) const
{
    const auto safeFrequency = juce::jlimit (
        displayMinimumFrequencyHz,
        displayMaximumFrequencyHz,
        frequencyHz);

    const auto normalised =
        (std::log (safeFrequency)
         - std::log (displayMinimumFrequencyHz))
        / (std::log (displayMaximumFrequencyHz)
           - std::log (displayMinimumFrequencyHz));

    return graphBounds.getX()
        + static_cast<float> (normalised)
            * graphBounds.getWidth();
}

float ToneMatchCurveComponent::decibelsToY (
    double decibels,
    juce::Rectangle<float> graphBounds) const
{
    const auto safeDb = juce::jlimit (
        displayMinimumDb,
        displayMaximumDb,
        decibels);

    const auto normalised =
        (displayMaximumDb - safeDb)
        / (displayMaximumDb - displayMinimumDb);

    return graphBounds.getY()
        + static_cast<float> (normalised)
            * graphBounds.getHeight();
}

void ToneMatchCurveComponent::drawCurve (
    juce::Graphics& g,
    const std::vector<double>& frequencyHz,
    const std::vector<double>& curveDb,
    juce::Rectangle<float> graphBounds,
    juce::Colour colour,
    float thickness) const
{
    if (frequencyHz.size() < 2
        || frequencyHz.size() != curveDb.size())
        return;

    juce::Path path;
    bool started = false;

    for (std::size_t index = 0;
         index < frequencyHz.size();
         ++index)
    {
        const auto frequency = frequencyHz[index];

        if (frequency < displayMinimumFrequencyHz
            || frequency > displayMaximumFrequencyHz)
            continue;

        const auto x = frequencyToX (
            frequency,
            graphBounds);

        const auto y = decibelsToY (
            curveDb[index],
            graphBounds);

        if (!started)
        {
            path.startNewSubPath (x, y);
            started = true;
        }
        else
        {
            path.lineTo (x, y);
        }
    }

    if (!started)
        return;

    g.setColour (colour);
    g.strokePath (
        path,
        juce::PathStrokeType (
            thickness,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
}

void ToneMatchCurveComponent::paint (juce::Graphics& g)
{
    g.fillAll (graphBackground);

    const auto bounds =
        getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (graphOutline.withAlpha (0.75f));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    g.setFont (gp200ui::semibold (14.5f));
    g.setColour (graphOutline);
    g.drawText (
        "MATCH CURVE | TARGET minus SOURCE",
        12, 7, getWidth() - 260, 20,
        juce::Justification::centredLeft);

    g.setFont (gp200ui::medium (12.5f));
    g.setColour (rawCurveColour);
    g.drawText (
        "RAW",
        getWidth() - 218, 7, 48, 20,
        juce::Justification::centredRight);

    g.setColour (graphText);
    g.drawText (
        "/",
        getWidth() - 166, 7, 12, 20,
        juce::Justification::centred);

    g.setColour (appliedCurveColour);
    g.drawText (
        "APPLIED",
        getWidth() - 150, 7, 82, 20,
        juce::Justification::centredLeft);

    const auto graphBounds = juce::Rectangle<float> (
        48.0f,
        34.0f,
        static_cast<float> (getWidth()) - 64.0f,
        static_cast<float> (getHeight()) - 58.0f);

    if (graphBounds.getWidth() <= 0.0f
        || graphBounds.getHeight() <= 0.0f)
        return;

    const double frequencyMarks[]
    {
        50.0, 100.0, 200.0, 500.0,
        1000.0, 2000.0, 5000.0, 10000.0
    };

    g.setFont (gp200ui::regular (12.5f));

    for (const auto frequency : frequencyMarks)
    {
        const auto x = frequencyToX (
            frequency,
            graphBounds);

        g.setColour (graphGrid);
        g.drawVerticalLine (
            juce::roundToInt (x),
            graphBounds.getY(),
            graphBounds.getBottom());

        g.setColour (graphText);

        const auto label =
            frequency >= 1000.0
                ? juce::String (
                      frequency / 1000.0,
                      frequency < 10000.0 ? 1 : 0)
                      + "k"
                : juce::String (
                      static_cast<int> (frequency));

        g.drawText (
            label,
            juce::roundToInt (x) - 22,
            juce::roundToInt (
                graphBounds.getBottom()) + 2,
            44, 15,
            juce::Justification::centred);
    }

    for (double db = displayMinimumDb;
         db <= displayMaximumDb;
         db += 6.0)
    {
        const auto y = decibelsToY (
            db,
            graphBounds);

        g.setColour (
            std::abs (db) < 0.001
                ? graphText.withAlpha (0.75f)
                : graphGrid);

        g.drawHorizontalLine (
            juce::roundToInt (y),
            graphBounds.getX(),
            graphBounds.getRight());

        g.setColour (graphText);
        g.drawText (
            (db > 0.0 ? "+" : "")
                + juce::String (db, 0),
            4,
            juce::roundToInt (y) - 8,
            38, 16,
            juce::Justification::centredRight);
    }

    if (rawFrequencyHz.empty())
    {
        g.setColour (graphText);
        g.setFont (gp200ui::regular (13.5f));
        g.drawText (
            "Capture SOURCE and TARGET, then press Analyse",
            graphBounds.toNearestInt(),
            juce::Justification::centred);
        return;
    }

    // RAW remains visible beneath the applied curve.
    drawCurve (
        g,
        rawFrequencyHz,
        rawCurveDb,
        graphBounds,
        rawCurveColour.withAlpha (0.78f),
        1.6f);

    drawCurve (
        g,
        appliedFrequencyHz,
        appliedCurveDb,
        graphBounds,
        appliedCurveColour,
        2.2f);
}
