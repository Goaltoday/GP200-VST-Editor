#pragma once

#include "ToneMatchTypes.h"

namespace tonematch
{
class ToneAnalysis
{
  public:
    struct Options
    {
        // 2^13 = 8192 muestras.
        int fftOrder{13};

        // 75 % de solapamiento.
        double overlapRatio{0.75};

        // Las ventanas por debajo de este nivel se ignoran.
        double silenceThresholdDb{-55.0};

        // Una ventana que alcance este valor se considera saturada.
        float clippingThreshold{0.999f};

        double minimumFrequencyHz{30.0};
        double maximumFrequencyHz{20000.0};

        // Evita logaritmos de cero.
        double powerFloor{1.0e-20};
    };

    ToneAnalysisProfile analyse (
        const ToneCaptureData& capture,
        const Options& options = Options{}) const;

  private:
    static bool validateOptions (
        const Options& options,
        juce::String& errorMessage);
};
}