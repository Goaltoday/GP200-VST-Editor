namespace gp200 {
bool MidiConnection::sendParamChange (int blockIndex, int paramIndex, juce::uint32 effectId, float value)
{
    const juce::ScopedLock lock (stateLock);
    if (sceneRefreshPending || juce::Time::getMillisecondCounterHiRes()<remoteSceneDeadline) { lastMessageText="Scene changed: waiting for fresh pedal state"; return false; }
    if (blockIndex < 0 || blockIndex >= static_cast<int> (effectBlockCount))
    {
        lastMessageText = "Cannot change parameter: invalid block index";
        return false;
    }

    if (paramIndex < 0 || paramIndex >= static_cast<int> (effectParamCount))
    {
        lastMessageText = "Cannot change parameter: invalid parameter index";
        return false;
    }

    if (GP200ModSync::isNamAmp (effectId)
        && (paramIndex > 1 || !std::isfinite (value) || value < 0.0f || value > 100.0f))
    {
        lastMessageText = "NAM2 accepts only Input / Output in the range 0-100";
        return false;
    }

    if (midiOutput == nullptr)
    {
        lastMessageText = "Cannot change parameter: MIDI output not open";
        return false;
    }

    const auto bytes = buildParamChange (blockIndex, paramIndex, effectId, value);

    auto message =
        juce::MidiMessage::createSysExMessage (bytes.data () + 1, static_cast<int> (bytes.size () - 2));

    midiOutput->sendMessageNow (message);

    updateCurrentPresetEffectParam (blockIndex, paramIndex, value);

    lastMessageText = "Sent parameter change to GP-200: block " + juce::String (blockIndex) + " param " +
                      juce::String (paramIndex) + " = " + juce::String (value, 2);

    return true;
}
void MidiConnection::updateCurrentPresetEffectId (int blockIndex, juce::uint32 effectId)
{
    const juce::ScopedLock lock (stateLock);
    if (blockIndex < 0 || blockIndex >= static_cast<int> (effectBlockCount))
        return;

    const auto base = effectBlockStart + static_cast<std::size_t> (blockIndex) * effectBlockSize;
    const auto effectIdByteOffset = base + effectIdOffset;
    const auto paramsByteOffset = base + paramsOffset;

    if (currentPresetDecodedData.getSize () < paramsByteOffset + effectParamCount * 4)
        return;

    auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());

    if (data == nullptr)
        return;

    data[effectIdByteOffset + 0] = static_cast<juce::uint8> (effectId & 0xFF);
    data[effectIdByteOffset + 1] = static_cast<juce::uint8> ((effectId >> 8) & 0xFF);
    data[effectIdByteOffset + 2] = static_cast<juce::uint8> ((effectId >> 16) & 0xFF);
    data[effectIdByteOffset + 3] = static_cast<juce::uint8> ((effectId >> 24) & 0xFF);

    EffectParameters defaultParams{};

    if (const auto* paramSet = GP200EffectParamDatabase::findParamsForEffect (
            effectId, GP200PresetCodec::blockNameForSlotIndex (blockIndex)))
    {
        for (int i = 0; i < paramSet->count; ++i)
        {
            const auto& param = paramSet->params[i];

            if (param.idx >= 0 && param.idx < static_cast<int> (defaultParams.size ()))
                defaultParams[static_cast<std::size_t> (param.idx)] = param.defaultValue;
        }
    }

    for (std::size_t i = 0; i < defaultParams.size (); ++i)
    {
        const auto offset = paramsByteOffset + i * 4;
        const auto value = defaultParams[i];
        std::memcpy (data + offset, &value, sizeof (float));
    }

    ++presetRevision;
}
void MidiConnection::updateCurrentPresetEffectParam (int blockIndex, int paramIndex, float value)
{
    const juce::ScopedLock lock (stateLock);
    if (blockIndex < 0 || blockIndex >= static_cast<int> (effectBlockCount))
        return;

    if (paramIndex < 0 || paramIndex >= static_cast<int> (effectParamCount))
        return;

    const auto base = effectBlockStart + static_cast<std::size_t> (blockIndex) * effectBlockSize;
    const auto paramsByteOffset = base + paramsOffset;
    const auto valueOffset = paramsByteOffset + static_cast<std::size_t> (paramIndex) * 4;

    if (currentPresetDecodedData.getSize () < valueOffset + sizeof (float))
        return;

    auto* data = static_cast<juce::uint8*> (currentPresetDecodedData.getData ());

    if (data == nullptr)
        return;

    std::memcpy (data + valueOffset, &value, sizeof (float));
    ++presetRevision;
}
std::vector<juce::uint8> MidiConnection::buildParamChange (int blockIndex, int paramIndex, juce::uint32 effectId, float value)
{
    juce::uint8 decoded[24]{};

    decoded[2] = 0x04;
    decoded[8] = 0x05;
    decoded[10] = 0x0C;
    decoded[12] = static_cast<juce::uint8> (blockIndex & 0x0F);
    decoded[13] = static_cast<juce::uint8> (paramIndex & 0x0F);

    const auto displayValue = encodeDisplayValue (value);
    decoded[14] = displayValue[0];
    decoded[15] = displayValue[1];

    decoded[16] = static_cast<juce::uint8> (effectId & 0xFF);
    decoded[17] = static_cast<juce::uint8> ((effectId >> 8) & 0xFF);
    decoded[18] = static_cast<juce::uint8> ((effectId >> 16) & 0xFF);
    decoded[19] = static_cast<juce::uint8> ((effectId >> 24) & 0xFF);

    std::memcpy (decoded + 20, &value, sizeof (float));

    const auto nibbles = nibbleEncode (decoded, 24);

    std::vector<juce::uint8> message;
    message.reserve (62);

    message.push_back (0xF0);
    message.push_back (0x21);
    message.push_back (0x25);
    message.push_back (0x7E);
    message.push_back (0x47);
    message.push_back (0x50);
    message.push_back (0x2D);
    message.push_back (0x32);
    message.push_back (0x12);
    message.push_back (0x18);
    message.push_back (0x00);
    message.push_back (0x00);
    message.push_back (0x00);

    message.insert (message.end (), nibbles.begin (), nibbles.end ());

    message.push_back (0xF7);

    return message;
}
std::array<juce::uint8, 2> MidiConnection::encodeDisplayValue (float value)
{
    int encodedValue = 0;

    if (std::isfinite (value) && value > 0.0f)
    {
        encodedValue = static_cast<int> (std::round (16367.0f + 16.0f * std::log2 (value)));
        encodedValue = juce::jlimit (0, 0xFFFF, encodedValue);
    }

    return {static_cast<juce::uint8> (encodedValue & 0xFF),
            static_cast<juce::uint8> ((encodedValue >> 8) & 0xFF)};
}
std::vector<juce::uint8> MidiConnection::nibbleEncode (const juce::uint8* data, int size)
{
    std::vector<juce::uint8> encoded;

    if (data == nullptr || size <= 0)
        return encoded;

    encoded.reserve (static_cast<std::size_t> (size * 2));

    for (int i = 0; i < size; ++i)
    {
        encoded.push_back (static_cast<juce::uint8> ((data[i] >> 4) & 0x0F));
        encoded.push_back (static_cast<juce::uint8> (data[i] & 0x0F));
    }

    return encoded;
}
}