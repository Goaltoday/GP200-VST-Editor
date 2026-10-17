juce::String EffectBlockComponent::getEffectName () const
{
    if (blendForDisplay) return "Blend A / B";
    const auto moduleAwareName = gp200::GP200EffectDatabase::getEffectName (effect.effectId, getBlockName ());
    if (getBlockName ().equalsIgnoreCase ("PRE")
        && moduleAwareName != gp200::GP200EffectDatabase::getEffectName (effect.effectId))
        return moduleAwareName;
    return getEffectDisplayName (effect.effectId, moduleAwareName);
}
juce::String EffectBlockComponent::getEffectDisplayName (juce::uint32 effectId,
                                                         const juce::String& fallbackName) const
{
    if (const auto entry = gp200::GP200ModSync::findEffectBankEntry (effectId, getBlockName ()))
        return entry->displayName;
    if (effectDisplayNameResolver)
    {
        const auto resolvedName = effectDisplayNameResolver (effectId).trim ();

        if (resolvedName.isNotEmpty ())
            return resolvedName;
    }

    return fallbackName;
}
bool usesDelayTimeControls (const gp200::GP200EffectSlot& effect,
                            const juce::String& blockName)
{
    // Pure keeps the PRE target ID accepted by the hardware, but its adapted
    // parameter ABI is the same as the Pure delay. Treat only this relocated
    // algorithm as a delay for the Time/Sync presentation.
    if (blockName.equalsIgnoreCase ("DLY")) return true;
    if (const auto entry = gp200::GP200ModSync::findEffectBankEntry (effect.effectId, blockName))
        return entry->sourceModule == "DLY";
    if (! blockName.equalsIgnoreCase ("PRE")) return false;
    const auto dynamicModule = gp200::GP200ModSync::getPreBankSourceModule (effect.effectId);
    return dynamicModule.equalsIgnoreCase ("DLY");
}
bool isDelayTimeParameter (const gp200::GP200EffectParamInfo& param)
{
    const auto name = juce::String (param.name);
    return name.equalsIgnoreCase ("Time") || name.equalsIgnoreCase ("Time A") || name.equalsIgnoreCase ("Time B");
}
int findDelayTimeSyncParamIndex (const gp200::GP200EffectParamSet& paramSet, int timeParamIndex = -1)
{
    if (timeParamIndex >= 0)
        for (int i = 0; i < paramSet.count; ++i)
            if (paramSet.params[i].idx == timeParamIndex)
            {
                const auto name = juce::String (paramSet.params[i].name);
                const auto wanted = name.equalsIgnoreCase ("Time A") ? "A Sync"
                                  : name.equalsIgnoreCase ("Time B") ? "B Sync" : "";
                if (wanted[0] != 0)
                    for (int j = 0; j < paramSet.count; ++j)
                        if (juce::String (paramSet.params[j].name).equalsIgnoreCase (wanted)
                            || (name.equalsIgnoreCase ("Time A") && juce::String (paramSet.params[j].name).equalsIgnoreCase ("Sync A"))
                            || (name.equalsIgnoreCase ("Time B") && juce::String (paramSet.params[j].name).equalsIgnoreCase ("Sync B")))
                            return paramSet.params[j].idx;
            }
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
bool isDelayTimeSyncEnabled (const gp200::GP200EffectSlot& effect, const gp200::GP200EffectParamSet& paramSet, int timeParamIndex)
{
    const auto syncParamIndex = findDelayTimeSyncParamIndex (paramSet, timeParamIndex);

    if (syncParamIndex < 0 || syncParamIndex >= static_cast<int> (effect.params.size ()))
        return false;

    return effect.params[static_cast<std::size_t> (syncParamIndex)] >= 0.5f;
}
void EffectBlockComponent::setParameterValueForDisplay (int paramIndex, float value)
{
    if (paramIndex < 0 || paramIndex >= static_cast<int> (effect.params.size ()))
        return;

    effect.params[static_cast<std::size_t> (paramIndex)] = value;

    const auto* paramSet = gp200::GP200EffectParamDatabase::findParamsForEffect (effect.effectId, getBlockName ());

    if (usesDelayTimeControls (effect, getBlockName ()) && paramSet != nullptr)
    {
        bool changedTimeSync = false;
        for (int i = 0; i < paramSet->count; ++i)
            if (isDelayTimeParameter (paramSet->params[i])
                && paramIndex == findDelayTimeSyncParamIndex (*paramSet, paramSet->params[i].idx)
                && delayTimeSyncDisplayed[static_cast<std::size_t> (paramSet->params[i].idx)]
                    != isDelayTimeSyncEnabled (effect, *paramSet, paramSet->params[i].idx))
                changedTimeSync = true;

        if (changedTimeSync)
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

            if (control.usesDiscreteOptions && control.valueLabel != nullptr)
            {
                control.valueLabel->setText (
                    gp200::GP200EffectParamDatabase::getDiscreteOptionLabel (
                        effect.effectId, paramIndex, value, getBlockName ()),
                    juce::dontSendNotification);
            }

            break;
        }
    }
}