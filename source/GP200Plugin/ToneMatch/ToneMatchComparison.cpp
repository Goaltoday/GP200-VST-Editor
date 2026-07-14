#include "ToneMatchComparison.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tonematch
{
namespace
{
double clamp01 (double value) noexcept
{
    return juce::jlimit (0.0, 1.0, value);
}
}

bool ToneMatchComparison::validateOptions (
    const Options& options,
    juce::String& error)
{
    if (options.minimumFrequencyHz <= 0.0
        || options.maximumFrequencyHz <= options.minimumFrequencyHz)
    {
        error = "Invalid comparison frequency range";
        return false;
    }

    if (options.outputPointCount < 16
        || options.outputPointCount > 16384)
    {
        error = "Invalid comparison point count";
        return false;
    }

    if (options.levelReferenceMinimumHz < options.minimumFrequencyHz
        || options.levelReferenceMaximumHz
               > options.maximumFrequencyHz
        || options.levelReferenceMaximumHz
               <= options.levelReferenceMinimumHz)
    {
        error = "Invalid level reference range";
        return false;
    }

    return true;
}

double ToneMatchComparison::interpolate (
    const std::vector<double>& frequencyHz,
    const std::vector<double>& values,
    double requestedFrequencyHz)
{
    if (frequencyHz.empty()
        || values.empty()
        || frequencyHz.size() != values.size())
    {
        return 0.0;
    }

    if (requestedFrequencyHz <= frequencyHz.front())
        return values.front();

    if (requestedFrequencyHz >= frequencyHz.back())
        return values.back();

    const auto upper = std::lower_bound (
        frequencyHz.begin(),
        frequencyHz.end(),
        requestedFrequencyHz);

    if (upper == frequencyHz.end())
        return values.back();

    const auto upperIndex =
        static_cast<std::size_t> (
            std::distance (frequencyHz.begin(), upper));

    if (upperIndex == 0)
        return values.front();

    const auto lowerIndex = upperIndex - 1;

    const auto lowerFrequency =
        frequencyHz[lowerIndex];

    const auto upperFrequency =
        frequencyHz[upperIndex];

    const auto frequencyDistance =
        upperFrequency - lowerFrequency;

    if (frequencyDistance <= 0.0)
        return values[lowerIndex];

    const auto alpha =
        juce::jlimit (
            0.0,
            1.0,
            (requestedFrequencyHz - lowerFrequency)
                / frequencyDistance);

    return values[lowerIndex]
        + alpha
            * (values[upperIndex] - values[lowerIndex]);
}

ToneMatchComparisonResult ToneMatchComparison::compare (
    const ToneAnalysisProfile& source,
    const ToneAnalysisProfile& target,
    const Options& options) const
{
    ToneMatchComparisonResult result;

    juce::String optionsError;

    if (!validateOptions (options, optionsError))
    {
        result.message = optionsError;
        return result;
    }

    if (!source.isValid())
    {
        result.message = "SOURCE analysis profile is invalid";
        return result;
    }

    if (!target.isValid())
    {
        result.message = "TARGET analysis profile is invalid";
        return result;
    }

    const auto pointCount = options.outputPointCount;

    result.frequencyHz.reserve (
        static_cast<std::size_t> (pointCount));

    result.rawCorrectionDb.reserve (
        static_cast<std::size_t> (pointCount));

    result.levelAlignedCorrectionDb.reserve (
        static_cast<std::size_t> (pointCount));

    result.confidence.reserve (
        static_cast<std::size_t> (pointCount));

    const auto logMinimum =
        std::log (options.minimumFrequencyHz);

    const auto logMaximum =
        std::log (options.maximumFrequencyHz);

    /*
        Primera pasada:
        - crear una rejilla común logarítmica;
        - interpolar SOURCE y TARGET;
        - calcular TARGET - SOURCE;
        - calcular confianza conjunta.
    */
    for (int point = 0; point < pointCount; ++point)
    {
        const auto normalisedPosition =
            pointCount > 1
                ? static_cast<double> (point)
                    / static_cast<double> (pointCount - 1)
                : 0.0;

        const auto frequency =
            std::exp (
                logMinimum
                + normalisedPosition
                    * (logMaximum - logMinimum));

        const auto sourceDb =
            interpolate (
                source.frequencyHz,
                source.spectrumDb,
                frequency);

        const auto targetDb =
            interpolate (
                target.frequencyHz,
                target.spectrumDb,
                frequency);

        const auto sourceConfidence =
            interpolate (
                source.frequencyHz,
                source.confidence,
                frequency);

        const auto targetConfidence =
            interpolate (
                target.frequencyHz,
                target.confidence,
                frequency);

        const auto jointConfidence =
            clamp01 (
                std::min (
                    sourceConfidence,
                    targetConfidence));

        result.frequencyHz.push_back (frequency);

        // Corrección necesaria para transformar SOURCE en TARGET.
        // La futura IR se aplicará sobre SOURCE.
        result.rawCorrectionDb.push_back (
            targetDb - sourceDb);

        result.confidence.push_back (
            jointConfidence);
    }

    /*
        No se aplica compensación automática de nivel.

        La curva RAW contiene la corrección completa necesaria:
        TARGET - SOURCE, incluida cualquier diferencia global
        de nivel entre ambas capturas.
    */
    result.removedLevelOffsetDb = 0.0;

    result.minimumCorrectionDb =
        std::numeric_limits<double>::max();

    result.maximumCorrectionDb =
        std::numeric_limits<double>::lowest();

    for (const auto rawCorrection :
         result.rawCorrectionDb)
    {
        // Se conserva por compatibilidad con la estructura actual,
        // pero contiene exactamente la misma curva RAW.
        result.levelAlignedCorrectionDb.push_back (
            rawCorrection);

        result.minimumCorrectionDb =
            std::min (
                result.minimumCorrectionDb,
                rawCorrection);

        result.maximumCorrectionDb =
            std::max (
                result.maximumCorrectionDb,
                rawCorrection);
    }

    if (source.captureSampleRate
        != target.captureSampleRate)
    {
        result.warning =
            "SOURCE and TARGET were captured at different sample rates";
    }
    else if (source.spectralCoverage < 0.35
             || target.spectralCoverage < 0.35)
    {
        result.warning =
            "One or both captures have limited spectral coverage";
    }

    result.success = true;
    result.message = "Comparison completed";

    return result;
}
}