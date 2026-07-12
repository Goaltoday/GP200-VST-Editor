#include "TunerEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
constexpr float minimumFrequencyHz = 35.0f;
constexpr float maximumFrequencyHz = 500.0f;

constexpr float minimumSignalDb = -60.0f;
constexpr float minimumConfidence = 0.60f;

constexpr double referenceA4Hz = 440.0;
}

TunerEngine::TunerEngine()
    : juce::Thread("GP200 Studio Tuner")
{
}

TunerEngine::~TunerEngine()
{
    signalThreadShouldExit();
    notify();
    stopThread(1500);
}

void TunerEngine::prepare(double newSampleRate)
{
    signalThreadShouldExit();
    notify();
    stopThread(1500);

    sampleRate =
        newSampleRate > 0.0
            ? newSampleRate
            : 44100.0;

    fifoBuffer.assign(
        static_cast<std::size_t>(fifoCapacity),
        0.0f);

    historyBuffer.assign(
        static_cast<std::size_t>(analysisSize),
        0.0f);

    analysisBuffer.assign(
        static_cast<std::size_t>(analysisSize),
        0.0f);

    windowBuffer.assign(
        static_cast<std::size_t>(analysisSize),
        0.0f);

    nsdfBuffer.assign(
        static_cast<std::size_t>(analysisSize),
        0.0f);

    squarePrefix.assign(
        static_cast<std::size_t>(analysisSize + 1),
        0.0);

    fftInput.assign(
        static_cast<std::size_t>(fftSize),
        std::complex<float>{});

    fftOutput.assign(
        static_cast<std::size_t>(fftSize),
        std::complex<float>{});

    powerSpectrum.assign(
        static_cast<std::size_t>(fftSize),
        std::complex<float>{});

    autocorrelation.assign(
        static_cast<std::size_t>(fftSize),
        std::complex<float>{});

    audioFifo.reset();

    historyWritePosition = 0;
    historySampleCount = 0;
    samplesSinceLastAnalysis = 0;

    reset();

    startThread(juce::Thread::Priority::normal);
}

void TunerEngine::reset()
{
    resultFrequencyHz.store(0.0f);
    resultCents.store(0.0f);
    resultConfidence.store(0.0f);
    resultLevelDb.store(-100.0f);
    resultMidiNote.store(-1);
    resultValid.store(false);

    resetRequested.store(true);
    notify();
}

void TunerEngine::setEnabled(bool shouldBeEnabled) noexcept
{
    enabled.store(shouldBeEnabled);
    resetRequested.store(true);

    if (!shouldBeEnabled)
    {
        resultFrequencyHz.store(0.0f);
        resultCents.store(0.0f);
        resultConfidence.store(0.0f);
        resultLevelDb.store(-100.0f);
        resultMidiNote.store(-1);
        resultValid.store(false);
    }

    notify();
}

bool TunerEngine::isEnabled() const noexcept
{
    return enabled.load();
}

void TunerEngine::process(
    const juce::AudioBuffer<float>& buffer)
{
    if (!enabled.load())
        return;

    if (fifoBuffer.empty())
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    // Analizamos únicamente el canal izquierdo.
    const float* source = buffer.getReadPointer(0);

    int sourcePosition = 0;

    while (sourcePosition < numSamples)
    {
        int start1 = 0;
        int size1 = 0;
        int start2 = 0;
        int size2 = 0;

        const int samplesRemaining =
            numSamples - sourcePosition;

        audioFifo.prepareToWrite(
            samplesRemaining,
            start1,
            size1,
            start2,
            size2);

        const int totalWritable = size1 + size2;

        if (totalWritable <= 0)
            break;

        if (size1 > 0)
        {
            std::memcpy(
                fifoBuffer.data() + start1,
                source + sourcePosition,
                static_cast<std::size_t>(size1) *
                    sizeof(float));

            sourcePosition += size1;
        }

        if (size2 > 0)
        {
            std::memcpy(
                fifoBuffer.data() + start2,
                source + sourcePosition,
                static_cast<std::size_t>(size2) *
                    sizeof(float));

            sourcePosition += size2;
        }

        audioFifo.finishedWrite(totalWritable);
    }

    notify();
}

TunerResult TunerEngine::getResult() const noexcept
{
    TunerResult result;

    result.frequencyHz = resultFrequencyHz.load();
    result.cents = resultCents.load();
    result.confidence = resultConfidence.load();
    result.levelDb = resultLevelDb.load();
    result.midiNote = resultMidiNote.load();
    result.valid = resultValid.load();

    return result;
}

void TunerEngine::run()
{
    while (!threadShouldExit())
    {
        if (resetRequested.exchange(false))
        {
            historyWritePosition = 0;
            historySampleCount = 0;
            samplesSinceLastAnalysis = 0;

            std::fill(
                historyBuffer.begin(),
                historyBuffer.end(),
                0.0f);

            // Descarta muestras antiguas del FIFO.
            while (audioFifo.getNumReady() > 0)
            {
                int start1 = 0;
                int size1 = 0;
                int start2 = 0;
                int size2 = 0;

                audioFifo.prepareToRead(
                    audioFifo.getNumReady(),
                    start1,
                    size1,
                    start2,
                    size2);

                audioFifo.finishedRead(size1 + size2);
            }
        }

        if (!enabled.load())
        {
            wait(20);
            continue;
        }

        const int readySamples = audioFifo.getNumReady();

        if (readySamples <= 0)
        {
            wait(10);
            continue;
        }

        int start1 = 0;
        int size1 = 0;
        int start2 = 0;
        int size2 = 0;

        audioFifo.prepareToRead(
            readySamples,
            start1,
            size1,
            start2,
            size2);

        if (size1 > 0)
        {
            consumeSamples(
                fifoBuffer.data() + start1,
                size1);
        }

        if (size2 > 0)
        {
            consumeSamples(
                fifoBuffer.data() + start2,
                size2);
        }

        audioFifo.finishedRead(size1 + size2);
    }
}

void TunerEngine::consumeSamples(
    const float* samples,
    int numSamples)
{
    if (samples == nullptr || numSamples <= 0)
        return;

    for (int index = 0; index < numSamples; ++index)
    {
        historyBuffer[
            static_cast<std::size_t>(
                historyWritePosition)] = samples[index];

        historyWritePosition =
            (historyWritePosition + 1) %
            analysisSize;

        historySampleCount =
            juce::jmin(
                historySampleCount + 1,
                analysisSize);

        ++samplesSinceLastAnalysis;

        if (historySampleCount == analysisSize &&
            samplesSinceLastAnalysis >= hopSize)
        {
            samplesSinceLastAnalysis = 0;

            copyHistoryToAnalysisBuffer();
            analyseCurrentWindow();

            if (threadShouldExit() ||
                !enabled.load())
            {
                return;
            }
        }
    }
}

void TunerEngine::copyHistoryToAnalysisBuffer()
{
    // historyWritePosition apunta a la muestra más antigua.
    const int firstSectionSize =
        analysisSize - historyWritePosition;

    std::copy_n(
        historyBuffer.begin() + historyWritePosition,
        firstSectionSize,
        analysisBuffer.begin());

    if (historyWritePosition > 0)
    {
        std::copy_n(
            historyBuffer.begin(),
            historyWritePosition,
            analysisBuffer.begin() + firstSectionSize);
    }
}

void TunerEngine::analyseCurrentWindow()
{
    if (sampleRate <= 0.0)
    {
        publishInvalidResult(-100.0f);
        return;
    }

    double sumSquares = 0.0;
    double mean = 0.0;

    for (const float sample : analysisBuffer)
    {
        mean += static_cast<double>(sample);

        sumSquares +=
            static_cast<double>(sample) *
            static_cast<double>(sample);
    }

    mean /= static_cast<double>(analysisSize);

    const float rms =
        static_cast<float>(
            std::sqrt(
                sumSquares /
                static_cast<double>(analysisSize)));

    const float levelDb =
        juce::Decibels::gainToDecibels(
            rms,
            -100.0f);

    if (levelDb < minimumSignalDb)
    {
        publishInvalidResult(levelDb);
        return;
    }

    /*
        Eliminación de continua + ventana Hann.

        fftInput tiene fftSize = 16384.
        Las últimas 8192 muestras permanecen a cero,
        lo que produce autocorrelación lineal y no circular.
    */
    std::fill(
        fftInput.begin(),
        fftInput.end(),
        std::complex<float>{});

    squarePrefix[0] = 0.0;

    for (int index = 0; index < analysisSize; ++index)
    {
        const float normalizedPosition =
            static_cast<float>(index) /
            static_cast<float>(analysisSize - 1);

        const float hann =
            0.5f -
            0.5f *
                std::cos(
                    juce::MathConstants<float>::twoPi *
                    normalizedPosition);

        const float sample =
            static_cast<float>(
                static_cast<double>(
                    analysisBuffer[
                        static_cast<std::size_t>(index)]) -
                mean) *
            hann;

        windowBuffer[
            static_cast<std::size_t>(index)] =
                sample;

        fftInput[
            static_cast<std::size_t>(index)] =
                std::complex<float>(sample, 0.0f);

        squarePrefix[
            static_cast<std::size_t>(index + 1)] =
                squarePrefix[
                    static_cast<std::size_t>(index)] +
                static_cast<double>(sample) *
                    static_cast<double>(sample);
    }

    fft.perform(
        fftInput.data(),
        fftOutput.data(),
        false);

    for (int index = 0; index < fftSize; ++index)
    {
        const float power =
            std::norm(
                fftOutput[
                    static_cast<std::size_t>(index)]);

        powerSpectrum[
            static_cast<std::size_t>(index)] =
                std::complex<float>(power, 0.0f);
    }

    fft.perform(
        powerSpectrum.data(),
        autocorrelation.data(),
        true);

    const int minimumLag =
        juce::jmax(
            1,
            static_cast<int>(
                sampleRate /
                static_cast<double>(
                    maximumFrequencyHz)));

    const int maximumLag =
        juce::jmin(
            analysisSize - 2,
            static_cast<int>(
                sampleRate /
                static_cast<double>(
                    minimumFrequencyHz)));

    if (minimumLag >= maximumLag)
    {
        publishInvalidResult(levelDb);
        return;
    }

    std::fill(
        nsdfBuffer.begin(),
        nsdfBuffer.end(),
        0.0f);

    /*
        MPM / NSDF:

        NSDF(tau) = 2 * autocorrelation(tau)
                    / (energyA + energyB)
    */
    for (int lag = minimumLag;
         lag <= maximumLag;
         ++lag)
    {
        const int comparedSamples =
            analysisSize - lag;

        const double energyA =
            squarePrefix[
                static_cast<std::size_t>(
                    comparedSamples)] -
            squarePrefix[0];

        const double energyB =
            squarePrefix[
                static_cast<std::size_t>(
                    analysisSize)] -
            squarePrefix[
                static_cast<std::size_t>(lag)];

        const double denominator =
            energyA + energyB;

        if (denominator <=
            std::numeric_limits<double>::epsilon())
        {
            continue;
        }

        const double correlation =
            static_cast<double>(
                autocorrelation[
                    static_cast<std::size_t>(lag)]
                    .real());

        nsdfBuffer[
            static_cast<std::size_t>(lag)] =
                static_cast<float>(
                    (2.0 * correlation) /
                    denominator);
    }

    float strongestPeak = 0.0f;

    for (int lag = minimumLag + 1;
         lag < maximumLag;
         ++lag)
    {
        const float previous =
            nsdfBuffer[
                static_cast<std::size_t>(lag - 1)];

        const float current =
            nsdfBuffer[
                static_cast<std::size_t>(lag)];

        const float next =
            nsdfBuffer[
                static_cast<std::size_t>(lag + 1)];

        if (current > previous &&
            current >= next &&
            current > strongestPeak)
        {
            strongestPeak = current;
        }
    }

    if (strongestPeak < minimumConfidence)
    {
        publishInvalidResult(levelDb);
        return;
    }

    /*
        Elegimos el primer pico suficientemente próximo al
        máximo. Esto suele seleccionar el periodo fundamental
        antes que sus múltiplos.
    */
    const float peakThreshold =
        strongestPeak * 0.90f;

    int selectedLag = -1;

    bool passedNegativeRegion = false;

    for (int lag = minimumLag + 1;
         lag < maximumLag;
         ++lag)
    {
        if (nsdfBuffer[
                static_cast<std::size_t>(lag)] < 0.0f)
        {
            passedNegativeRegion = true;
        }

        const float previous =
            nsdfBuffer[
                static_cast<std::size_t>(lag - 1)];

        const float current =
            nsdfBuffer[
                static_cast<std::size_t>(lag)];

        const float next =
            nsdfBuffer[
                static_cast<std::size_t>(lag + 1)];

        if (passedNegativeRegion &&
            current >= peakThreshold &&
            current > previous &&
            current >= next)
        {
            selectedLag = lag;
            break;
        }
    }

    if (selectedLag < 0)
    {
        for (int lag = minimumLag + 1;
             lag < maximumLag;
             ++lag)
        {
            const float previous =
                nsdfBuffer[
                    static_cast<std::size_t>(lag - 1)];

            const float current =
                nsdfBuffer[
                    static_cast<std::size_t>(lag)];

            const float next =
                nsdfBuffer[
                    static_cast<std::size_t>(lag + 1)];

            if (current >= peakThreshold &&
                current > previous &&
                current >= next)
            {
                selectedLag = lag;
                break;
            }
        }
    }

    if (selectedLag < 0)
    {
        publishInvalidResult(levelDb);
        return;
    }

    float refinedLag =
        static_cast<float>(selectedLag);

    const float left =
        nsdfBuffer[
            static_cast<std::size_t>(
                selectedLag - 1)];

    const float centre =
        nsdfBuffer[
            static_cast<std::size_t>(
                selectedLag)];

    const float right =
        nsdfBuffer[
            static_cast<std::size_t>(
                selectedLag + 1)];

    const float interpolationDenominator =
        left - 2.0f * centre + right;

    if (std::abs(interpolationDenominator) >
        0.000001f)
    {
        const float offset =
            0.5f *
            (left - right) /
            interpolationDenominator;

        refinedLag +=
            juce::jlimit(
                -1.0f,
                1.0f,
                offset);
    }

    if (refinedLag <= 0.0f)
    {
        publishInvalidResult(levelDb);
        return;
    }

    const float frequencyHz =
        static_cast<float>(
            sampleRate /
            static_cast<double>(refinedLag));

    if (!std::isfinite(frequencyHz) ||
        frequencyHz < minimumFrequencyHz ||
        frequencyHz > maximumFrequencyHz)
    {
        publishInvalidResult(levelDb);
        return;
    }

    const double exactMidiNote =
        69.0 +
        12.0 *
            std::log2(
                static_cast<double>(frequencyHz) /
                referenceA4Hz);

    const int nearestMidiNote =
        static_cast<int>(
            std::round(exactMidiNote));

    const double targetFrequency =
        referenceA4Hz *
        std::pow(
            2.0,
            (static_cast<double>(nearestMidiNote) -
             69.0) /
                12.0);

    const float cents =
        static_cast<float>(
            1200.0 *
            std::log2(
                static_cast<double>(frequencyHz) /
                targetFrequency));

    publishResult(
        frequencyHz,
        cents,
        centre,
        levelDb,
        nearestMidiNote);
}

void TunerEngine::publishInvalidResult(
    float levelDb) noexcept
{
    resultValid.store(false);

    resultFrequencyHz.store(0.0f);
    resultCents.store(0.0f);
    resultConfidence.store(0.0f);
    resultLevelDb.store(levelDb);
    resultMidiNote.store(-1);
}

void TunerEngine::publishResult(
    float frequencyHz,
    float cents,
    float confidence,
    float levelDb,
    int midiNote) noexcept
{
    // valid se publica al final para que la GUI lea
    // primero un conjunto de valores ya actualizado.
    resultValid.store(false);

    resultFrequencyHz.store(frequencyHz);
    resultCents.store(cents);
    resultConfidence.store(confidence);
    resultLevelDb.store(levelDb);
    resultMidiNote.store(midiNote);

    resultValid.store(true);
}