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