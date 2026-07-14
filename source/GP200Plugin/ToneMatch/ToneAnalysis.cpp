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

    std::vector<double> meanPower (
        static_cast<std::size_t> (spectrumBinCount),
        0.0);

    std::vector<double> powerM2 (
        static_cast<std::size_t> (spectrumBinCount),
        0.0);

    std::vector<std::uint64_t> binFrameCount (
        static_cast<std::size_t> (spectrumBinCount),
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

            auto& count =
                binFrameCount[
                    static_cast<std::size_t> (bin)];

            auto& mean =
                meanPower[
                    static_cast<std::size_t> (bin)];

            auto& m2 =
                powerM2[
                    static_cast<std::size_t> (bin)];

            ++count;

            const auto delta = power - mean;
            mean += delta / static_cast<double> (count);

            const auto deltaAfterMean = power - mean;
            m2 += delta * deltaAfterMean;
        }

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

    profile.frequencyHz.reserve (
        static_cast<std::size_t> (spectrumBinCount));

    profile.spectrumDb.reserve (
        static_cast<std::size_t> (spectrumBinCount));

    profile.confidence.reserve (
        static_cast<std::size_t> (spectrumBinCount));

    profile.variance.reserve (
        static_cast<std::size_t> (spectrumBinCount));

    double strongestPower = options.powerFloor;

    for (int bin = 0; bin < spectrumBinCount; ++bin)
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

        strongestPower = std::max (
            strongestPower,
            meanPower[
                static_cast<std::size_t> (bin)]);
    }

    std::size_t confidentBinCount = 0;
    std::size_t includedBinCount = 0;

    for (int bin = 0; bin < spectrumBinCount; ++bin)
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

        const auto count = binFrameCount[index];
        const auto averagePower =
            std::max (
                meanPower[index],
                options.powerFloor);

        const auto variance =
            count > 1
                ? powerM2[index] /
                    static_cast<double> (count - 1)
                : 0.0;

        const auto spectrumDb =
            linearPowerToDb (
                averagePower,
                options.powerFloor);

        const auto relativePower =
            averagePower /
            std::max (
                strongestPower,
                options.powerFloor);

        /*
            Confianza inicial:

            - aumenta con el número de ventanas;
            - aumenta con la energía relativa;
            - disminuye si la potencia es muy inestable.

            Es deliberadamente conservadora. Más adelante
            podremos sustituir esta fórmula sin cambiar el
            formato ToneAnalysisProfile.
        */
        const auto frameConfidence =
            clamp01 (
                static_cast<double> (count) / 32.0);

        const auto energyConfidence =
            clamp01 (
                (linearPowerToDb (
                     relativePower,
                     options.powerFloor) +
                 60.0) /
                60.0);

        const auto normalizedVariance =
            variance /
            std::max (
                averagePower * averagePower,
                options.powerFloor);

        const auto stabilityConfidence =
            1.0 /
            (1.0 + normalizedVariance);

        const auto confidence =
            clamp01 (
                frameConfidence *
                energyConfidence *
                stabilityConfidence);

        profile.frequencyHz.push_back (frequency);
        profile.spectrumDb.push_back (spectrumDb);
        profile.confidence.push_back (confidence);
        profile.variance.push_back (variance);

        ++includedBinCount;

        if (confidence >= 0.25)
            ++confidentBinCount;
    }

    if (includedBinCount > 0)
    {
        profile.spectralCoverage =
            static_cast<double> (confidentBinCount) /
            static_cast<double> (includedBinCount);
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
    else if (profile.spectralCoverage < 0.35)
    {
        profile.warning =
            "The capture has limited spectral coverage";
    }

    return profile;
}
}