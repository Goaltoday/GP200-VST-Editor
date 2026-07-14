#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <vector>

namespace tonematch
{
enum class CaptureRole
{
    source,
    target
};

enum class CaptureState
{
    empty,
    capturing,
    ready,
    error
};

struct ToneCaptureData
{
    CaptureRole role{CaptureRole::source};

    double sampleRate{0.0};
    int originalChannelCount{0};

    // Copia mono de la señal capturada.
    juce::AudioBuffer<float> audio;

    double durationSeconds{0.0};
    double peakDb{-100.0};
    double rmsDb{-100.0};

    bool wasClipped{false};
    juce::String errorMessage;

    bool isValid() const noexcept
    {
        return sampleRate > 0.0
            && audio.getNumChannels() == 1
            && audio.getNumSamples() > 0
            && errorMessage.isEmpty();
    }
};

struct ToneAnalysisProfile
{
    double captureSampleRate{0.0};
    double usefulDurationSeconds{0.0};

    std::vector<double> frequencyHz;
    std::vector<double> spectrumDb;
    std::vector<double> confidence;
    std::vector<double> variance;

    double peakDb{-100.0};
    double rmsDb{-100.0};
    double spectralCoverage{0.0};

    std::uint64_t analysedFrameCount{0};
    std::uint64_t rejectedSilentFrames{0};
    std::uint64_t rejectedClippedFrames{0};

    juce::String warning;

    bool isValid() const noexcept
    {
        const auto size = frequencyHz.size();

        return size > 1
            && spectrumDb.size() == size
            && confidence.size() == size
            && variance.size() == size;
    }
};

struct ToneMatchOptions
{
    // Formato obligatorio del resultado para GP-200.
    double outputSampleRate{44100.0};
    int outputLengthSamples{1024};

    double matchAmount{0.80};

    double maximumBoostDb{9.0};
    double maximumCutDb{-15.0};

    double minimumFrequencyHz{40.0};
    double correctionFadeOutStartHz{18000.0};
    double maximumFrequencyHz{22000.0};

    // SolverV1 comenzará con suavizado natural de 1/6 de octava.
    double smoothingFractionOfOctave{1.0 / 6.0};
};

struct ToneMatchResult
{
    bool success{false};
    juce::String message;
    juce::String warning;

    juce::AudioBuffer<float> impulseResponse;
    double impulseResponseSampleRate{44100.0};

    std::vector<double> frequencyHz;
    std::vector<double> rawCorrectionDb;
    std::vector<double> smoothedCorrectionDb;
    std::vector<double> achievedCorrectionDb;
    std::vector<double> confidence;

    double errorBeforeDb{0.0};
    double errorAfterDb{0.0};
    double overallConfidence{0.0};

    bool hasValidImpulseResponse() const noexcept
    {
        return success
            && impulseResponseSampleRate == 44100.0
            && impulseResponse.getNumChannels() == 1
            && impulseResponse.getNumSamples() == 1024;
    }
};
}