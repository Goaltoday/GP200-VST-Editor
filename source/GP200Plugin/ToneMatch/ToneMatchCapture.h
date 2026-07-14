#pragma once

#include "ToneMatchTypes.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace tonematch
{
class ToneMatchCapture
{
  public:
    ToneMatchCapture() = default;

    void prepare (double newSampleRate,
                  int maximumBlockSize,
                  int inputChannelCount);

    void releaseResources();

    bool start (CaptureRole role);
    ToneCaptureData stop();

    void clear();

    void pushAudioBlock (
        const juce::AudioBuffer<float>& buffer) noexcept;

    bool isCapturing() const noexcept;
    CaptureState getState() const noexcept;
    CaptureRole getCurrentRole() const noexcept;

    double getCapturedDurationSeconds() const noexcept;
    float getCurrentPeakLinear() const noexcept;

  private:
    static constexpr double maximumCaptureDurationSeconds = 300.0;

    double sampleRate{0.0};
    int configuredInputChannelCount{0};

    std::vector<float> captureStorage;
    std::atomic<std::int64_t> capturedSamples{0};

    std::atomic<bool> capturing{false};
    std::atomic<CaptureState> state{CaptureState::empty};

    CaptureRole currentRole{CaptureRole::source};

    std::atomic<float> maximumPeakLinear{0.0f};
    std::atomic<double> sumOfSquares{0.0};
    std::atomic<std::uint64_t> accumulatedSampleCount{0};

    std::atomic<bool> clippingDetected{false};
};
}