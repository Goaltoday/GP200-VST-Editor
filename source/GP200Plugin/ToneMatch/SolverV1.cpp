#include "SolverV1.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace tonematch
{
namespace
{
constexpr double minimumMagnitude = 1.0e-12;
constexpr double ln10Over20 = 0.11512925464970229;
constexpr double outputGainDb = 12.0;

int nextPowerOfTwoOrder (int requiredSize)
{
    int order = 0;
    int size = 1;

    while (size < requiredSize)
    {
        size <<= 1;
        ++order;
    }

    return order;
}

}

juce::String SolverV1::getName() const
{
    return "SolverV1";
}

juce::String SolverV1::getDescription() const
{
    return "Minimum-phase 1024-sample tone-match IR";
}

bool SolverV1::validateOptions (
    const ToneMatchOptions& options,
    juce::String& errorMessage)
{
    if (std::abs (options.outputSampleRate - 44100.0) > 0.01)
    {
        errorMessage = "SolverV1 requires a 44.1 kHz output sample rate";
        return false;
    }

    if (options.outputLengthSamples != 1024)
    {
        errorMessage = "SolverV1 requires an output length of 1024 samples";
        return false;
    }

    if (options.matchAmount < 0.0 || options.matchAmount > 1.0)
    {
        errorMessage = "Match amount must be between 0 and 1";
        return false;
    }

    if (options.maximumCutDb >= options.maximumBoostDb)
    {
        errorMessage = "Invalid correction limits";
        return false;
    }

    if (options.minimumFrequencyHz <= 0.0
        || options.correctionFadeOutStartHz <= options.minimumFrequencyHz
        || options.maximumFrequencyHz <= options.correctionFadeOutStartHz)
    {
        errorMessage = "Invalid correction frequency range";
        return false;
    }

    if (options.smoothingFractionOfOctave <= 0.0
        || options.smoothingFractionOfOctave > 2.0)
    {
        errorMessage = "Invalid smoothing fraction";
        return false;
    }

    return true;
}

double SolverV1::interpolate (
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
    const auto lowerFrequency = frequencyHz[lowerIndex];
    const auto upperFrequency = frequencyHz[upperIndex];
    const auto distance = upperFrequency - lowerFrequency;

    if (distance <= 0.0)
        return values[lowerIndex];

    const auto alpha = juce::jlimit (
        0.0,
        1.0,
        (requestedFrequencyHz - lowerFrequency) / distance);

    return values[lowerIndex]
        + alpha * (values[upperIndex] - values[lowerIndex]);
}

ToneMatchResult SolverV1::solve (
    const ToneMatchComparisonResult& comparison,
    const ToneMatchOptions& options)
{
    ToneMatchResult result;

    juce::String optionsError;

    if (!validateOptions (options, optionsError))
    {
        result.message = optionsError;
        return result;
    }

    if (!comparison.isValid())
    {
        result.message = "Tone-match comparison is invalid";
        return result;
    }

    // La comparación ya contiene la única curva RAW canónica.
    // El solver no vuelve a calcular TARGET - SOURCE.
    result.frequencyHz = comparison.frequencyHz;
    result.rawCorrectionDb = comparison.rawCorrectionDb;
    result.smoothedCorrectionDb = comparison.rawCorrectionDb;
    result.confidence = comparison.confidence;

    const auto fftOrder = nextPowerOfTwoOrder (
        options.outputLengthSamples * 2);
    const auto fftSize = 1 << fftOrder;
    const auto positiveBinCount = (fftSize / 2) + 1;

    juce::dsp::FFT fft (fftOrder);

    std::vector<std::complex<float>> logSpectrum (
        static_cast<std::size_t> (fftSize));
    std::vector<std::complex<float>> cepstrum (
        static_cast<std::size_t> (fftSize));
    std::vector<std::complex<float>> minimumPhaseCepstrum (
        static_cast<std::size_t> (fftSize));
    std::vector<std::complex<float>> complexLogSpectrum (
        static_cast<std::size_t> (fftSize));
    std::vector<std::complex<float>> minimumPhaseSpectrum (
        static_cast<std::size_t> (fftSize));
    std::vector<std::complex<float>> impulseComplex (
        static_cast<std::size_t> (fftSize));

    for (int bin = 0; bin < positiveBinCount; ++bin)
    {
        const auto frequency =
            static_cast<double> (bin)
            * options.outputSampleRate
            / static_cast<double> (fftSize);

        // Conserva la curva RAW también en los extremos:
        // por debajo del primer punto usa el primer valor y por encima
        // del último punto mantiene el último valor, sin volver a 0 dB.
        const auto correctionDb = interpolate (
            result.frequencyHz,
            result.smoothedCorrectionDb,
            frequency);

        const auto logMagnitude = correctionDb * ln10Over20;
        logSpectrum[static_cast<std::size_t> (bin)] =
            { static_cast<float> (logMagnitude), 0.0f };

        if (bin > 0 && bin < fftSize / 2)
        {
            logSpectrum[static_cast<std::size_t> (fftSize - bin)] =
                { static_cast<float> (logMagnitude), 0.0f };
        }
    }

    fft.perform (
        logSpectrum.data(),
        cepstrum.data(),
        true);

    minimumPhaseCepstrum[0] = cepstrum[0];

    for (int index = 1; index < fftSize / 2; ++index)
    {
        minimumPhaseCepstrum[static_cast<std::size_t> (index)] =
            cepstrum[static_cast<std::size_t> (index)] * 2.0f;
    }

    minimumPhaseCepstrum[static_cast<std::size_t> (fftSize / 2)] =
        cepstrum[static_cast<std::size_t> (fftSize / 2)];

    fft.perform (
        minimumPhaseCepstrum.data(),
        complexLogSpectrum.data(),
        false);

    for (int index = 0; index < fftSize; ++index)
    {
        minimumPhaseSpectrum[static_cast<std::size_t> (index)] =
            std::exp (complexLogSpectrum[static_cast<std::size_t> (index)]);
    }

    fft.perform (
        minimumPhaseSpectrum.data(),
        impulseComplex.data(),
        true);

    result.impulseResponse.setSize (
        1,
        options.outputLengthSamples,
        false,
        true,
        false);

    auto* impulse = result.impulseResponse.getWritePointer (0);

    for (int sample = 0;
         sample < options.outputLengthSamples;
         ++sample)
    {
        impulse[sample] = impulseComplex[
            static_cast<std::size_t> (sample)].real();
    }
	
	
	// Ganancia global de salida para aproximar el nivel práctico
// de una IR comercial. No cambia la forma tonal de la curva RAW.
const auto outputGain =
    static_cast<float> (
        std::pow (10.0, outputGainDb / 20.0));

for (int sample = 0;
     sample < options.outputLengthSamples;
     ++sample)
{
    impulse[sample] *= outputGain;
}
	
	

    result.impulseResponseSampleRate = options.outputSampleRate;

    // Medición diagnóstica de la respuesta realmente conseguida después
    // de truncar a 1024 muestras. Esta medición NO modifica la ganancia.
    std::vector<std::complex<float>> measuredInput (
        static_cast<std::size_t> (fftSize));
    std::vector<std::complex<float>> measuredSpectrum (
        static_cast<std::size_t> (fftSize));

    for (int sample = 0;
         sample < options.outputLengthSamples;
         ++sample)
    {
        measuredInput[static_cast<std::size_t> (sample)] =
            { impulse[sample], 0.0f };
    }

    fft.perform (
        measuredInput.data(),
        measuredSpectrum.data(),
        false);

    std::vector<double> measuredFrequency;
    std::vector<double> measuredDb;
    measuredFrequency.reserve (positiveBinCount);
    measuredDb.reserve (positiveBinCount);

    for (int bin = 0; bin < positiveBinCount; ++bin)
    {
        const auto frequency =
            static_cast<double> (bin)
            * options.outputSampleRate
            / static_cast<double> (fftSize);

        const auto magnitude = std::max (
            static_cast<double> (
                std::abs (
                    measuredSpectrum[static_cast<std::size_t> (bin)])),
            minimumMagnitude);

        measuredFrequency.push_back (frequency);
        measuredDb.push_back (20.0 * std::log10 (magnitude));
    }

    result.achievedCorrectionDb.reserve (
        result.frequencyHz.size());

    double weightedErrorBefore = 0.0;
    double weightedErrorAfter = 0.0;
    double confidenceSum = 0.0;

    for (std::size_t index = 0;
         index < result.frequencyHz.size();
         ++index)
    {
        const auto achievedDb = interpolate (
    measuredFrequency,
    measuredDb,
    result.frequencyHz[index]);

result.achievedCorrectionDb.push_back (achievedDb);

const auto confidence = result.confidence[index];
const auto raw = result.rawCorrectionDb[index];

// La IR final incluye +12 dB de ganancia práctica de salida.
// Para calcular el error tonal, eliminamos ese desplazamiento
// global y comparamos únicamente la forma del filtro.
const auto achievedForErrorDb =
    achievedDb - outputGainDb;

const auto residual =
    raw - achievedForErrorDb;

        weightedErrorBefore += raw * raw * confidence;
        weightedErrorAfter += residual * residual * confidence;
        confidenceSum += confidence;
    }

    if (confidenceSum > 0.0)
    {
        result.errorBeforeDb = std::sqrt (
            weightedErrorBefore / confidenceSum);

        result.errorAfterDb = std::sqrt (
            weightedErrorAfter / confidenceSum);

        result.overallConfidence =
            confidenceSum
            / static_cast<double> (result.confidence.size());
    }

    const auto peak = result.impulseResponse.getMagnitude (
        0,
        options.outputLengthSamples);

    if (peak > 1.0f)
    {
        result.warning =
            "Generated IR peak is above 0 dBFS; export as 32-bit float";
    }

    result.success = true;
    result.message = "Tone-match IR generated";

    return result;
}
}
