#include "EffectBlockComponent.h"

#include <cmath>
#include <utility>

namespace
{
const juce::Colour backgroundColour{0xff252525};
const juce::Colour expandedBackgroundColour{0xff2d2d2d};
const juce::Colour textColour{0xffffffff};
const juce::Colour mutedTextColour{0xffb5b5b5};
const juce::Colour darkButtonColour{0xff111111};

juce::Colour colourForSlotIndex (int slotIndex)
{
    switch (slotIndex)
    {
    case 0:
        return juce::Colour (0xffffb12b); // PRE
    case 1:
        return juce::Colour (0xffd761ff); // WAH
    case 2:
        return juce::Colour (0xffff5050); // DST
    case 3:
        return juce::Colour (0xffff8a2a); // AMP
    case 4:
        return juce::Colour (0xff9aa6a6); // NR
    case 5:
        return juce::Colour (0xff3be07d); // CAB
    case 6:
        return juce::Colour (0xff35c8ff); // EQ
    case 7:
        return juce::Colour (0xff4f8dff); // MOD
    case 8:
        return juce::Colour (0xff7068ff); // DLY
    case 9:
        return juce::Colour (0xffb96cff); // RVB
    case 10:
        return juce::Colour (0xffb7b7b7); // VOL
    default:
        return juce::Colour (0xffffa42a);
    }
}

juce::String cleanAssignmentDisplayText (const juce::String& text)
{
    auto cleaned = text.trim ();

    // GP-200 assignment names can contain a duplicated suffix after markers like
    // "!" or repeated shortened text, for example:
    // "MARSHALL 412 BL !12 BL" -> "MARSHALL 412 BL"
    const auto bangIndex = cleaned.indexOfChar ('!');

    if (bangIndex >= 0)
        cleaned = cleaned.substring (0, bangIndex).trim ();

    // Some SnapTone names can arrive with an extra trailing short token.
    // Example: "VH4 3rd DIEZEL EZEL" -> "VH4 3rd DIEZEL"
    const auto lastSpace = cleaned.lastIndexOfChar (' ');

    if (lastSpace > 0)
    {
        const auto lastToken = cleaned.substring (lastSpace + 1).trim ();
        const auto previousText = cleaned.substring (0, lastSpace).trim ();

        if (lastToken.length () >= 3 && previousText.toUpperCase ().contains (lastToken.toUpperCase ()))
        {
            cleaned = previousText;
        }
    }

    return cleaned;
}

static constexpr int delaySyncTimeLabelCount = 11;
static constexpr juce::uint32 hammyEffectId = 0x01000049u;
static constexpr int hammyRangeLabelCount = 6;

juce::String getHammyRangeLabel (int index)
{
    static const char* labels[hammyRangeLabelCount] = {
        "-2 Oct", "-1 Oct", "+1 Oct", "+2 Oct", "+/-1 Oct", "+/-2 Oct"};

    return labels[juce::jlimit (0, hammyRangeLabelCount - 1, index)];
}

juce::String getHammyHarmonyLabel (int index)
{
    return index <= 0 ? "OFF" : "ON";
}

juce::String getDelaySyncTimeLabel (int index)
{
    static const char* labels[delaySyncTimeLabelCount] = {
        "1/1", "1/2", "1/2D", "1/2T", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16"};

    const auto safeIndex = juce::jlimit (0, delaySyncTimeLabelCount - 1, index);

    return labels[safeIndex];
}

bool isDelayTimeParameter (const gp200::GP200EffectParamInfo& param)
{
    return param.idx == 1 && juce::String (param.name).containsIgnoreCase ("Time");
}

int findDelayTimeSyncParamIndex (const gp200::GP200EffectParamSet& paramSet)
{
    for (int i = 0; i < paramSet.count; ++i)
    {
        const auto name = juce::String (paramSet.params[i].name);

        if (name.equalsIgnoreCase ("Time Sync"))
            return paramSet.params[i].idx;
    }

    for (int i = 0; i < paramSet.count; ++i)
    {
        const auto name = juce::String (paramSet.params[i].name);

        if (name.equalsIgnoreCase ("Sync"))
            return paramSet.params[i].idx;
    }

    return -1;
}

bool isDelayTimeSyncEnabled (const gp200::GP200EffectSlot& effect, const gp200::GP200EffectParamSet& paramSet)
{
    const auto syncParamIndex = findDelayTimeSyncParamIndex (paramSet);

    if (syncParamIndex < 0 || syncParamIndex >= static_cast<int> (effect.params.size ()))
        return false;

    return effect.params[static_cast<std::size_t> (syncParamIndex)] >= 0.5f;
}
} // namespace

//==============================================================================
EffectBlockComponent::EffectBlockComponent (const gp200::GP200EffectSlot& effectToShow,
                                            EffectDisplayNameResolver displayNameResolver)
    : effect (effectToShow), effectDisplayNameResolver (std::move (displayNameResolver))
{
    auto setupMoveButton = [] (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff111111));
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1b1b1b));
        button.setColour (juce::TextButton::textColourOffId, mutedTextColour);
        button.setColour (juce::TextButton::textColourOnId, mutedTextColour);
    };

    setupMoveButton (moveUpButton);
    setupMoveButton (moveDownButton);
    setupMoveButton (previousEffectButton);
    setupMoveButton (nextEffectButton);

    addAndMakeVisible (moveUpButton);
    addAndMakeVisible (moveDownButton);
    addAndMakeVisible (previousEffectButton);
    addAndMakeVisible (nextEffectButton);
    addAndMakeVisible (effectSelector);
    addAndMakeVisible (effectNameLabel);
    addAndMakeVisible (effectDescriptionLabel);

    moveUpButton.onClick = [this]
    {
        if (onMoveRequested)
            onMoveRequested (getBlockIndex (), -1);
    };

    moveDownButton.onClick = [this]
    {
        if (onMoveRequested)
            onMoveRequested (getBlockIndex (), 1);
    };

    previousEffectButton.onClick = [this] { changeEffectByOffset (-1); };

    nextEffectButton.onClick = [this] { changeEffectByOffset (1); };

    effectSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1b1b1b));
    effectSelector.setColour (juce::ComboBox::textColourId, textColour);
    effectSelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0x00000000));
    effectSelector.setColour (juce::ComboBox::arrowColourId, mutedTextColour);
    effectSelector.setColour (juce::ComboBox::buttonColourId, juce::Colour (0xff1b1b1b));

    effectNameLabel.setInterceptsMouseClicks (false, false);
    effectNameLabel.setJustificationType (juce::Justification::centredLeft);
    effectNameLabel.setFont (juce::Font (13.0f));
    effectNameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1b1b1b));
    effectNameLabel.setColour (juce::Label::textColourId, textColour);

    effectDescriptionLabel.setInterceptsMouseClicks (false, false);
    effectDescriptionLabel.setJustificationType (juce::Justification::centredLeft);
    effectDescriptionLabel.setFont (juce::Font (11.0f));
    effectDescriptionLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1b1b1b));
    effectDescriptionLabel.setColour (juce::Label::textColourId, mutedTextColour.withAlpha (0.72f));

    rebuildParameterControls ();
    rebuildEffectChoices ();
    updateParameterControlsVisibility ();
    updateEffectDescriptionLabel ();

    effectSelector.onChange = [this]
    {
        if (updatingEffectSelector)
            return;

        const auto selectedIndex = effectSelector.getSelectedItemIndex ();

        if (selectedIndex < 0 || selectedIndex >= static_cast<int> (effectChoiceIds.size ()))
            return;

        const auto selectedEffectId = effectChoiceIds[static_cast<std::size_t> (selectedIndex)];

        if (selectedEffectId == effect.effectId)
            return;

        if (onEffectChangeRequested)
            onEffectChangeRequested (getBlockIndex (), selectedEffectId);
    };

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void EffectBlockComponent::updateEffect (const gp200::GP200EffectSlot& newEffect)
{
    effect = newEffect;
    rebuildParameterControls ();
    updateEffectDescriptionLabel ();
    rebuildEffectChoices ();
    updateParameterControlsVisibility ();
    resized ();
    repaint ();

    if (onHeightChanged)
        onHeightChanged ();
}

void EffectBlockComponent::resized ()
{
    // Header only: align move buttons, module tag, selector and ON/OFF.
    // Do not change the parameter area.
    moveUpButton.setBounds (10, 17, 24, 20);
    moveDownButton.setBounds (38, 17, 24, 20);

    const auto selectorBounds = getEffectSelectorBounds ();

    previousEffectButton.setBounds (
        selectorBounds.getX () - 28, selectorBounds.getY (), 24, selectorBounds.getHeight ());

    effectSelector.setBounds (selectorBounds);

    // These labels sit above the ComboBox text area.
    // They do not intercept clicks, so the ComboBox still opens normally.
    effectNameLabel.setBounds (
        selectorBounds.getX () + 6, selectorBounds.getY () + 1, 170, selectorBounds.getHeight () - 2);

    effectDescriptionLabel.setBounds (selectorBounds.getX () + 176,
                                      selectorBounds.getY () + 1,
                                      selectorBounds.getWidth () - 210,
                                      selectorBounds.getHeight () - 2);

    nextEffectButton.setBounds (
        selectorBounds.getRight () + 6, selectorBounds.getY (), 24, selectorBounds.getHeight ());

    auto y = headerHeight + verticalPadding;

    for (auto& control : parameterControls)
    {
        if (control.label != nullptr)
            control.label->setBounds (24, y, 150, 24);

        if (control.slider != nullptr)
            control.slider->setBounds (184, y, getWidth () - 208, 24);

        y += paramControlHeight;
    }
}

void EffectBlockComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds ().reduced (1);
    const auto blockColour = getBlockColour ();

    g.setColour (expanded ? expandedBackgroundColour : backgroundColour);
    g.fillRoundedRectangle (bounds.toFloat (), 7.0f);

    g.setColour (blockColour.withAlpha (expanded ? 0.95f : 0.45f));
    g.drawRoundedRectangle (bounds.toFloat (), 7.0f, expanded ? 1.6f : 1.0f);

    auto header = bounds.withHeight (headerHeight);

    auto tagBounds = juce::Rectangle<int>{72, 16, 58, 28};

    g.setColour (blockColour.withAlpha (0.18f));
    g.fillRoundedRectangle (tagBounds.toFloat (), 4.0f);

    g.setColour (blockColour);
    g.drawRoundedRectangle (tagBounds.toFloat (), 4.0f, 1.2f);

    g.setFont (12.0f);
    g.drawText (getBlockName (), tagBounds, juce::Justification::centred);

    g.setColour (mutedTextColour);
    g.setFont (13.0f);
    g.drawText (expanded ? "v" : ">",
                header.withX (getWidth () - 250).withWidth (25).withHeight (28).withY (header.getY () + 10),
                juce::Justification::centred);

    auto onOffBounds = getOnOffBounds ();

    g.setColour (effect.enabled ? blockColour.withAlpha (0.22f) : darkButtonColour);
    g.fillRoundedRectangle (onOffBounds.toFloat (), 4.0f);

    g.setColour (effect.enabled ? blockColour : juce::Colour (0xff707070));
    g.drawRoundedRectangle (onOffBounds.toFloat (), 4.0f, 1.1f);

    g.setFont (11.5f);
    g.drawText (effect.enabled ? "ON" : "OFF", onOffBounds, juce::Justification::centred);

    if (draggingForReorder)
    {
        const auto dragBounds = header.reduced (4);

        g.setColour (blockColour.withAlpha (0.16f));
        g.fillRoundedRectangle (dragBounds.toFloat (), 8.0f);

        g.setColour (blockColour);
        g.drawRoundedRectangle (dragBounds.toFloat (), 8.0f, 2.6f);
    }

    if (!expanded)
        return;

    g.setColour (juce::Colour (0xff454545));
    g.drawHorizontalLine (headerHeight, 18.0f, static_cast<float> (getWidth () - 18));

    if (parameterControls.empty ())
    {
        const auto y = headerHeight + verticalPadding;

        g.setColour (mutedTextColour);
        g.setFont (13.0f);
        g.drawText ("No editable parameters available.",
                    20,
                    y,
                    getWidth () - 40,
                    22,
                    juce::Justification::centredLeft);
    }
}

void EffectBlockComponent::mouseDown (const juce::MouseEvent& event)
{
    const auto position = event.getPosition ();

    draggingForReorder = false;

    dragCandidate = position.y <= headerHeight && !getOnOffBounds ().contains (position) &&
                    !getEffectSelectorBounds ().contains (position) &&
                    !moveUpButton.getBounds ().contains (position) &&
                    !moveDownButton.getBounds ().contains (position) &&
                    !previousEffectButton.getBounds ().contains (position) &&
                    !nextEffectButton.getBounds ().contains (position);
}

void EffectBlockComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (!dragCandidate)
        return;

    if (!draggingForReorder && event.getDistanceFromDragStart () > 6)
    {
        draggingForReorder = true;

        setAlpha (0.90f);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);

        repaint ();
    }

    if (draggingForReorder && onDragReorderPreview)
    {
        const auto contentY = getY () + event.getPosition ().getY ();

        onDragReorderPreview (getBlockIndex (), contentY);
    }
}

void EffectBlockComponent::mouseUp (const juce::MouseEvent& event)
{
    if (draggingForReorder)
    {
        setAlpha (1.0f);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);

        const auto contentY = getY () + event.getPosition ().getY ();

        dragCandidate = false;
        draggingForReorder = false;

        repaint ();

        if (onDragReorderRequested)
            onDragReorderRequested (getBlockIndex (), contentY);

        return;
    }

    dragCandidate = false;
    draggingForReorder = false;
    setAlpha (1.0f);
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint ();

    if (getOnOffBounds ().contains (event.getPosition ()))
    {
        if (onToggleRequested)
            onToggleRequested (getBlockIndex (), !effect.enabled);

        return;
    }

    if (event.position.y <= static_cast<float> (headerHeight))
    {
        setExpanded (!expanded);

        if (onHeightChanged)
            onHeightChanged ();
    }
}

int EffectBlockComponent::getPreferredHeight (int) const
{
    if (!expanded)
        return headerHeight;

    const auto parameterCount = juce::jmax (1, static_cast<int> (parameterControls.size ()));

    return headerHeight + verticalPadding + parameterCount * paramControlHeight + verticalPadding;
}

bool EffectBlockComponent::isExpanded () const
{
    return expanded;
}

void EffectBlockComponent::setExpanded (bool shouldBeExpanded)
{
    if (expanded == shouldBeExpanded)
        return;

    expanded = shouldBeExpanded;
    updateParameterControlsVisibility ();
    resized ();
    repaint ();

    if (onHeightChanged)
        onHeightChanged ();
}

int EffectBlockComponent::getSlotIndex () const
{
    if (effect.blockIndex >= 0)
        return effect.blockIndex;

    return effect.slotIndex;
}

int EffectBlockComponent::getBlockIndex () const
{
    if (effect.blockIndex >= 0)
        return effect.blockIndex;

    return effect.slotIndex;
}

void EffectBlockComponent::setEnabledForDisplay (bool shouldBeEnabled)
{
    if (effect.enabled == shouldBeEnabled)
        return;

    effect.enabled = shouldBeEnabled;
    repaint ();
}

void EffectBlockComponent::setParameterValueForDisplay (int paramIndex, float value)
{
    if (paramIndex < 0 || paramIndex >= static_cast<int> (effect.params.size ()))
        return;

    effect.params[static_cast<std::size_t> (paramIndex)] = value;

    const auto* paramSet = gp200::GP200EffectParamDatabase::findParamsForEffect (effect.effectId);

    if (getBlockName ().equalsIgnoreCase ("DLY") && paramSet != nullptr)
    {
        const auto syncParamIndex = findDelayTimeSyncParamIndex (*paramSet);

        if (paramIndex == syncParamIndex)
        {
            // The MIDI echo for Sync can arrive while the Sync slider is still
            // inside juce::Slider::mouseUp(). Rebuilding here would destroy that
            // slider (and its popup) before JUCE finishes the mouse event.
            scheduleDelaySyncControlRebuild ();
            return;
        }
    }

    for (auto& control : parameterControls)
    {
        if (control.paramIndex == paramIndex && control.slider != nullptr)
        {
            if (std::abs (control.slider->getValue () - static_cast<double> (value)) > 0.0001)
                control.slider->setValue (value, juce::dontSendNotification);

            // A MIDI echo can return the same numeric value already held by the
            // slider. In that case setValue() is intentionally skipped, but the
            // text box may still contain JUCE's temporary numeric edit text
            // (notably "0"). Refresh Hammy's labelled values explicitly.
            if (effect.effectId == hammyEffectId && (paramIndex == 0 || paramIndex == 1))
                control.slider->updateText ();

            break;
        }
    }
}

void EffectBlockComponent::setMoveButtonsEnabled (bool canMoveUp, bool canMoveDown)
{
    moveUpButton.setEnabled (canMoveUp);
    moveDownButton.setEnabled (canMoveDown);
}

juce::Rectangle<int> EffectBlockComponent::getOnOffBounds () const
{
    return {getWidth () - 62, 16, 50, 28};
}

juce::Rectangle<int> EffectBlockComponent::getEffectSelectorBounds () const
{
    const auto selectorX = 178;
    const auto selectorY = 16;
    const auto selectorH = 28;

    const auto rightEdge = getOnOffBounds ().getX () - 34;
    const auto selectorW = juce::jmax (240, rightEdge - selectorX);

    return {selectorX, selectorY, selectorW, selectorH};
}

namespace
{
float getParamMinimum (const gp200::GP200EffectParamInfo& param)
{
    return param.maxValue > param.minValue ? param.minValue : 0.0f;
}

float getParamMaximum (const gp200::GP200EffectParamInfo& param)
{
    if (param.maxValue > param.minValue)
        return param.maxValue;

    switch (param.kind)
    {
    case gp200::GP200ParamKind::toggle:
        return 1.0f;

    case gp200::GP200ParamKind::choice:
        // The source table does not yet include a choice count. Preserve
        // the previous fallback while allowing defaults above 10.
        return juce::jmax (10.0f, param.defaultValue);

    case gp200::GP200ParamKind::continuous:
        return 100.0f;
    }

    return 100.0f;
}

float getParamStep (const gp200::GP200EffectParamInfo& param, float minimum, float maximum)
{
    if (param.step > 0.0f)
        return param.step;

    if (param.kind == gp200::GP200ParamKind::toggle || param.kind == gp200::GP200ParamKind::choice)
    {
        return 1.0f;
    }

    return (maximum - minimum) <= 10.0f ? 0.1f : 1.0f;
}
} // namespace


void EffectBlockComponent::scheduleDelaySyncControlRebuild ()
{
    if (delaySyncControlRebuildPending)
        return;

    delaySyncControlRebuildPending = true;

    juce::Component::SafePointer<EffectBlockComponent> safeThis (this);

    juce::MessageManager::callAsync ([safeThis]
    {
        if (safeThis == nullptr)
            return;

        safeThis->delaySyncControlRebuildPending = false;
        safeThis->rebuildParameterControls ();
        safeThis->updateParameterControlsVisibility ();
        safeThis->resized ();
        safeThis->repaint ();

        if (safeThis->onHeightChanged)
            safeThis->onHeightChanged ();
    });
}

void EffectBlockComponent::rebuildParameterControls ()
{
    parameterControls.clear ();

    const auto* paramSet = gp200::GP200EffectParamDatabase::findParamsForEffect (effect.effectId);

    if (paramSet == nullptr || paramSet->count <= 0)
        return;

    for (int i = 0; i < paramSet->count; ++i)
    {
        const auto& param = paramSet->params[i];

        if (param.idx < 0 || param.idx >= static_cast<int> (effect.params.size ()))
            continue;

        ParameterControl control;
        control.paramIndex = param.idx;
        control.name = param.name;

        control.label = std::make_unique<juce::Label> ();
        control.label->setText (control.name, juce::dontSendNotification);
        control.label->setJustificationType (juce::Justification::centredLeft);
        control.label->setColour (juce::Label::textColourId, textColour);
        control.label->setFont (juce::Font (13.0f));

        control.slider = std::make_unique<juce::Slider> ();
        control.slider->setSliderStyle (juce::Slider::LinearHorizontal);
        control.slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
        control.slider->setColour (juce::Slider::textBoxTextColourId, textColour);
        control.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1b1b1b));
        control.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff454545));
        control.slider->setColour (juce::Slider::thumbColourId, getBlockColour ());
        control.slider->setColour (juce::Slider::trackColourId, getBlockColour ().withAlpha (0.45f));
        control.slider->setColour (juce::Slider::backgroundColourId, juce::Colour (0xff151515));

        const auto isDelayBlock = getBlockName ().equalsIgnoreCase ("DLY");
        const auto isSyncedDelayTime =
            isDelayBlock && isDelayTimeParameter (param) && isDelayTimeSyncEnabled (effect, *paramSet);

        if (isSyncedDelayTime)
        {
            control.slider->setRange (0.0, 10.0, 1.0);
            control.slider->setDoubleClickReturnValue (true, 4.0);
            control.slider->setNumDecimalPlacesToDisplay (0);

            control.slider->textFromValueFunction = [] (double value)
            { return getDelaySyncTimeLabel (static_cast<int> (std::round (value))); };

            const auto displayValue = juce::jlimit (
                0.0,
                10.0,
                static_cast<double> (juce::roundToInt (effect.params[static_cast<std::size_t> (param.idx)])));

            control.slider->setValue (displayValue, juce::dontSendNotification);
        }
        else
        {
            const auto isHammy = effect.effectId == hammyEffectId;

            if (isHammy && param.idx == 0)
            {
                control.slider->setRange (0.0, 5.0, 1.0);
                control.slider->setDoubleClickReturnValue (true, 2.0);
                control.slider->setNumDecimalPlacesToDisplay (0);
                control.slider->textFromValueFunction = [] (double value)
                { return getHammyRangeLabel (juce::roundToInt (value)); };
            }
            else if (isHammy && param.idx == 1)
            {
                control.slider->setRange (0.0, 1.0, 1.0);
                control.slider->setDoubleClickReturnValue (true, 0.0);
                control.slider->setNumDecimalPlacesToDisplay (0);
                control.slider->textFromValueFunction = [] (double value)
                { return getHammyHarmonyLabel (juce::roundToInt (value)); };
            }
            else
            {
                const auto minimum = getParamMinimum (param);
                const auto maximum = getParamMaximum (param);
                const auto step = getParamStep (param, minimum, maximum);

                control.slider->setRange (minimum, maximum, step);
                control.slider->setDoubleClickReturnValue (true, param.defaultValue);

                if (isDelayBlock && isDelayTimeParameter (param))
                {
                    control.slider->textFromValueFunction = [] (double value)
                    { return juce::String (static_cast<int> (std::round (value))) + " ms"; };
                }
            }

            control.slider->setValue (effect.params[static_cast<std::size_t> (param.idx)],
                                      juce::dontSendNotification);
        }

        auto* slider = control.slider.get ();
        const auto paramIndex = param.idx;

        slider->onValueChange = [this, slider, paramIndex]
        {
            const auto value = static_cast<float> (slider->getValue ());

            if (paramIndex >= 0 && paramIndex < static_cast<int> (effect.params.size ()))
                effect.params[static_cast<std::size_t> (paramIndex)] = value;

            if (!slider->isMouseButtonDown ())
            {
                if (onParameterChangeRequested)
                    onParameterChangeRequested (getBlockIndex (), paramIndex, effect.effectId, value);
            }
        };

        slider->onDragEnd = [this, slider, paramIndex]
        {
            if (onParameterChangeRequested)
                onParameterChangeRequested (
                    getBlockIndex (), paramIndex, effect.effectId, static_cast<float> (slider->getValue ()));
        };

        addAndMakeVisible (*control.label);
        addAndMakeVisible (*control.slider);

        parameterControls.push_back (std::move (control));
    }
}

void EffectBlockComponent::updateParameterControlsVisibility ()
{
    for (auto& control : parameterControls)
    {
        if (control.label != nullptr)
            control.label->setVisible (expanded);

        if (control.slider != nullptr)
            control.slider->setVisible (expanded);
    }
}

void EffectBlockComponent::updateEffectDescriptionLabel ()
{
    auto name = getEffectName ().trim ();
    auto description = gp200::GP200EffectDatabase::getEffectDescription (effect.effectId).trim ();

    const auto separatorIndex = name.indexOf (" - ");

    if (separatorIndex >= 0)
    {
        const auto customName = cleanAssignmentDisplayText (name.substring (separatorIndex + 3));

        name = name.substring (0, separatorIndex).trim ();

        if (customName.isNotEmpty ())
            description = customName;
    }

    effectNameLabel.setText (name, juce::dontSendNotification);
    effectNameLabel.setVisible (name.isNotEmpty ());

    effectDescriptionLabel.setText (description, juce::dontSendNotification);
    effectDescriptionLabel.setVisible (description.isNotEmpty ());
}

void EffectBlockComponent::changeEffectByOffset (int offset)
{
    if (effectChoiceIds.empty ())
        return;

    int currentIndex = -1;

    for (int i = 0; i < static_cast<int> (effectChoiceIds.size ()); ++i)
    {
        if (effectChoiceIds[static_cast<std::size_t> (i)] == effect.effectId)
        {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex < 0)
        currentIndex = effectSelector.getSelectedItemIndex ();

    if (currentIndex < 0)
        return;

    const auto count = static_cast<int> (effectChoiceIds.size ());

    if (count <= 1)
        return;

    auto newIndex = currentIndex + offset;

    while (newIndex < 0)
        newIndex += count;

    while (newIndex >= count)
        newIndex -= count;

    if (newIndex == currentIndex)
        return;

    const auto selectedEffectId = effectChoiceIds[static_cast<std::size_t> (newIndex)];

    if (selectedEffectId == effect.effectId)
        return;

    effectSelector.setSelectedItemIndex (newIndex, juce::dontSendNotification);

    if (onEffectChangeRequested)
        onEffectChangeRequested (getBlockIndex (), selectedEffectId);
}

void EffectBlockComponent::rebuildEffectChoices ()
{
    updatingEffectSelector = true;

    effectSelector.clear (juce::dontSendNotification);
    effectChoiceIds.clear ();

    const auto moduleName = getBlockName ();
    auto choices = gp200::GP200EffectDatabase::getEffectsForModule (moduleName);

    bool hasCurrentEffect = false;

    for (const auto& info : choices)
    {
        if (info.effectId == effect.effectId)
        {
            hasCurrentEffect = true;
            break;
        }
    }

    if (!hasCurrentEffect)
    {
        if (const auto* currentInfo = gp200::GP200EffectDatabase::findEffect (effect.effectId))
            choices.insert (choices.begin (), *currentInfo);
    }

    for (const auto& info : choices)
    {
        effectChoiceIds.push_back (info.effectId);

        auto menuName = getEffectDisplayName (info.effectId, info.name).trim ();
        auto menuDescription = gp200::GP200EffectDatabase::getEffectDescription (info.effectId).trim ();

        const auto separatorIndex = menuName.indexOf (" - ");

        if (separatorIndex >= 0)
        {
            const auto customName = cleanAssignmentDisplayText (menuName.substring (separatorIndex + 3));

            menuName = menuName.substring (0, separatorIndex).trim ();

            if (customName.isNotEmpty ())
                menuDescription = customName;
        }

        auto menuText = menuName;

        if (menuDescription.isNotEmpty ())
            menuText << "    " << menuDescription;

        effectSelector.addItem (menuText, static_cast<int> (effectChoiceIds.size ()));
    }

    int selectedIndex = -1;

    for (std::size_t i = 0; i < effectChoiceIds.size (); ++i)
    {
        if (effectChoiceIds[i] == effect.effectId)
        {
            selectedIndex = static_cast<int> (i);
            break;
        }
    }

    if (selectedIndex >= 0)
        effectSelector.setSelectedItemIndex (selectedIndex, juce::dontSendNotification);
    else
        effectSelector.setText (getEffectName (), juce::dontSendNotification);

    previousEffectButton.setEnabled (effectChoiceIds.size () > 1);
    nextEffectButton.setEnabled (effectChoiceIds.size () > 1);
    updateEffectDescriptionLabel ();
    updatingEffectSelector = false;
}

juce::Colour EffectBlockComponent::getBlockColour () const
{
    return colourForSlotIndex (getSlotIndex ());
}

juce::String EffectBlockComponent::getBlockName () const
{
    return gp200::GP200PresetCodec::blockNameForSlotIndex (getSlotIndex ());
}

juce::String EffectBlockComponent::getEffectName () const
{
    return getEffectDisplayName (effect.effectId, gp200::GP200PresetCodec::effectNameForId (effect.effectId));
}

juce::String EffectBlockComponent::getEffectDisplayName (juce::uint32 effectId,
                                                         const juce::String& fallbackName) const
{
    if (effectDisplayNameResolver)
    {
        const auto resolvedName = effectDisplayNameResolver (effectId).trim ();

        if (resolvedName.isNotEmpty ())
            return resolvedName;
    }

    return fallbackName;
}
