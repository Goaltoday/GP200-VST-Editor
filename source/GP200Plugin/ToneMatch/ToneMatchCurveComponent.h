/*
    GP200 VST

    Portions adapted from phash/gp200editor and  its contributors.
    Those portions are licensed under GPL-3.0-or-later.

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "ToneMatchComparison.h"
#include "ToneMatchTypes.h"

class ToneMatchCurveComponent final : public juce::Component
{
  public:
    ToneMatchCurveComponent() = default;

    void setComparison (
        const tonematch::ToneMatchComparisonResult& comparison);

    void setResult (
        const tonematch::ToneMatchResult& result);

    void clear();
    void clearAppliedCurve();

    void paint (juce::Graphics& g) override;

  private:
    void updateDisplayRange();

    float frequencyToX (
        double frequencyHz,
        juce::Rectangle<float> graphBounds) const;

    float decibelsToY (
        double decibels,
        juce::Rectangle<float> graphBounds) const;

    void drawCurve (
        juce::Graphics& g,
        const std::vector<double>& frequencyHz,
        const std::vector<double>& curveDb,
        juce::Rectangle<float> graphBounds,
        juce::Colour colour,
        float thickness) const;

    std::vector<double> rawFrequencyHz;
    std::vector<double> rawCurveDb;

    std::vector<double> appliedFrequencyHz;
    std::vector<double> appliedCurveDb;

    double displayMinimumFrequencyHz{40.0};
    double displayMaximumFrequencyHz{18000.0};
    double displayMinimumDb{-18.0};
    double displayMaximumDb{18.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (
        ToneMatchCurveComponent)
};
