#pragma once

#include "ToneMatchTypes.h"

namespace tonematch
{
class IToneMatchSolver
{
  public:
    virtual ~IToneMatchSolver() = default;

    virtual juce::String getName() const = 0;
    virtual juce::String getDescription() const = 0;

    virtual ToneMatchResult solve (
        const ToneAnalysisProfile& source,
        const ToneAnalysisProfile& target,
        const ToneMatchOptions& options) = 0;
};
}