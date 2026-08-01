#include "TunerDisplayComponent.h"
#include "GP200Typography.h"

#include <algorithm>
#include <cmath>

namespace
{
const juce::Colour panelColour{0xff202528};
const juce::Colour panelOutlineColour{0xffffa42a};
const juce::Colour textColour{0xffffffff};
const juce::Colour mutedTextColour{0xffaeb3b5};
const juce::Colour inTuneColour{0xff57f05f};
}

void TunerDisplayComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(panelColour);
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(juce::Colour(0xff44484a));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    if (!hasValidReading)
    {
        g.setFont (gp200ui::regular (15.25f));
        g.setColour(mutedTextColour);

        g.drawText("Play a note",
                   getLocalBounds(),
                   juce::Justification::centred);

        return;
    }

    auto contentBounds = getLocalBounds();

auto noteBounds = contentBounds.removeFromLeft(88);

g.setFont (gp200ui::semibold (26.0f));
g.setColour(textColour);

g.drawText(noteText,
           noteBounds,
           juce::Justification::centred);

auto topBounds = contentBounds.removeFromTop(16);

    g.setFont (gp200ui::regular (13.5f));
    g.setColour(mutedTextColour);

    g.drawText(juce::String(frequency, 2) + " Hz",
               topBounds.removeFromLeft(90),
               juce::Justification::centredLeft);

    const auto centsText =
        (centsOffset >= 0.0f ? "+" : "") +
        juce::String(centsOffset, 1) +
        " cents";

    g.drawText(centsText,
               topBounds,
               juce::Justification::centredRight);

    auto meterBounds =
        contentBounds.reduced(8, 4).toFloat();

    const float centreX = meterBounds.getCentreX();

    g.setColour(juce::Colour(0xff555b5e));
    g.drawHorizontalLine(
        static_cast<int>(meterBounds.getCentreY()),
        meterBounds.getX(),
        meterBounds.getRight());

    g.setColour(mutedTextColour.withAlpha(0.7f));

    g.drawVerticalLine(
        static_cast<int>(centreX),
        meterBounds.getY(),
        meterBounds.getBottom());

    const float clampedCents =
        std::clamp(centsOffset, -50.0f, 50.0f);

    const float normalized =
        (clampedCents + 50.0f) / 100.0f;

    const float markerX =
        meterBounds.getX() +
        normalized * meterBounds.getWidth();

    const bool isInTune =
        std::abs(centsOffset) <= 3.0f;

    g.setColour(isInTune
                    ? inTuneColour
                    : panelOutlineColour);

    g.fillEllipse(markerX - 4.0f,
                  meterBounds.getCentreY() - 4.0f,
                  8.0f,
                  8.0f);
}

void TunerDisplayComponent::setTunerReading(
    const juce::String& noteName,
    float frequencyHz,
    float cents,
    bool valid)
{
    noteText = noteName;
    frequency = frequencyHz;
    centsOffset = cents;
    hasValidReading = valid;

    repaint();
}

void TunerDisplayComponent::clearReading()
{
    noteText = "--";
    frequency = 0.0f;
    centsOffset = 0.0f;
    hasValidReading = false;

    repaint();
}