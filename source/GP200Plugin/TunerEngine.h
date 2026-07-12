#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <complex>
#include <vector>
#include <array>

struct TunerResult
{
    float frequencyHz{0.0f};
    float cents{0.0f};
    float confidence{0.0f};
    float levelDb{-100.0f};
    int midiNote{-1};
    bool valid{false};
};

class TunerEngine final : private juce::Thread
{
public:
    TunerEngine();
    ~TunerEngine() override;

    void prepare(double newSampleRate);
    void reset();

    void setEnabled(bool shouldBeEnabled) noexcept;
    bool isEnabled() const noexcept;

    // Se llama desde processBlock(). Solo copia muestras al FIFO.
    void process(const juce::AudioBuffer<float>& buffer);

    TunerResult getResult() const noexcept;

private:
    void run() override;

    void consumeSamples(const float* samples, int numSamples);
    void copyHistoryToAnalysisBuffer();
    void analyseCurrentWindow();
	float refinePeakLag(int selectedLag) const noexcept;

    void publishInvalidResult(float levelDb) noexcept;
    void publishResult(float frequencyHz,
                       float cents,
                       float confidence,
                       float levelDb,
                       int midiNote) noexcept;
					   
					    static constexpr int smoothingWindowSize = 5;
					   
	void resetSmoothing() noexcept;

static float calculateMedian(
    const std::array<float, smoothingWindowSize>& values,
    int valueCount);

    static constexpr int analysisSize = 8192;

    // La autocorrelación lineal necesita padding a 2N.
    static constexpr int fftOrder = 14;
    static constexpr int fftSize = 1 << fftOrder;

    // Un análisis cada 4096 muestras:
    // unos 11.7 análisis/segundo a 48 kHz.
    static constexpr int hopSize = 4096;

    static constexpr int fifoCapacity = 32768;

    double sampleRate{44100.0};

    juce::dsp::FFT fft{fftOrder};

    juce::AbstractFifo audioFifo{fifoCapacity};
    std::vector<float> fifoBuffer;

    std::vector<float> historyBuffer;
    int historyWritePosition{0};
    int historySampleCount{0};
    int samplesSinceLastAnalysis{0};

    std::vector<float> analysisBuffer;
    std::vector<float> windowBuffer;
    std::vector<float> nsdfBuffer;
    std::vector<double> squarePrefix;

    std::vector<std::complex<float>> fftInput;
    std::vector<std::complex<float>> fftOutput;
    std::vector<std::complex<float>> powerSpectrum;
    std::vector<std::complex<float>> autocorrelation;

    std::atomic<bool> enabled{false};
    std::atomic<bool> resetRequested{false};
	
	

std::array<float, smoothingWindowSize> frequencyHistory{};
std::array<float, smoothingWindowSize> centsHistory{};

int smoothingWritePosition{0};
int smoothingSampleCount{0};
int smoothingMidiNote{-1};

    std::atomic<float> resultFrequencyHz{0.0f};
    std::atomic<float> resultCents{0.0f};
    std::atomic<float> resultConfidence{0.0f};
    std::atomic<float> resultLevelDb{-100.0f};
    std::atomic<int> resultMidiNote{-1};
    std::atomic<bool> resultValid{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerEngine)
};