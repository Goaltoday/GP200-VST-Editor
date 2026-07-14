#include "ToneAnalysis.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace tonematch
{
namespace
{
constexpr double negativeInfinityDb = -160.0;
constexpr double madToSigma = 1.4826;

double linearPowerToDb (
    double power,
    double powerFloor) noexcept
{
    return 10.0 * std::log10 (
        std::max (power, powerFloor));
}

double calculateFrameRms (
    const float* samples,
    int numSamples) noexcept
{
    if (samples == nullptr || numSamples <= 0)
        return 0.0;

    double sumOfSquares = 0.0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value =
            static_cast<double> (samples[sample]);

        sumOfSquares += value * value;
    }

    return std::sqrt (
        sumOfSquares /
        static_cast<double> (numSamples));
}

bool frameContainsClipping (
    const float* samples,
    int numSamples,
    float clippingThreshold) noexcept
{
    if (samples == nullptr || numSamples <= 0)
        return false;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (std::abs (samples[sample]) >= clippingThreshold)
            return true;
    }

    return false;
}

double clamp01 (double value) noexcept
{
    return juce::jlimit (0.0, 1.0, value);
}

double calculateMedian (
    std::vector<double>& values)
{
    if (values.empty())
        return 0.0;

    const auto middle =
        values.begin()
        + static_cast<std::ptrdiff_t> (
            values.size() / 2);

    std::nth_element (
        values.begin(),
        middle,
        values.end());

    return *middle;
}

double calculateMedianAbsoluteDeviation (
    const std::vector<double>& values,
    double median)
{
    if (values.empty())
        return 0.0;

    std::vector<double> absoluteDeviations;
    absoluteDeviations.reserve (values.size());

    for (const auto value : values)
    {
        absoluteDeviations.push_back (
            std::abs (value - median));
    }

    return calculateMedian (
        absoluteDeviations);
}
}

bool ToneAnalysis::validateOptions (
    const Options& options,
    juce::String& errorMessage)
{
    if (options.fftOrder < 8 || options.fftOrder > 18)
    {
        errorMessage =
            "FFT order must be between 8 and 18";
        return false;
    }

    if (options.overlapRatio < 0.0 ||
        options.overlapRatio >= 1.0)
    {
        errorMessage =
            "Overlap ratio must be at least 0 and below 1";
        return false;
    }

    if (options.clippingThreshold <= 0.0f ||
        options.clippingThreshold > 1.0f)
    {
        errorMessage =
            "Invalid clipping threshold";
        return false;
    }

    if (options.minimumFrequencyHz < 0.0 ||
        options.maximumFrequencyHz <=
            options.minimumFrequencyHz)
    {
        errorMessage =
            "Invalid analysis frequency range";
        return false;
    }

    if (options.powerFloor <= 0.0)
    {
        errorMessage =
            "Power floor must be greater than zero";
        return false;
    }

    if (options.robustGroupCount < 3 ||
        options.robustGroupCount > 31 ||
        (options.robustGroupCount % 2) == 0)
    {
        errorMessage =
            "Robust group count must be an odd number between 3 and 31";
        return false;
    }

    if (options.confidenceReferenceDeviationDb <= 0.0)
    {
        errorMessage =
            "Confidence reference deviation must be greater than zero";
        return false;
    }

    return true;
}

ToneAnalysisProfile ToneAnalysis::analyse (
    const ToneCaptureData& capture,
    const Options& options) const
{
    ToneAnalysisProfile profile;

    profile.captureSampleRate = capture.sampleRate;
    profile.peakDb = capture.peakDb;
    profile.rmsDb = capture.rmsDb;

    juce::String optionsError;

    if (!validateOptions (options, optionsError))
    {
        profile.warning = optionsError;
        return profile;
    }

    if (!capture.isValid())
    {
        profile.warning =
            capture.errorMessage.isNotEmpty()
                ? capture.errorMessage
                : "The captured audio is not valid";

        return profile;
    }

    if (capture.audio.getNumChannels() != 1)
    {
        profile.warning =
            "Tone analysis requires a mono capture";
        return profile;
    }

    const auto sampleRate = capture.sampleRate;
    const auto totalSamples =
        capture.audio.getNumSamples();

    const auto fftSize = 1 << options.fftOrder;
    const auto spectrumBinCount = (fftSize / 2) + 1;

    if (totalSamples < fftSize)
    {
        profile.warning =
            "The capture is too short for analysis";
        return profile;
    }

    const auto hopSize = juce::jmax (
        1,
        static_cast<int> (
            std::round (
                static_cast<double> (fftSize) *
                (1.0 - options.overlapRatio))));

    const auto nyquist = sampleRate * 0.5;

    const auto minimumFrequency =
        juce::jlimit (
            0.0,
            nyquist,
            options.minimumFrequencyHz);

    const auto maximumFrequency =
        juce::jlimit (
            minimumFrequency,
            nyquist,
            options.maximumFrequencyHz);

    juce::dsp::FFT fft (options.fftOrder);

    juce::dsp::WindowingFunction<float> window (
        static_cast<std::size_t> (fftSize),
        juce::dsp::WindowingFunction<float>::hann,
        true);

    std::vector<float> frame (
        static_cast<std::size_t> (fftSize),
        0.0f);

    // performFrequencyOnlyForwardTransform necesita
    // espacio para 2 * fftSize floats.
    std::vector<float> fftBuffer (
        static_cast<std::size_t> (fftSize * 2),
        0.0f);

    const auto groupCount =
        static_cast<std::size_t> (
            options.robustGroupCount);

    /*
        Cada grupo acumula potencia lineal.

        Las ventanas se asignan de forma intercalada:
        frame 0 -> grupo 0
        frame 1 -> grupo 1
        ...
        frame N -> grupo N % groupCount

        Esto reparte las distintas partes de la interpretación
        entre todos los grupos.
    */
    std::vector<std::vector<double>> groupSpectrumSums (
        groupCount,
        std::vector<double> (
            static_cast<std::size_t> (spectrumBinCount),
            0.0));

    std::vector<std::uint64_t> groupFrameCounts (
        groupCount,
        0);

    const auto* source =
        capture.audio.getReadPointer (0);

    const auto silenceThresholdLinear =
        juce::Decibels::decibelsToGain (
            options.silenceThresholdDb);

    std::uint64_t acceptedFrames = 0;
    std::uint64_t rejectedSilentFrames = 0;
    std::uint64_t rejectedClippedFrames = 0;

    double usefulSampleTotal = 0.0;

    for (int frameStart = 0;
         frameStart + fftSize <= totalSamples;
         frameStart += hopSize)
    {
        const auto* frameSource =
            source + frameStart;

        const auto frameRms =
            calculateFrameRms (
                frameSource,
                fftSize);

        if (frameRms < silenceThresholdLinear)
        {
            ++rejectedSilentFrames;
            continue;
        }

        if (frameContainsClipping (
                frameSource,
                fftSize,
                options.clippingThreshold))
        {
            ++rejectedClippedFrames;
            continue;
        }

        std::copy_n (
            frameSource,
            fftSize,
            frame.begin());

        // Eliminación de DC por ventana.
        double frameMean = 0.0;

        for (const auto sample : frame)
            frameMean += static_cast<double> (sample);

        frameMean /= static_cast<double> (fftSize);

        for (auto& sample : frame)
            sample -= static_cast<float> (frameMean);

        window.multiplyWithWindowingTable (
            frame.data(),
            static_cast<std::size_t> (fftSize));

        std::fill (
            fftBuffer.begin(),
            fftBuffer.end(),
            0.0f);

        std::copy (
            frame.begin(),
            frame.end(),
            fftBuffer.begin());

        fft.performFrequencyOnlyForwardTransform (
            fftBuffer.data(),
            true);

        const auto groupIndex =
            static_cast<std::size_t> (
                acceptedFrames %
                static_cast<std::uint64_t> (
                    groupCount));

        auto& groupSpectrum =
            groupSpectrumSums[groupIndex];

        for (int bin = 0;
             bin < spectrumBinCount;
             ++bin)
        {
            const auto frequency =
                static_cast<double> (bin) *
                sampleRate /
                static_cast<double> (fftSize);

            if (frequency < minimumFrequency ||
                frequency > maximumFrequency)
            {
                continue;
            }

            const auto magnitude =
                static_cast<double> (
                    fftBuffer[
                        static_cast<std::size_t> (bin)]);

            const auto power =
                magnitude * magnitude;

            groupSpectrum[
                static_cast<std::size_t> (bin)]
                += power;
        }

        ++groupFrameCounts[groupIndex];
        ++acceptedFrames;

        // El tiempo útil se calcula con hopSize para no
        // contar repetidamente el solapamiento.
        usefulSampleTotal += static_cast<double> (hopSize);
    }

    profile.analysedFrameCount = acceptedFrames;
    profile.rejectedSilentFrames = rejectedSilentFrames;
    profile.rejectedClippedFrames = rejectedClippedFrames;

    profile.usefulDurationSeconds =
        usefulSampleTotal / sampleRate;

    if (acceptedFrames == 0)
    {
        profile.warning =
            "No usable audio frames were found";

        return profile;
    }

    const auto activeGroupCount =
        static_cast<std::size_t> (
            std::count_if (
                groupFrameCounts.begin(),
                groupFrameCounts.end(),
                [] (std::uint64_t count)
                {
                    return count > 0;
                }));

    if (activeGroupCount == 0)
    {
        profile.warning =
            "No robust spectrum groups were created";

        return profile;
    }

    std::vector<double> robustSpectrumDb (
        static_cast<std::size_t> (spectrumBinCount),
        negativeInfinityDb);

    std::vector<double> robustDeviationDb (
        static_cast<std::size_t> (spectrumBinCount),
        0.0);

    /*
        Robust Welch por bin:

        1. Cada grupo produce su media de potencia lineal.
        2. La media de cada grupo se convierte a dB.
        3. Se toma la mediana de las medias en dB.
        4. La dispersión se estima con MAD, no con varianza clásica.
    */
    for (int bin = 0;
         bin < spectrumBinCount;
         ++bin)
    {
        const auto frequency =
            static_cast<double> (bin) *
            sampleRate /
            static_cast<double> (fftSize);

        if (frequency < minimumFrequency ||
            frequency > maximumFrequency)
        {
            continue;
        }

        std::vector<double> groupMeans;
        groupMeans.reserve (activeGroupCount);

        const auto binIndex =
            static_cast<std::size_t> (bin);

        for (std::size_t group = 0;
             group < groupCount;
             ++group)
        {
            const auto frameCount =
                groupFrameCounts[group];

            if (frameCount == 0)
                continue;

            const auto meanPower =
                groupSpectrumSums[group][binIndex]
                / static_cast<double> (frameCount);

            groupMeans.push_back (
                linearPowerToDb (
                    meanPower,
                    options.powerFloor));
        }

        auto valuesForMedian = groupMeans;

        const auto medianDb =
            calculateMedian (
                valuesForMedian);

        const auto madDb =
            calculateMedianAbsoluteDeviation (
                groupMeans,
                medianDb);

        robustSpectrumDb[binIndex] =
            medianDb;

        robustDeviationDb[binIndex] =
            madToSigma * madDb;
    }

    double strongestSpectrumDb =
        negativeInfinityDb;

    for (int bin = 0;
         bin < spectrumBinCount;
         ++bin)
    {
        const auto frequency =
            static_cast<double> (bin) *
            sampleRate /
            static_cast<double> (fftSize);

        if (frequency < minimumFrequency ||
            frequency > maximumFrequency)
        {
            continue;
        }

        strongestSpectrumDb =
            std::max (
                strongestSpectrumDb,
                robustSpectrumDb[
                    static_cast<std::size_t> (bin)]);
    }

    profile.frequencyHz.reserve (
        static_cast<std::size_t> (spectrumBinCount));

    profile.spectrumDb.reserve (
        static_cast<std::size_t> (spectrumBinCount));

    profile.confidence.reserve (
        static_cast<std::size_t> (spectrumBinCount));

    profile.variance.reserve (
        static_cast<std::size_t> (spectrumBinCount));

    std::size_t confidentBinCount = 0;
    std::size_t includedBinCount = 0;

    for (int bin = 0;
         bin < spectrumBinCount;
         ++bin)
    {
        const auto frequency =
            static_cast<double> (bin) *
            sampleRate /
            static_cast<double> (fftSize);

        if (frequency < minimumFrequency ||
            frequency > maximumFrequency)
        {
            continue;
        }

        const auto index =
            static_cast<std::size_t> (bin);

        const auto spectrumDb =
            robustSpectrumDb[index];

        const auto deviationDb =
            robustDeviationDb[index];

        /*
            Confianza robusta:

            - aumenta con el número total de ventanas;
            - aumenta con la energía relativa del bin;
            - disminuye cuando los grupos no son consistentes;
            - aumenta cuando hay suficientes grupos activos.
        */
        const auto frameConfidence =
            clamp01 (
                static_cast<double> (
                    acceptedFrames) /
                32.0);

        const auto groupConfidence =
            clamp01 (
                static_cast<double> (
                    activeGroupCount) /
                static_cast<double> (
                    options.robustGroupCount));

        const auto energyConfidence =
            clamp01 (
                (spectrumDb
                 - strongestSpectrumDb
                 + 60.0) /
                60.0);

        const auto stabilityConfidence =
            1.0 /
            (1.0
             + deviationDb /
                 options.confidenceReferenceDeviationDb);

        const auto confidence =
            clamp01 (
                frameConfidence *
                groupConfidence *
                energyConfidence *
                stabilityConfidence);

        profile.frequencyHz.push_back (
            frequency);

        profile.spectrumDb.push_back (
            spectrumDb);

        profile.confidence.push_back (
            confidence);

        // El campo se conserva por compatibilidad.
        // Ahora representa dispersión robusta en dB².
        profile.variance.push_back (
            deviationDb * deviationDb);

        ++includedBinCount;

        if (confidence >= 0.25)
            ++confidentBinCount;
    }

    if (includedBinCount > 0)
    {
        profile.spectralCoverage =
            static_cast<double> (
                confidentBinCount) /
            static_cast<double> (
                includedBinCount);
    }

    if (rejectedClippedFrames > 0)
    {
        profile.warning =
            "Some clipped frames were ignored";
    }
    else if (profile.usefulDurationSeconds < 3.0)
    {
        profile.warning =
            "The useful capture is very short";
    }
    else if (activeGroupCount < 3)
    {
        profile.warning =
            "Too few groups for a robust spectrum estimate";
    }
    else if (profile.spectralCoverage < 0.35)
    {
        profile.warning =
            "The capture has limited spectral coverage";
    }

    return profile;
}
}
