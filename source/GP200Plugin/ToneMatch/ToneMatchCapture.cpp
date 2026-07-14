#include "ToneMatchCapture.h"

#include <algorithm>
#include <cmath>

namespace tonematch
{
void ToneMatchCapture::prepare (double newSampleRate,
                                int maximumBlockSize,
                                int inputChannelCount)
{
    juce::ignoreUnused (maximumBlockSize);

    capturing.store (false, std::memory_order_release);

    sampleRate = newSampleRate;
    configuredInputChannelCount = juce::jmax (1, inputChannelCount);

    const auto maximumSamples = static_cast<std::size_t> (
        std::ceil (sampleRate * maximumCaptureDurationSeconds));

    captureStorage.assign (maximumSamples, 0.0f);

    clear();
}

void ToneMatchCapture::releaseResources()
{
    capturing.store (false, std::memory_order_release);

    captureStorage.clear();
    captureStorage.shrink_to_fit();

    sampleRate = 0.0;
    configuredInputChannelCount = 0;

    clear();
}

bool ToneMatchCapture::start (CaptureRole role)
{
    if (sampleRate <= 0.0 || captureStorage.empty())
    {
        state.store (CaptureState::error, std::memory_order_release);
        return false;
    }

    capturing.store (false, std::memory_order_release);

    currentRole = role;

    capturedSamples.store (0, std::memory_order_release);
    maximumPeakLinear.store (0.0f, std::memory_order_release);
    sumOfSquares.store (0.0, std::memory_order_release);
    accumulatedSampleCount.store (0, std::memory_order_release);
    clippingDetected.store (false, std::memory_order_release);

    std::fill (captureStorage.begin(), captureStorage.end(), 0.0f);

    state.store (CaptureState::capturing, std::memory_order_release);
    capturing.store (true, std::memory_order_release);

    return true;
}

ToneCaptureData ToneMatchCapture::stop()
{
    capturing.store (false, std::memory_order_release);

    ToneCaptureData result;
    result.role = currentRole;
    result.sampleRate = sampleRate;
    result.originalChannelCount = configuredInputChannelCount;

    const auto sampleCount = juce::jlimit<std::int64_t> (
        0,
        static_cast<std::int64_t> (captureStorage.size()),
        capturedSamples.load (std::memory_order_acquire));

    if (sampleCount <= 0 || sampleRate <= 0.0)
    {
        result.errorMessage = "No audio was captured";
        state.store (CaptureState::error, std::memory_order_release);
        return result;
    }

    result.audio.setSize (
        1,
        static_cast<int> (sampleCount),
        false,
        true,
        false);

    std::copy_n (
        captureStorage.data(),
        static_cast<std::size_t> (sampleCount),
        result.audio.getWritePointer (0));

    result.durationSeconds =
        static_cast<double> (sampleCount) / sampleRate;

    const auto peak =
        maximumPeakLinear.load (std::memory_order_acquire);

    const auto squareSum =
        sumOfSquares.load (std::memory_order_acquire);

    const auto rmsSampleCount =
        accumulatedSampleCount.load (std::memory_order_acquire);

    const auto rmsLinear =
        rmsSampleCount > 0
            ? std::sqrt (
                  squareSum /
                  static_cast<double> (rmsSampleCount))
            : 0.0;

    result.peakDb = juce::Decibels::gainToDecibels (
        static_cast<double> (peak),
        -100.0);

    result.rmsDb = juce::Decibels::gainToDecibels (
        rmsLinear,
        -100.0);

    result.wasClipped =
        clippingDetected.load (std::memory_order_acquire);

    state.store (CaptureState::ready, std::memory_order_release);

    return result;
}

void ToneMatchCapture::clear()
{
    capturing.store (false, std::memory_order_release);

    capturedSamples.store (0, std::memory_order_release);
    maximumPeakLinear.store (0.0f, std::memory_order_release);
    sumOfSquares.store (0.0, std::memory_order_release);
    accumulatedSampleCount.store (0, std::memory_order_release);
    clippingDetected.store (false, std::memory_order_release);

    state.store (CaptureState::empty, std::memory_order_release);
}

void ToneMatchCapture::pushAudioBlock (
    const juce::AudioBuffer<float>& buffer) noexcept
{
    if (!capturing.load (std::memory_order_acquire))
        return;

    const auto inputChannels = buffer.getNumChannels();
    const auto blockSamples = buffer.getNumSamples();

    if (inputChannels <= 0 || blockSamples <= 0)
        return;

    const auto startSample =
        capturedSamples.load (std::memory_order_relaxed);

    const auto remainingCapacity =
        static_cast<std::int64_t> (captureStorage.size())
        - startSample;

    const auto samplesToWrite = static_cast<int> (
        juce::jlimit<std::int64_t> (
            0,
            blockSamples,
            remainingCapacity));

    if (samplesToWrite <= 0)
    {
        capturing.store (false, std::memory_order_release);
        state.store (CaptureState::ready, std::memory_order_release);
        return;
    }

    auto peak = maximumPeakLinear.load (std::memory_order_relaxed);
    auto squareSum = sumOfSquares.load (std::memory_order_relaxed);

    for (int sample = 0; sample < samplesToWrite; ++sample)
    {
        double monoValue = 0.0;

        for (int channel = 0; channel < inputChannels; ++channel)
            monoValue += buffer.getSample (channel, sample);

        monoValue /= static_cast<double> (inputChannels);

        const auto value = static_cast<float> (monoValue);

        captureStorage[
            static_cast<std::size_t> (startSample + sample)] = value;

        const auto absoluteValue = std::abs (value);

        peak = juce::jmax (peak, absoluteValue);
        squareSum += static_cast<double> (value)
                   * static_cast<double> (value);

        if (absoluteValue >= 0.999f)
            clippingDetected.store (true, std::memory_order_relaxed);
    }

    maximumPeakLinear.store (peak, std::memory_order_relaxed);
    sumOfSquares.store (squareSum, std::memory_order_relaxed);

    accumulatedSampleCount.fetch_add (
        static_cast<std::uint64_t> (samplesToWrite),
        std::memory_order_relaxed);

    capturedSamples.store (
        startSample + samplesToWrite,
        std::memory_order_release);
}

bool ToneMatchCapture::isCapturing() const noexcept
{
    return capturing.load (std::memory_order_acquire);
}

CaptureState ToneMatchCapture::getState() const noexcept
{
    return state.load (std::memory_order_acquire);
}

CaptureRole ToneMatchCapture::getCurrentRole() const noexcept
{
    return currentRole;
}

double ToneMatchCapture::getCapturedDurationSeconds() const noexcept
{
    if (sampleRate <= 0.0)
        return 0.0;

    return static_cast<double> (
        capturedSamples.load (std::memory_order_acquire))
        / sampleRate;
}

float ToneMatchCapture::getCurrentPeakLinear() const noexcept
{
    return maximumPeakLinear.load (std::memory_order_acquire);
}
}