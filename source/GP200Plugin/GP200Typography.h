#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

namespace gp200ui
{
inline juce::Typeface::Ptr regularTypeface ()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor (
        BinaryData::SpaceGroteskRegular_ttf,
        BinaryData::SpaceGroteskRegular_ttfSize);
    return typeface;
}

inline juce::Typeface::Ptr mediumTypeface ()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor (
        BinaryData::SpaceGroteskMedium_ttf,
        BinaryData::SpaceGroteskMedium_ttfSize);
    return typeface;
}

inline juce::Typeface::Ptr semiboldTypeface ()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor (
        BinaryData::SpaceGroteskSemiBold_ttf,
        BinaryData::SpaceGroteskSemiBold_ttfSize);
    return typeface;
}

inline juce::Font makeFont (const juce::Typeface::Ptr& typeface,
                            float height)
{
    if (typeface != nullptr)
        return juce::Font (juce::FontOptions (typeface).withHeight (height));

    return juce::Font (juce::FontOptions (height));
}

inline juce::Font regular (float height)
{
    return makeFont (regularTypeface (), height);
}

inline juce::Font medium (float height)
{
    return makeFont (mediumTypeface (), height);
}

inline juce::Font semibold (float height)
{
    return makeFont (semiboldTypeface (), height);
}

class SpaceGroteskLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Font getTextButtonFont (juce::TextButton& button,
                                  int buttonHeight) override
    {
        const auto text = button.getButtonText ();

        // Preserve the current size of the three principal actions.
        if (text == "Save to DAW"
            || text == "Recall from DAW"
            || text == "Store to GP-200")
        {
            const auto height = juce::jlimit (13.0f, 18.5f,
                                              static_cast<float> (buttonHeight) * 0.44f);
            return text == "Store to GP-200" ? semibold (height)
                                               : medium (height);
        }

        // The remaining controls have enough room for a slightly larger face.
        const auto height = juce::jlimit (14.75f, 20.5f,
                                          static_cast<float> (buttonHeight) * 0.48f);
        return medium (height);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return medium (16.5f);
    }

    juce::Font getPopupMenuFont () override
    {
        return regular (16.5f);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return regular (15.5f);
    }
};
} // namespace gp200ui
