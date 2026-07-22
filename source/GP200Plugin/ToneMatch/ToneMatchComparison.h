/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed under GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "ToneMatchTypes.h"

#include <vector>

namespace tonematch
{
struct ToneMatchComparisonResult
{
    bool success{false};

    juce::String message;
    juce::String warning;

    std::vector<double> frequencyHz;

    // Diferencia directa después de interpolar los perfiles.
    std::vector<double> rawCorrectionDb;

    // Diferencia después de eliminar la diferencia global de nivel.
    std::vector<double> levelAlignedCorrectionDb;

    std::vector<double> confidence;

    // Nivel medio SOURCE - TARGET eliminado de la curva.
    double removedLevelOffsetDb{0.0};

    double minimumCorrectionDb{0.0};
    double maximumCorrectionDb{0.0};

    bool isValid() const noexcept
    {
        const auto size = frequencyHz.size();

        return success
            && size > 1
            && rawCorrectionDb.size() == size
            && levelAlignedCorrectionDb.size() == size
            && confidence.size() == size;
    }
};

class ToneMatchComparison
{
  public:
    struct Options
    {
        double minimumFrequencyHz{40.0};
        double maximumFrequencyHz{18000.0};

        // Número de puntos de la curva visual y del futuro solver.
        int outputPointCount{512};

        // Banda empleada para eliminar la diferencia global de volumen.
        double levelReferenceMinimumHz{100.0};
        double levelReferenceMaximumHz{10000.0};

        // No se usarán puntos con confianza inferior a este valor
        // para calcular el offset general de nivel.
        double minimumConfidenceForLevel{0.10};
    };

    ToneMatchComparisonResult compare (
        const ToneAnalysisProfile& source,
        const ToneAnalysisProfile& target,
        const Options& options = Options{}) const;

  private:
    static double interpolate (
        const std::vector<double>& frequencyHz,
        const std::vector<double>& values,
        double requestedFrequencyHz);

    static bool validateOptions (
        const Options& options,
        juce::String& error);
};
}