#include "GP200EffectParamDatabase.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace gp200
{
namespace
{
constexpr GP200EffectParamInfo paramLayout_000[] = {
    {0, "Sustain", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_001[] = {
    {0, "Sustain", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Attack", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Clipping", GP200ParamKind::continuous, 10.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_002[] = {
    {0, "Threshold", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Ratio", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Attack", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Release", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Blend", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_003[] = {
    {0, "Gain", GP200ParamKind::continuous, 29.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_004[] = {
    {0, "Gain", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_005[] = {
    {0, "Gain", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_006[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_007[] = {
    {0, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {3, "Low Cut", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_008[] = {
    {0, "Gain", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "+3dB", GP200ParamKind::toggle, 1.0f, 0.0f, 0.0f, 0.0f},
    {2, "Bright", GP200ParamKind::toggle, 1.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_009[] = {
    {0, "Threshold", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_010[] = {
    {0, "Threshold", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Attack", GP200ParamKind::continuous, 25.0f, 0.0f, 100.0f, 1.0f},
    {2, "Release", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_011[] = {
    {0, "Threshold", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Ratio", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {2, "Attack", GP200ParamKind::continuous, 20.0f, 1.0f, 500.0f, 1.0f},
    {3, "Release", GP200ParamKind::continuous, 200.0f, 10.0f, 10000.0f, 1.0f},
    {4, "Hold", GP200ParamKind::continuous, 500.0f, 0.0f, 1000.0f, 1.0f},
    {5, "Hysteresis", GP200ParamKind::continuous, -3.0f, -10.0f, 0.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_012[] = {
    {0, "Shape", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_013[] = {
    {0, "Body", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Top", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Mode", GP200ParamKind::choice, 2.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_014[] = {
    {0, "Sense", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Range", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Q", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Mix", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Mode", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_015[] = {
    {0, "Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Low", GP200ParamKind::continuous, 25.0f, 0.0f, 100.0f, 1.0f},
    {4, "Q", GP200ParamKind::continuous, 70.0f, 0.0f, 100.0f, 1.0f},
    {5, "High", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
    {6, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_016[] = {
    {0, "Step 1", GP200ParamKind::continuous, 10.0f, 0.0f, 100.0f, 1.0f},
    {1, "Step 2", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {2, "Step 3", GP200ParamKind::continuous, 55.0f, 0.0f, 100.0f, 1.0f},
    {3, "Step 4", GP200ParamKind::continuous, 85.0f, 0.0f, 100.0f, 1.0f},
    {4, "Rate", GP200ParamKind::continuous, 10.0f, 0.0f, 100.0f, 1.0f},
    {5, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_017[] = {
    {0, "Low Oct", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "High Oct", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Dry", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_018[] = {
    {0, "Hi Pitch", GP200ParamKind::continuous, 12.0f, 0.0f, 24.0f, 1.0f},
    {1, "Low Pitch", GP200ParamKind::continuous, -12.0f, -24.0f, 0.0f, 1.0f},
    {2, "Dry", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Hi Vol", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Low Vol", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_019[] = {
    {0, "Hi Pitch", GP200ParamKind::continuous, 12.0f, 0.0f, 12.0f, 1.0f},
    {1, "Low Pitch", GP200ParamKind::continuous, 0.0f, -12.0f, 0.0f, 1.0f},
    {2, "Wet", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Dry", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Range", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_020[] = {
    {0, "Detune", GP200ParamKind::continuous, -25.0f, -50.0f, 50.0f, 1.0f},
    {1, "Wet", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Dry", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_021[] = {
    {0, "Mix", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Krush", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bit", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {3, "Hi Cut", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Lo Cut", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_022[] = {
    {0, "Mix", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Freq", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Fine", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {3, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_023[] = {
    {0, "Saturation", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Mix", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "High Cut", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_024[] = {
    {0, "125Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {1, "400Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {2, "800Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {3, "1.6kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {4, "4kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {5, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_025[] = {
    {0, "100Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {1, "500Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {2, "1kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {3, "3kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {4, "6kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {5, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_026[] = {
    {0, "33Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {1, "150Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {2, "600Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {3, "2kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {4, "8kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {5, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_027[] = {
    {0, "50Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {1, "120Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {2, "400Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {3, "800Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {4, "4.5kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {5, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_028[] = {
    {0, "80Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {1, "240Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {2, "750Hz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {3, "2.2kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {4, "6.6kHz", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_029[] = {
    {0, "31Hz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {1, "63Hz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {2, "125Hz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {3, "250Hz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {4, "500Hz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {5, "1kHz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {6, "2kHz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {7, "4kHz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {8, "8kHz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {9, "16kHz", GP200ParamKind::continuous, 0.0f, -12.0f, 12.0f, 1.0f},
    {10, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_030[] = {
    {0, "Range", GP200ParamKind::choice, 2.0f, 0.0f, 0.0f, 0.0f},
    {1, "Harmony", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Position", GP200ParamKind::continuous, 0.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_031[] = {
    {0, "Mix", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Key", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
    {2, "Mode", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
    {3, "Interval", GP200ParamKind::choice, 8.0f, 0.0f, 0.0f, 0.0f},
    {4, "Smooth Mode", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_032[] = {
    {0, "Mix", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Key", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
    {2, "Mode", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
    {3, "Interval 1", GP200ParamKind::choice, 8.0f, 0.0f, 0.0f, 0.0f},
    {4, "Interval 2", GP200ParamKind::choice, 10.0f, 0.0f, 0.0f, 0.0f},
    {5, "Smooth Mode", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_033[] = {
    {0, "Gain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 70.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_034[] = {
    {0, "Gain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_035[] = {
    {0, "Gain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_036[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_037[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Fat", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {4, "Air", GP200ParamKind::toggle, 1.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_038[] = {
    {0, "Gain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_039[] = {
    {0, "Gain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Mode", GP200ParamKind::choice, 1.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_040[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_041[] = {
    {0, "Gain", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_042[] = {
    {0, "Gain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_043[] = {
    {0, "Gain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Voice", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_044[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Attack", GP200ParamKind::continuous, 2.0f, 0.0f, 5.0f, 1.0f},
    {4, "Gate", GP200ParamKind::continuous, 10.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_045[] = {
    {0, "Gain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Mode", GP200ParamKind::choice, 1.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_046[] = {
    {0, "Sustain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_047[] = {
    {0, "Fuzz", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_048[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_049[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Filter", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_050[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_051[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Contour", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_052[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Mode", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_053[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Mode", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
    {4, "Blend", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_054[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Blend", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_055[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Blend", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Low", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Lo-mid", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Hi-mid", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {7, "Attack", GP200ParamKind::choice, 2.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_056[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Master", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Mid Freq", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Drive", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_057[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Tight", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_058[] = {
    {0, "Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_059[] = {
    {0, "Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_060[] = {
    {0, "Mode", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_061[] = {
    {0, "Mix", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Filter", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Depth L", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Depth C", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Depth R", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_062[] = {
    {0, "Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Pre Delay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Feedback", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_063[] = {
    {0, "Flg Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Flg Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Feedback", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Trm Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Trm Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {5, "Flg Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {6, "Trm Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_064[] = {
    {0, "Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_065[] = {
    {0, "Sens", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Output", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_066[] = {
    {0, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {1, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_067[] = {
    {0, "Color", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_068[] = {
    {0, "Phs Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Phs Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Pan Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Pan Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {4, "Phs Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {5, "Pan Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_069[] = {
    {0, "Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Mode", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
    {4, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_070[] = {
    {0, "Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Rate", GP200ParamKind::continuous, 0.5f, 0.1f, 10.0f, 0.1f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {4, "Bias", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_071[] = {
    {0, "Attack", GP200ParamKind::continuous, 1000.0f, 80.0f, 4000.0f, 1.0f},
    {1, "Curve", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_072[] = {
    {0, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Activate", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_073[] = {
    {0, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Attack", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {2, "Release", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Activate", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_074[] = {
    {0, "Range", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Q", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Position", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_075[] = {
    {0, "Range", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Q", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Position", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "EQ", GP200ParamKind::toggle, 1.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_076[] = {
    {0, "Volume", GP200ParamKind::continuous, 100.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_077[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_078[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_079[] = {
    {0, "Gain", GP200ParamKind::continuous, 35.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Middle", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
    {5, "Bright", GP200ParamKind::toggle, 1.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_080[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume ", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_081[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Bright", GP200ParamKind::toggle, 1.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_082[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone Cut", GP200ParamKind::continuous, 60.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_083[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone Cut", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bright", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_084[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Bright", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_085[] = {
    {0, "Gain", GP200ParamKind::continuous, 35.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_086[] = {
    {0, "Gain", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_087[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Bright", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_088[] = {
    {0, "Gain", GP200ParamKind::continuous, 35.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone Cut", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_089[] = {
    {0, "Gain", GP200ParamKind::continuous, 35.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_090[] = {
    {0, "Gain", GP200ParamKind::continuous, 35.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_091[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone Cut", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Char", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_092[] = {
    {0, "Gain 1", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone 1", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Gain 2", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Tone 2", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_093[] = {
    {0, "Gain", GP200ParamKind::continuous, 25.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 65.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 45.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 65.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_094[] = {
    {0, "Gain", GP200ParamKind::continuous, 45.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_095[] = {
    {0, "Gain 1", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Gain 2", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_096[] = {
    {0, "Gain 1", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Gain 2", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_097[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_098[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_099[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone Cut", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_100[] = {
    {0, "Input", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_101[] = {
    {0, "Gain", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Edge", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_102[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Presence", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_103[] = {
    {0, "Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Midrange", GP200ParamKind::choice, 1.0f, 0.0f, 0.0f, 0.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_104[] = {
    {0, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_105[] = {
    {0, "Gain", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_106[] = {
    {0, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Bright", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {2, "Bass", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Middle", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Treble", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_107[] = {
    {0, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {1, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Balance", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "EQ Freq", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "EQ Q", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "EQ Gain", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_108[] = {
    {1, "Volume", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Low Cut", GP200ParamKind::continuous, 19.0f, 19.0f, 2000.0f, 1.0f},
    {6, "High Cut", GP200ParamKind::continuous, 20001.0f, 2000.0f, 20001.0f, 1.0f},
};

constexpr GP200EffectParamInfo paramLayout_109[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {4, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_110[] = {
    {0, "Mix A", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time A", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback A", GP200ParamKind::continuous, 20.0f, -100.0f, 100.0f, 1.0f},
    {3, "Mix B", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {4, "Time B", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {5, "Feedback B", GP200ParamKind::continuous, 20.0f, -100.0f, 100.0f, 1.0f},
    {6, "Sync A", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {7, "Sync B", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {8, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_111[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 150.0f, 20.0f, 300.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {3, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_112[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Sweep Depth", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Sweep Rate", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Sweep Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {6, "Time Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {7, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_113[] = {
    {0, "Dly Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Ring Mix", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Freq", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {7, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_114[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {3, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {4, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_115[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {3, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Mode", GP200ParamKind::continuous, 5.0f, 1.0f, 12.0f, 1.0f},
    {5, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {6, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_116[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Mod", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {6, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_117[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 20.0f, -100.0f, 100.0f, 1.0f},
    {3, "Time R%", GP200ParamKind::continuous, 61.8f, 1.0f, 100.0f, 1.0f},
    {4, "Spread", GP200ParamKind::continuous, 100.0f, -100.0f, 100.0f, 1.0f},
    {5, "Level", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {7, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_118[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 20.0f, -100.0f, 100.0f, 1.0f},
    {3, "Time R%", GP200ParamKind::continuous, 61.8f, 1.0f, 100.0f, 1.0f},
    {4, "Spread", GP200ParamKind::continuous, 100.0f, -100.0f, 100.0f, 1.0f},
    {5, "Wow &amp;", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Age", GP200ParamKind::choice, 0.0f, 0.0f, 0.0f, 0.0f},
    {7, "Scrape", GP200ParamKind::continuous, 100.0f, 0.0f, 100.0f, 1.0f},
    {8, "Drive", GP200ParamKind::continuous, 0.0f, 0.0f, 100.0f, 1.0f},
    {9, "Level", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {10, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {11, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_119[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 20.0f, -100.0f, 100.0f, 1.0f},
    {3, "Crush", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {4, "Bit", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {5, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {6, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_120[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 20.0f, -100.0f, 100.0f, 1.0f},
    {3, "Level", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {5, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_121[] = {
    {0, "Mix", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Level", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Mod", GP200ParamKind::continuous, 100.0f, 0.0f, 100.0f, 1.0f},
    {5, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {7, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_122[] = {
    {0, "Mix", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 4000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {3, "Latch", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Cut", GP200ParamKind::continuous, 100.0f, 0.0f, 100.0f, 1.0f},
    {5, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {6, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_123[] = {
    {0, "Mix", GP200ParamKind::continuous, 20.0f, 0.0f, 100.0f, 1.0f},
    {1, "Time", GP200ParamKind::continuous, 500.0f, 20.0f, 2000.0f, 1.0f},
    {2, "Feedback", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {3, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Mod", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {5, "Pitch", GP200ParamKind::choice, 28.0f, 0.0f, 0.0f, 0.0f},
    {6, "Slice", GP200ParamKind::choice, 2.0f, 0.0f, 0.0f, 0.0f},
    {7, "Direction", GP200ParamKind::continuous, -100.0f, -100.0f, 100.0f, 1.0f},
    {8, "Blend", GP200ParamKind::continuous, 40.0f, 0.0f, 100.0f, 1.0f},
    {9, "Smooth", GP200ParamKind::continuous, 0.0f, 0.0f, 100.0f, 1.0f},
    {10, "Level", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {11, "Sync", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
    {12, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_124[] = {
    {0, "Mix", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Pre Delay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Decay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_125[] = {
    {0, "Mix", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Decay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "High Damp", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_126[] = {
    {0, "Mix", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Decay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Tone", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_127[] = {
    {0, "Mix", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Decay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_128[] = {
    {0, "Mix", GP200ParamKind::continuous, 30.0f, 0.0f, 100.0f, 1.0f},
    {1, "Pre Delay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {2, "Decay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Lo End", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {4, "Hi End", GP200ParamKind::continuous, 0.0f, -50.0f, 50.0f, 1.0f},
    {5, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_129[] = {
    {0, "Mix", GP200ParamKind::continuous, 25.0f, 0.0f, 100.0f, 1.0f},
    {1, "Pre Delay", GP200ParamKind::continuous, 0.0f, 0.0f, 300.0f, 1.0f},
    {2, "Decay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Low Damp", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Hi Damp", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Mod", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_130[] = {
    {0, "Mix", GP200ParamKind::continuous, 25.0f, 0.0f, 100.0f, 1.0f},
    {1, "Pre Delay", GP200ParamKind::continuous, 0.0f, 0.0f, 300.0f, 1.0f},
    {2, "Decay", GP200ParamKind::continuous, 70.0f, 0.0f, 100.0f, 1.0f},
    {3, "Low Damp", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Hi Damp", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Mod", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_131[] = {
    {0, "Mix", GP200ParamKind::continuous, 25.0f, 0.0f, 100.0f, 1.0f},
    {1, "Pre Delay", GP200ParamKind::continuous, 100.0f, 0.0f, 300.0f, 1.0f},
    {2, "Decay", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {3, "Low Damp", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Hi Damp", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Mod", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamInfo paramLayout_132[] = {
    {0, "Mix", GP200ParamKind::continuous, 25.0f, 0.0f, 100.0f, 1.0f},
    {1, "Pre Delay", GP200ParamKind::continuous, 100.0f, 0.0f, 300.0f, 1.0f},
    {2, "Decay", GP200ParamKind::continuous, 70.0f, 0.0f, 100.0f, 1.0f},
    {3, "Low Damp", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {4, "Hi Damp", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {5, "Mod", GP200ParamKind::continuous, 50.0f, 0.0f, 100.0f, 1.0f},
    {6, "Trail", GP200ParamKind::toggle, 0.0f, 0.0f, 0.0f, 0.0f},
};

constexpr GP200EffectParamSet paramSets[] = {
    {0X00000000u, paramLayout_000, 2},  {0X00000001u, paramLayout_001, 4}, {0X00000003u, paramLayout_002, 7},
    {0X0000000Au, paramLayout_003, 4},  {0X0000000Bu, paramLayout_004, 4}, {0X0000000Cu, paramLayout_004, 4},
    {0X0000000Eu, paramLayout_005, 1},  {0X00000014u, paramLayout_006, 1}, {0X00000019u, paramLayout_007, 4},
    {0X0000001Au, paramLayout_008, 3},  {0X0000001Bu, paramLayout_009, 1}, {0X0000001Du, paramLayout_010, 3},
    {0X00000021u, paramLayout_011, 6},  {0X01000000u, paramLayout_012, 1}, {0X01000001u, paramLayout_013, 4},
    {0X0100000Fu, paramLayout_014, 5},  {0X01000015u, paramLayout_015, 7}, {0X01000019u, paramLayout_016, 6},
    {0X01000021u, paramLayout_017, 3},  {0X01000023u, paramLayout_018, 5}, {0X01000024u, paramLayout_019, 5},
    {0X01000029u, paramLayout_020, 3},  {0X0100002Eu, paramLayout_021, 5}, {0X0100002Fu, paramLayout_022, 4},
    {0X01000033u, paramLayout_023, 4},  {0X01000035u, paramLayout_024, 6}, {0X01000036u, paramLayout_025, 6},
    {0X01000039u, paramLayout_026, 6},  {0X0100003Au, paramLayout_027, 6}, {0X0100003Cu, paramLayout_028, 5},
    {0X01000043u, paramLayout_029, 11}, {0X01000049u, paramLayout_030, 4}, {0X0100004Du, paramLayout_031, 5},
    {0X0100004Eu, paramLayout_032, 6},  {0X03000000u, paramLayout_033, 3}, {0X03000001u, paramLayout_033, 3},
    {0X03000002u, paramLayout_034, 2},  {0X03000004u, paramLayout_035, 3}, {0X03000006u, paramLayout_036, 3},
    {0X03000008u, paramLayout_037, 5},  {0X03000009u, paramLayout_038, 3}, {0X0300000Au, paramLayout_039, 4},
    {0X0300000Bu, paramLayout_040, 4},  {0X0300000Eu, paramLayout_041, 3}, {0X0300000Fu, paramLayout_042, 5},
    {0X03000010u, paramLayout_043, 4},  {0X03000014u, paramLayout_035, 3}, {0X0300001Cu, paramLayout_044, 5},
    {0X0300001Eu, paramLayout_045, 5},  {0X03000022u, paramLayout_046, 3}, {0X03000024u, paramLayout_047, 2},
    {0X03000026u, paramLayout_047, 2},  {0X03000029u, paramLayout_048, 2}, {0X0300002Au, paramLayout_036, 3},
    {0X0300002Bu, paramLayout_049, 3},  {0X0300002Du, paramLayout_050, 5}, {0X0300002Eu, paramLayout_051, 5},
    {0X03000030u, paramLayout_036, 3},  {0X03000032u, paramLayout_052, 4}, {0X0300003Fu, paramLayout_053, 5},
    {0X03000040u, paramLayout_054, 5},  {0X03000044u, paramLayout_055, 8}, {0X03000050u, paramLayout_052, 4},
    {0X03000051u, paramLayout_056, 7},  {0X03000052u, paramLayout_057, 6}, {0X04000000u, paramLayout_058, 4},
    {0X04000001u, paramLayout_059, 4},  {0X04000002u, paramLayout_060, 1}, {0X04000008u, paramLayout_059, 4},
    {0X0400000Fu, paramLayout_061, 7},  {0X04000011u, paramLayout_062, 5}, {0X04000012u, paramLayout_062, 5},
    {0X04000013u, paramLayout_062, 5},  {0X04000014u, paramLayout_063, 7}, {0X04000015u, paramLayout_064, 3},
    {0X04000016u, paramLayout_059, 4},  {0X04000017u, paramLayout_059, 4}, {0X04000018u, paramLayout_065, 4},
    {0X04000019u, paramLayout_066, 2},  {0X0400001Au, paramLayout_064, 3}, {0X0400001Bu, paramLayout_067, 3},
    {0X0400001Eu, paramLayout_068, 6},  {0X0400001Fu, paramLayout_064, 3}, {0X04000020u, paramLayout_069, 5},
    {0X04000021u, paramLayout_064, 3},  {0X04000026u, paramLayout_059, 4}, {0X04000027u, paramLayout_059, 4},
    {0X04000028u, paramLayout_070, 5},  {0X0400002Du, paramLayout_071, 2}, {0X0400002Fu, paramLayout_072, 2},
    {0X04000030u, paramLayout_073, 4},  {0X05000001u, paramLayout_074, 4}, {0X05000006u, paramLayout_074, 4},
    {0X05000007u, paramLayout_074, 4},  {0X05000008u, paramLayout_074, 4}, {0X0500000Au, paramLayout_075, 5},
    {0X06000003u, paramLayout_076, 1},  {0X07000001u, paramLayout_077, 3}, {0X07000003u, paramLayout_078, 6},
    {0X07000004u, paramLayout_079, 6},  {0X07000005u, paramLayout_080, 4}, {0X07000009u, paramLayout_081, 5},
    {0X0700000Du, paramLayout_042, 5},  {0X0700000Fu, paramLayout_077, 3}, {0X07000010u, paramLayout_082, 5},
    {0X07000011u, paramLayout_083, 4},  {0X07000014u, paramLayout_084, 5}, {0X07000015u, paramLayout_085, 6},
    {0X07000019u, paramLayout_086, 6},  {0X0700001Au, paramLayout_087, 6}, {0X0700001Bu, paramLayout_088, 6},
    {0X0700001Fu, paramLayout_089, 5},  {0X07000022u, paramLayout_090, 3}, {0X07000023u, paramLayout_078, 6},
    {0X07000024u, paramLayout_085, 6},  {0X07000027u, paramLayout_091, 6}, {0X07000028u, paramLayout_092, 5},
    {0X0700002Au, paramLayout_093, 6},  {0X0700002Bu, paramLayout_094, 6}, {0X0700002Cu, paramLayout_095, 7},
    {0X0700002Du, paramLayout_086, 6},  {0X0700002Eu, paramLayout_086, 6}, {0X0700002Fu, paramLayout_096, 7},
    {0X07000030u, paramLayout_097, 6},  {0X07000035u, paramLayout_097, 6}, {0X07000039u, paramLayout_097, 6},
    {0X0700003Au, paramLayout_097, 6},  {0X0700003Bu, paramLayout_097, 6}, {0X0700003Du, paramLayout_078, 6},
    {0X0700003Eu, paramLayout_098, 5},  {0X07000040u, paramLayout_097, 6}, {0X07000041u, paramLayout_097, 6},
    {0X07000043u, paramLayout_097, 6},  {0X07000044u, paramLayout_097, 6}, {0X07000047u, paramLayout_097, 6},
    {0X07000048u, paramLayout_097, 6},  {0X07000049u, paramLayout_099, 6}, {0X0700004Au, paramLayout_100, 7},
    {0X0700004Bu, paramLayout_101, 6},  {0X0700004Eu, paramLayout_097, 6}, {0X07000053u, paramLayout_050, 5},
    {0X07000055u, paramLayout_097, 6},  {0X07000056u, paramLayout_097, 6}, {0X07000057u, paramLayout_097, 6},
    {0X07000059u, paramLayout_097, 6},  {0X0700005Au, paramLayout_102, 6}, {0X0700005Du, paramLayout_097, 6},
    {0X0700005Eu, paramLayout_097, 6},  {0X0700005Fu, paramLayout_097, 6}, {0X07000060u, paramLayout_097, 6},
    {0X07000063u, paramLayout_097, 6},  {0X07000065u, paramLayout_097, 6}, {0X07000066u, paramLayout_097, 6},
    {0X07000068u, paramLayout_097, 6},  {0X07000069u, paramLayout_097, 6}, {0X0700006Au, paramLayout_097, 6},
    {0X0700006Bu, paramLayout_097, 6},  {0X0700006Du, paramLayout_097, 6}, {0X0700006Eu, paramLayout_097, 6},
    {0X07000073u, paramLayout_103, 6},  {0X07000075u, paramLayout_104, 3}, {0X07000077u, paramLayout_050, 5},
    {0X0700007Bu, paramLayout_105, 5},  {0X0700007Cu, paramLayout_098, 5}, {0X08000075u, paramLayout_104, 3},
    {0X08000076u, paramLayout_106, 5},  {0X0800007Au, paramLayout_107, 6}, {0X0800007Bu, paramLayout_107, 6},
    {0X0A000000u, paramLayout_108, 3},  {0X0A000001u, paramLayout_108, 3}, {0X0A000002u, paramLayout_108, 3},
    {0X0A000003u, paramLayout_108, 3},  {0X0A000004u, paramLayout_108, 3}, {0X0A000005u, paramLayout_108, 3},
    {0X0A000006u, paramLayout_108, 3},  {0X0A000007u, paramLayout_108, 3}, {0X0A000008u, paramLayout_108, 3},
    {0X0A000009u, paramLayout_108, 3},  {0X0A00000Au, paramLayout_108, 3}, {0X0A00000Bu, paramLayout_108, 3},
    {0X0A00000Cu, paramLayout_108, 3},  {0X0A00000Du, paramLayout_108, 3}, {0X0A00000Eu, paramLayout_108, 3},
    {0X0A00000Fu, paramLayout_108, 3},  {0X0A000010u, paramLayout_108, 3}, {0X0A000011u, paramLayout_108, 3},
    {0X0A000012u, paramLayout_108, 3},  {0X0A000013u, paramLayout_108, 3}, {0X0A000014u, paramLayout_108, 3},
    {0X0A000015u, paramLayout_108, 3},  {0X0A000016u, paramLayout_108, 3}, {0X0A000017u, paramLayout_108, 3},
    {0X0A000018u, paramLayout_108, 3},  {0X0A000019u, paramLayout_108, 3}, {0X0A00001Au, paramLayout_108, 3},
    {0X0A00001Bu, paramLayout_108, 3},  {0X0A00001Cu, paramLayout_108, 3}, {0X0A00001Du, paramLayout_108, 3},
    {0X0A00001Eu, paramLayout_108, 3},  {0X0A00001Fu, paramLayout_108, 3}, {0X0A000020u, paramLayout_108, 3},
    {0X0A000021u, paramLayout_108, 3},  {0X0A000022u, paramLayout_108, 3}, {0X0A000023u, paramLayout_108, 3},
    {0X0A000024u, paramLayout_108, 3},  {0X0A000025u, paramLayout_108, 3}, {0X0A000026u, paramLayout_108, 3},
    {0X0A000027u, paramLayout_108, 3},  {0X0A000028u, paramLayout_108, 3}, {0X0A000029u, paramLayout_108, 3},
    {0X0A00002Au, paramLayout_108, 3},  {0X0A00002Bu, paramLayout_108, 3}, {0X0A00002Cu, paramLayout_108, 3},
    {0X0A00002Du, paramLayout_108, 3},  {0X0A00002Eu, paramLayout_108, 3}, {0X0A00002Fu, paramLayout_108, 3},
    {0X0A000030u, paramLayout_108, 3},  {0X0A000031u, paramLayout_108, 3}, {0X0A000032u, paramLayout_108, 3},
    {0X0A000033u, paramLayout_108, 3},  {0X0A000034u, paramLayout_108, 3}, {0X0A000035u, paramLayout_108, 3},
    {0X0A000036u, paramLayout_108, 3},  {0X0A000037u, paramLayout_108, 3}, {0X0A000038u, paramLayout_108, 3},
    {0X0A000039u, paramLayout_108, 3},  {0X0A00003Au, paramLayout_108, 3}, {0X0A00003Bu, paramLayout_108, 3},
    {0X0A00003Cu, paramLayout_108, 3},  {0X0A00003Du, paramLayout_108, 3}, {0X0A00003Eu, paramLayout_108, 3},
    {0X0A00003Fu, paramLayout_108, 3},  {0X0A000040u, paramLayout_108, 3}, {0X0A000041u, paramLayout_108, 3},
    {0X0A000042u, paramLayout_108, 3},  {0X0A000043u, paramLayout_108, 3}, {0X0A000044u, paramLayout_108, 3},
    {0X0A000045u, paramLayout_108, 3},  {0X0A100000u, paramLayout_108, 3}, {0X0A100001u, paramLayout_108, 3},
    {0X0A100002u, paramLayout_108, 3},  {0X0A100003u, paramLayout_108, 3}, {0X0A100004u, paramLayout_108, 3},
    {0X0A100005u, paramLayout_108, 3},  {0X0A100006u, paramLayout_108, 3}, {0X0A100007u, paramLayout_108, 3},
    {0X0A100008u, paramLayout_108, 3},  {0X0A100009u, paramLayout_108, 3}, {0X0A10000Au, paramLayout_108, 3},
    {0X0A10000Bu, paramLayout_108, 3},  {0X0A10000Cu, paramLayout_108, 3}, {0X0A10000Du, paramLayout_108, 3},
    {0X0A10000Eu, paramLayout_108, 3},  {0X0A10000Fu, paramLayout_108, 3}, {0X0A100010u, paramLayout_108, 3},
    {0X0A100011u, paramLayout_108, 3},  {0X0A100012u, paramLayout_108, 3}, {0X0A100013u, paramLayout_108, 3},
    {0X0B000000u, paramLayout_109, 5},  {0X0B000001u, paramLayout_109, 5}, {0X0B000002u, paramLayout_109, 5},
    {0X0B000003u, paramLayout_110, 9},  {0X0B000004u, paramLayout_109, 5}, {0X0B000005u, paramLayout_111, 5},
    {0X0B000006u, paramLayout_112, 8},  {0X0B000009u, paramLayout_113, 8}, {0X0B00000Bu, paramLayout_114, 5},
    {0X0B00000Cu, paramLayout_115, 7},  {0X0B00000Du, paramLayout_114, 5}, {0X0B000012u, paramLayout_114, 5},
    {0X0B000014u, paramLayout_116, 7},  {0X0B00001Du, paramLayout_117, 8}, {0X0B00001Fu, paramLayout_117, 8},
    {0X0B000021u, paramLayout_118, 12}, {0X0B000026u, paramLayout_119, 7}, {0X0B000028u, paramLayout_120, 6},
    {0X0B00002Au, paramLayout_121, 8},  {0X0B00002Eu, paramLayout_121, 8}, {0X0B000032u, paramLayout_122, 7},
    {0X0B000033u, paramLayout_123, 13}, {0X0C000000u, paramLayout_124, 4}, {0X0C000001u, paramLayout_124, 4},
    {0X0C000002u, paramLayout_124, 4},  {0X0C000003u, paramLayout_125, 4}, {0X0C000004u, paramLayout_126, 4},
    {0X0C000006u, paramLayout_127, 3},  {0X0C000007u, paramLayout_127, 3}, {0X0C000008u, paramLayout_128, 6},
    {0X0C000009u, paramLayout_128, 6},  {0X0C00000Bu, paramLayout_129, 7}, {0X0C00000Cu, paramLayout_130, 7},
    {0X0C00000Du, paramLayout_131, 7},  {0X0C00000Eu, paramLayout_132, 7}, {0X0C000011u, paramLayout_129, 7},
    {0X0C000012u, paramLayout_129, 7},  {0X0F000000u, paramLayout_050, 5}, {0X0F000001u, paramLayout_050, 5},
    {0X0F000002u, paramLayout_050, 5},  {0X0F000003u, paramLayout_050, 5}, {0X0F000004u, paramLayout_050, 5},
    {0X0F000005u, paramLayout_050, 5},  {0X0F000006u, paramLayout_050, 5}, {0X0F000007u, paramLayout_050, 5},
    {0X0F000008u, paramLayout_050, 5},  {0X0F000009u, paramLayout_050, 5},
};

juce::String formatFloatValue (float value)
{
    if (!std::isfinite (value))
        return "0";

    const auto rounded = std::round (value);

    if (std::abs (value - rounded) < 0.05f)
        return juce::String (static_cast<int> (rounded));

    return juce::String (value, 1);
}
} // namespace

const GP200EffectParamSet* GP200EffectParamDatabase::findParamsForEffect (juce::uint32 effectId)
{
    const auto it = std::lower_bound (std::begin (paramSets),
                                      std::end (paramSets),
                                      effectId,
                                      [] (const GP200EffectParamSet& set, juce::uint32 wantedId)
                                      { return set.effectId < wantedId; });

    return it != std::end (paramSets) && it->effectId == effectId ? it : nullptr;
}

const GP200EffectParamInfo* GP200EffectParamDatabase::findParam (juce::uint32 effectId, int paramIndex)
{
    if (const auto* set = findParamsForEffect (effectId))
    {
        for (int i = 0; i < set->count; ++i)
        {
            if (set->params[i].idx == paramIndex)
                return &set->params[i];
        }
    }

    return nullptr;
}

juce::String GP200EffectParamDatabase::getParamName (juce::uint32 effectId, int paramIndex)
{
    if (const auto* param = findParam (effectId, paramIndex))
        return param->name;

    return "P" + juce::String (paramIndex);
}

juce::String GP200EffectParamDatabase::formatKnownParams (juce::uint32 effectId,
                                                          const std::array<float, effectParamCount>& values,
                                                          int maxParamsToShow)
{
    const auto* set = findParamsForEffect (effectId);

    if (set == nullptr || set->count <= 0)
    {
        return "P0=" + formatFloatValue (values[0]) + " P1=" + formatFloatValue (values[1]) +
               " P2=" + formatFloatValue (values[2]);
    }

    juce::String text;
    const auto countToShow = juce::jmin (set->count, maxParamsToShow);

    for (int i = 0; i < countToShow; ++i)
    {
        const auto& param = set->params[i];

        if (param.idx < 0 || param.idx >= static_cast<int> (values.size ()))
            continue;

        if (text.isNotEmpty ())
            text << "  ";

        text << param.name << "=" << formatFloatValue (values[static_cast<std::size_t> (param.idx)]);
    }

    if (set->count > countToShow)
        text << "  ...";

    return text;
}
} // namespace gp200
