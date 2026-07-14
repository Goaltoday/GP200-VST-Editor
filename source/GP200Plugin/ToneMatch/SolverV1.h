#pragma once

#include "IToneMatchSolver.h"

namespace tonematch
{
class SolverV1 final : public IToneMatchSolver
{
  public:
    juce::String getName() const override;
    juce::String getDescription() const override;

    ToneMatchResult solve (
        const ToneAnalysisProfile& source,
        const ToneAnalysisProfile& target,
        const ToneMatchOptions& options) override;

  private:
    static double interpolate (
        const std::vector<double>& frequencyHz,
        const std::vector<double>& values,
        double requestedFrequencyHz);

    static bool validateOptions (
        const ToneMatchOptions& options,
        juce::String& errorMessage);
};
}
