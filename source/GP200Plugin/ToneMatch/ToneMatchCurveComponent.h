/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed under GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "ToneMatchComparison.h"

class ToneMatchCurveComponent final
    : public juce::Component
{
  public:
    ToneMatchCurveComponent() = default;

    void setComparison (
        tonematch::ToneMatchComparisonResult newComparison);

    void clear();

    void paint (juce::Graphics& g) override;

  private:
    float frequencyToX (
        double frequencyHz,
        juce::Rectangle<float> graphBounds) const;

    float decibelsToY (
        double decibels,
        juce::Rectangle<float> graphBounds) const;

    tonematch::ToneMatchComparisonResult comparison;

    double displayMinimumFrequencyHz{40.0};
    double displayMaximumFrequencyHz{18000.0};

    // Escala visual inicial. No limita todavía la curva real.
    double displayMinimumDb{-18.0};
    double displayMaximumDb{18.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (
        ToneMatchCurveComponent)
};