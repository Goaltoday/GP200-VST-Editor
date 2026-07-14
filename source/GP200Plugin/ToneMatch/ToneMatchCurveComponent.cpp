#include "ToneMatchCurveComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
const juce::Colour graphBackground{0xff151a1d};
const juce::Colour graphGrid{0xff394146};
const juce::Colour graphText{0xffb8b8b8};
const juce::Colour graphCurve{0xff57f05f};
const juce::Colour graphOutline{0xffffa42a};
}

void ToneMatchCurveComponent::setComparison (
    tonematch::ToneMatchComparisonResult newComparison)
{
    comparison = std::move (newComparison);

    // Mantiene como mínimo la escala original de -18 a +18 dB,
    // pero se amplía automáticamente en pasos de 6 dB para mostrar
    // todos los valores de la curva RAW.
    displayMinimumDb = -18.0;
    displayMaximumDb = 18.0;

    if (!comparison.rawCorrectionDb.empty())
    {
        const auto [minimumIt, maximumIt] =
            std::minmax_element (
                comparison.rawCorrectionDb.begin(),
                comparison.rawCorrectionDb.end());

        constexpr double gridStepDb = 6.0;

        const auto requiredMinimumDb =
            std::floor (*minimumIt / gridStepDb) * gridStepDb;

        const auto requiredMaximumDb =
            std::ceil (*maximumIt / gridStepDb) * gridStepDb;

        displayMinimumDb =
            std::min (displayMinimumDb, requiredMinimumDb);

        displayMaximumDb =
            std::max (displayMaximumDb, requiredMaximumDb);

        if (*minimumIt <= displayMinimumDb + 0.001)
            displayMinimumDb -= gridStepDb;

        if (*maximumIt >= displayMaximumDb - 0.001)
            displayMaximumDb += gridStepDb;
    }

    repaint();
}

void ToneMatchCurveComponent::clear()
{
    comparison = {};
    displayMinimumDb = -18.0;
    displayMaximumDb = 18.0;
    repaint();
}

float ToneMatchCurveComponent::frequencyToX (
    double frequencyHz,
    juce::Rectangle<float> graphBounds) const
{
    const auto safeFrequency =
        juce::jlimit (
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
    const auto safeDb =
        juce::jlimit (
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

void ToneMatchCurveComponent::paint (
    juce::Graphics& g)
{
    g.fillAll (graphBackground);

    auto bounds =
        getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (graphOutline.withAlpha (0.75f));
    g.drawRoundedRectangle (
        bounds,
        5.0f,
        1.0f);

    g.setColour (graphOutline);
    g.setFont (
        juce::FontOptions (13.0f)
            .withStyle ("Bold"));

    g.drawText (
        "MATCH CURVE — TARGET minus SOURCE",
        12,
        8,
        getWidth() - 24,
        22,
        juce::Justification::centredLeft);

    const auto graphBounds =
        juce::Rectangle<float> (
            48.0f,
            40.0f,
            static_cast<float> (getWidth()) - 64.0f,
            static_cast<float> (getHeight()) - 68.0f);

    if (graphBounds.getWidth() <= 0.0f
        || graphBounds.getHeight() <= 0.0f)
    {
        return;
    }

    const double frequencyMarks[]
    {
        50.0,
        100.0,
        200.0,
        500.0,
        1000.0,
        2000.0,
        5000.0,
        10000.0
    };

    g.setFont (juce::FontOptions (10.0f));

    for (const auto frequency : frequencyMarks)
    {
        const auto x =
            frequencyToX (
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
                graphBounds.getBottom()) + 3,
            44,
            16,
            juce::Justification::centred);
    }

    for (double db = displayMinimumDb;
         db <= displayMaximumDb;
         db += 6.0)
    {
        const auto y =
            decibelsToY (
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
            38,
            16,
            juce::Justification::centredRight);
    }

    if (!comparison.isValid())
    {
        g.setColour (graphText);

        g.drawText (
            "Capture SOURCE and TARGET, then press Analyse",
            graphBounds.toNearestInt(),
            juce::Justification::centred);

        return;
    }

    juce::Path curvePath;

    bool pathStarted = false;

    for (std::size_t index = 0;
         index < comparison.frequencyHz.size();
         ++index)
    {
        const auto frequency =
            comparison.frequencyHz[index];

        if (frequency
                < displayMinimumFrequencyHz
            || frequency
                > displayMaximumFrequencyHz)
        {
            continue;
        }

        const auto x =
            frequencyToX (
                frequency,
                graphBounds);

        const auto y =
            decibelsToY (
                comparison.rawCorrectionDb[index],
                graphBounds);

        if (!pathStarted)
        {
            curvePath.startNewSubPath (x, y);
            pathStarted = true;
        }
        else
        {
            curvePath.lineTo (x, y);
        }
    }

    if (pathStarted)
    {
        g.setColour (graphCurve);
        g.strokePath (
            curvePath,
            juce::PathStrokeType (
                2.0f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
    }
}