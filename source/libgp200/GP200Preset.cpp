/*
    GP200 VST

    Portions adapted from phash/gp200editor and its contributors.
    Those portions are licensed under GPL-3.0-or-later.

  
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "GP200Preset.h"
#include "GP200EffectDatabase.h"
#include "GP200EffectParamDatabase.h"

#include <cmath>
#include <cstring>

namespace gp200
{
namespace
{
juce::String formatParamValue (float value)
{
    if (!std::isfinite (value))
        return "0";

    const auto rounded = std::round (value);

    if (std::abs (value - rounded) < 0.05f)
        return juce::String (static_cast<int> (rounded));

    return juce::String (value, 1);
}

juce::String makeParamText (const juce::String& name, float value)
{
    return name + "=" + formatParamValue (value);
}

constexpr std::size_t prstUserFileSize = 1224;
constexpr std::size_t prstFactoryFileSize = 1176;

constexpr std::size_t prstMagicOffset = 0x00;
constexpr std::size_t prstDeviceIdOffset = 0x10;

constexpr std::size_t prstPatchNameOffset = 0x44;
constexpr std::size_t prstAuthorOffset = 0x54;
constexpr std::size_t prstTextLength = 16;

constexpr std::size_t prstFxLoopSendOffset = 0x92;
constexpr std::size_t prstFxLoopReturnOffset = 0x93;
constexpr std::size_t prstRoutingOrderOffset = 0x94;

constexpr std::size_t prstEffectBlockStart = 0xA0;
constexpr std::size_t prstEffectBlockSize = 0x48;

constexpr std::size_t prstChecksumOffset = 0x4C6;

bool matchesAscii (
    const juce::uint8* data,
    std::size_t dataSize,
    std::size_t offset,
    const char* expected,
    std::size_t expectedLength)
{
    if (data == nullptr ||
        expected == nullptr ||
        offset + expectedLength > dataSize)
    {
        return false;
    }

    for (std::size_t i = 0; i < expectedLength; ++i)
    {
        if (data[offset + i] !=
            static_cast<juce::uint8> (expected[i]))
        {
            return false;
        }
    }

    return true;
}
} // namespace

juce::String GP200EffectSlot::getSummaryLine () const
{
    const auto blockName = GP200PresetCodec::blockNameForSlotIndex (slotIndex >= 0 ? slotIndex : blockIndex);
    const auto state = enabled ? "ON " : "OFF";
    const auto effectName = GP200PresetCodec::effectNameForId (effectId);

    return blockName.paddedRight (' ', 4) + " " + juce::String (state).paddedRight (' ', 3) + "  " +
           effectName.paddedRight (' ', 22) + "  ID " + GP200PresetCodec::effectIdToHex (effectId);
}

juce::StringArray GP200EffectSlot::getDetailedLines () const
{
    juce::StringArray lines;

    lines.add (getSummaryLine ());

    const auto* paramSet = GP200EffectParamDatabase::findParamsForEffect (effectId);

    if (paramSet != nullptr && paramSet->count > 0)
    {
        juce::String currentLine = "    ";
        int paramsInCurrentLine = 0;

        for (int i = 0; i < paramSet->count; ++i)
        {
            const auto& param = paramSet->params[i];

            if (param.idx < 0 || param.idx >= static_cast<int> (params.size ()))
                continue;

            const auto token = makeParamText (param.name, params[static_cast<std::size_t> (param.idx)]);

            if (paramsInCurrentLine >= 4)
            {
                lines.add (currentLine);
                currentLine = "    ";
                paramsInCurrentLine = 0;
            }

            if (paramsInCurrentLine > 0)
                currentLine << "    ";

            currentLine << token;
            ++paramsInCurrentLine;
        }

        if (currentLine.trim ().isNotEmpty ())
            lines.add (currentLine);
    }
    else
    {
        juce::String currentLine = "    ";

        for (std::size_t i = 0; i < params.size (); ++i)
        {
            if (i > 0 && i % 5 == 0)
            {
                lines.add (currentLine);
                currentLine = "    ";
            }

            if (i % 5 != 0)
                currentLine << "    ";

            currentLine << "P" << juce::String (static_cast<int> (i)) << "=" << formatParamValue (params[i]);
        }

        if (currentLine.trim ().isNotEmpty ())
            lines.add (currentLine);
    }

    return lines;
}

const GP200EffectSlot* GP200Preset::findEffectBySlotIndex (int wantedSlotIndex) const
{
    for (const auto& effect : effects)
    {
        if (effect.slotIndex == wantedSlotIndex)
            return &effect;
    }

    if (wantedSlotIndex >= 0 && wantedSlotIndex < static_cast<int> (effects.size ()))
        return &effects[static_cast<std::size_t> (wantedSlotIndex)];

    return nullptr;
}

juce::String GP200Preset::getSignalChainText () const
{
    if (!isValid)
        return "Signal chain: not available";

    juce::String chain;
    chain << "Signal chain: ";

    for (std::size_t i = 0; i < routingOrder.size (); ++i)
    {
        const auto slotIndex = routingOrder[i];

        if (i > 0)
            chain << " -> ";

        chain << GP200PresetCodec::blockNameForSlotIndex (slotIndex);
    }

    return chain;
}

juce::StringArray GP200Preset::getEffectSummaryLines () const
{
    juce::StringArray lines;

    if (!isValid)
    {
        lines.add ("Decoded preset blocks: not available");
        return lines;
    }

    lines.add (getSignalChainText ());
    lines.add ("FX Loop Send: " + juce::String (fxLoopSend) +
               "    FX Loop Return: " + juce::String (fxLoopReturn));
    lines.add ({});

    for (const auto slotIndex : routingOrder)
    {
        const auto* effect = findEffectBySlotIndex (slotIndex);

        if (effect == nullptr)
            continue;

        const auto effectLines = effect->getDetailedLines ();

        for (const auto& line : effectLines)
            lines.add (line);

        lines.add ({});
    }

    return lines;
}

GP200Preset GP200PresetCodec::decodeLivePresetDump (const juce::MemoryBlock& presetData)
{
    GP200Preset preset;

    const auto dataSize = presetData.getSize ();

    if (dataSize < effectBlockStart + effectBlockSize * effectBlockCount)
        return preset;

    const auto* data = static_cast<const juce::uint8*> (presetData.getData ());

    preset.isValid = true;

    preset.patchName = readAsciiString (data, dataSize, presetNameOffset, presetNameMaxLength);

    if (preset.patchName.isEmpty ())
        preset.patchName = "unknown";

    preset.author = readAsciiString (data, dataSize, authorOffset, presetNameMaxLength);

    if (dataSize > fxLoopSendOffset)
        preset.fxLoopSend = data[fxLoopSendOffset];

    if (dataSize > fxLoopReturnOffset)
        preset.fxLoopReturn = data[fxLoopReturnOffset];

    for (std::size_t i = 0; i < effectBlockCount; ++i)
    {
        const auto offset = routingOrderOffset + i;

        if (offset < dataSize)
            preset.routingOrder[i] = data[offset];
        else
            preset.routingOrder[i] = static_cast<int> (i);
    }

    for (std::size_t block = 0; block < effectBlockCount; ++block)
    {
        const auto base = effectBlockStart + block * effectBlockSize;

        auto& effect = preset.effects[block];

        effect.blockIndex = static_cast<int> (block);
        effect.slotIndex = data[base + slotIndexOffset];
        effect.enabled = data[base + enabledOffset] == 1;
        effect.effectId = readUInt32LE (data, dataSize, base + effectIdOffset);

        for (std::size_t param = 0; param < effectParamCount; ++param)
            effect.params[param] = readFloat32LE (data, dataSize, base + paramsOffset + param * 4);
    }

    return preset;
}

GP200Preset GP200PresetCodec::decodePrstFile (
    const juce::MemoryBlock& fileData)
{
    GP200Preset preset;

    const auto dataSize = fileData.getSize ();

    if (dataSize != prstUserFileSize &&
        dataSize != prstFactoryFileSize)
    {
        return preset;
    }

    const auto* data =
        static_cast<const juce::uint8*> (
            fileData.getData ());

    if (data == nullptr)
        return preset;

    if (!matchesAscii (
            data,
            dataSize,
            prstMagicOffset,
            "TSRP",
            4))
    {
        return preset;
    }

    if (!matchesAscii (
            data,
            dataSize,
            prstDeviceIdOffset,
            "2-PG",
            4))
    {
        return preset;
    }

    const auto requiredSize =
        prstEffectBlockStart +
        prstEffectBlockSize * effectBlockCount;

    if (dataSize < requiredSize)
        return preset;

    // Los archivos de usuario de 1224 bytes tienen checksum.
    if (dataSize == prstUserFileSize)
    {
        juce::uint32 calculatedChecksum = 0;

        for (std::size_t i = 0;
             i < prstChecksumOffset;
             ++i)
        {
            calculatedChecksum += data[i];
        }

        calculatedChecksum &= 0xFFFFu;

        const auto storedChecksum =
            (static_cast<juce::uint32> (
                 data[prstChecksumOffset]) << 8) |
            static_cast<juce::uint32> (
                data[prstChecksumOffset + 1]);

        if (calculatedChecksum != storedChecksum)
            return preset;
    }

    preset.prstRawSource = fileData;

    preset.patchName =
        readAsciiString (
            data,
            dataSize,
            prstPatchNameOffset,
            prstTextLength);

    if (preset.patchName.isEmpty ())
        preset.patchName = "unknown";

    preset.author =
        readAsciiString (
            data,
            dataSize,
            prstAuthorOffset,
            prstTextLength);

    const auto rawFxSend =
        static_cast<int> (
            data[prstFxLoopSendOffset]);

    const auto rawFxReturn =
        static_cast<int> (
            data[prstFxLoopReturnOffset]);

    preset.fxLoopSend =
        rawFxSend >= 1 && rawFxSend <= 10
            ? rawFxSend
            : 4;

    preset.fxLoopReturn =
        rawFxReturn >= 1 && rawFxReturn <= 10
            ? rawFxReturn
            : 4;

    // Validación estricta del routing.
    std::array<bool, effectBlockCount> routingSeen{};

    for (std::size_t i = 0;
         i < effectBlockCount;
         ++i)
    {
        const auto blockIndex =
            static_cast<int> (
                data[prstRoutingOrderOffset + i]);

        if (blockIndex < 0 ||
            blockIndex >=
                static_cast<int> (effectBlockCount))
        {
            return GP200Preset{};
        }

        if (routingSeen[
                static_cast<std::size_t> (blockIndex)])
        {
            return GP200Preset{};
        }

        routingSeen[
            static_cast<std::size_t> (blockIndex)] = true;

        preset.routingOrder[i] = blockIndex;
    }

    // Los bloques se conservan en orden físico/protocolo.
    for (std::size_t block = 0;
         block < effectBlockCount;
         ++block)
    {
        const auto base =
            prstEffectBlockStart +
            block * prstEffectBlockSize;

        const auto slotIndex =
            static_cast<int> (
                data[base + slotIndexOffset]);

        // Evita importar un archivo corrupto en un bloque incorrecto.
        if (slotIndex != static_cast<int> (block))
            return GP200Preset{};

        const auto enabledByte =
            data[base + enabledOffset];

        if (enabledByte > 1)
            return GP200Preset{};

        auto& effect = preset.effects[block];

        effect.blockIndex =
            static_cast<int> (block);

        effect.slotIndex = slotIndex;
        effect.enabled = enabledByte == 1;

        effect.effectId =
            readUInt32LE (
                data,
                dataSize,
                base + effectIdOffset);

        for (std::size_t param = 0;
             param < effectParamCount;
             ++param)
        {
            effect.params[param] =
                readFloat32LE (
                    data,
                    dataSize,
                    base +
                        paramsOffset +
                        param * sizeof (float));
        }
    }

    preset.isValid = true;
    return preset;
}

juce::MemoryBlock GP200PresetCodec::encodePrstFile (
    const GP200Preset& preset)
{
    juce::MemoryBlock result;
    result.setSize (prstUserFileSize, true);

    auto* data = static_cast<juce::uint8*> (result.getData ());
    if (data == nullptr)
        return {};

    // Preserve every unmodelled byte when this preset came from a real
    // 1224-byte user PRST. Otherwise seed the confirmed GP-200 structure.
    if (preset.prstRawSource.getSize () >= prstUserFileSize)
    {
        std::memcpy (data,
                     preset.prstRawSource.getData (),
                     prstUserFileSize);
    }
    else
    {
        const auto writeAscii = [data] (std::size_t offset,
                                        const char* text,
                                        std::size_t length)
        {
            std::memcpy (data + offset, text, length);
        };

        writeAscii (0x00, "TSRP", 4);
        data[0x0B] = 0x06;
        writeAscii (0x10, "2-PG", 4);
        data[0x14] = 0x00;
        data[0x15] = 0x01;
        data[0x16] = 0x01;
        data[0x17] = 0x00;

        const auto timestamp = static_cast<juce::uint32> (
            juce::Time::getCurrentTime ().toMilliseconds () / 1000);
        writeUInt32LE (data, prstUserFileSize, 0x1C, timestamp);

        data[0x24] = 0x94;
        data[0x25] = 0x04;
        writeAscii (0x28, "MRAP", 4);
        writeUInt32LE (data, prstUserFileSize, 0x2C, 1172u);

        data[0x30] = 0x02;
        data[0x32] = 0x58;
        data[0x36] = 0x78;
        data[0x38] = 0x32;

        data[0x8C] = 0x08;
        data[0x8D] = 0x00;
        data[0x8E] = 0x10;
        data[0x8F] = 0x00;

        for (std::size_t block = 0; block < effectBlockCount; ++block)
        {
            const auto base = prstEffectBlockStart + block * prstEffectBlockSize;
            data[base + 0] = 0x14;
            data[base + 2] = 0x44;
            data[base + 4] = static_cast<juce::uint8> (block);
            data[base + 7] = 0x0F;
        }
    }

    writeAsciiString (data,
                      prstUserFileSize,
                      prstPatchNameOffset,
                      prstTextLength,
                      preset.patchName);
    writeAsciiString (data,
                      prstUserFileSize,
                      prstAuthorOffset,
                      prstTextLength,
                      preset.author);

    data[prstFxLoopSendOffset] = static_cast<juce::uint8> (
        juce::jlimit (1, 10, preset.fxLoopSend));
    data[prstFxLoopReturnOffset] = static_cast<juce::uint8> (
        juce::jlimit (1, 10, preset.fxLoopReturn));

    for (std::size_t i = 0; i < effectBlockCount; ++i)
    {
        data[prstRoutingOrderOffset + i] = static_cast<juce::uint8> (
            juce::jlimit (0,
                          static_cast<int> (effectBlockCount) - 1,
                          preset.routingOrder[i]));
    }

    for (const auto& effect : preset.effects)
    {
        const auto physicalSlot = juce::jlimit (
            0,
            static_cast<int> (effectBlockCount) - 1,
            effect.slotIndex >= 0 ? effect.slotIndex : effect.blockIndex);
        const auto base = prstEffectBlockStart
                        + static_cast<std::size_t> (physicalSlot) * prstEffectBlockSize;

        data[base + slotIndexOffset] = static_cast<juce::uint8> (physicalSlot);
        data[base + enabledOffset] = effect.enabled ? 1 : 0;
        writeUInt32LE (data,
                       prstUserFileSize,
                       base + effectIdOffset,
                       effect.effectId);

        for (std::size_t param = 0; param < effectParamCount; ++param)
        {
            const auto value = std::isfinite (effect.params[param])
                                   ? effect.params[param]
                                   : 0.0f;
            writeFloat32LE (data,
                            prstUserFileSize,
                            base + paramsOffset + param * sizeof (float),
                            value);
        }
    }

    juce::uint32 checksum = 0;
    for (std::size_t i = 0; i < prstChecksumOffset; ++i)
        checksum += data[i];

    checksum &= 0xFFFFu;
    data[prstChecksumOffset] = static_cast<juce::uint8> ((checksum >> 8) & 0xFFu);
    data[prstChecksumOffset + 1] = static_cast<juce::uint8> (checksum & 0xFFu);

    return result;
}

juce::MemoryBlock
GP200PresetCodec::makeLivePresetDumpFromPrst (
    const GP200Preset& preset,
    const juce::MemoryBlock& livePresetTemplate)
{
    if (!preset.isValid)
        return {};

    const auto requiredSize =
        effectBlockStart +
        effectBlockSize * effectBlockCount;

    if (livePresetTemplate.getSize () < requiredSize)
        return {};

    // Copia completa del preset actual.
    // Todo lo que no conocemos queda intacto.
    juce::MemoryBlock result (livePresetTemplate);

    auto* data =
        static_cast<juce::uint8*> (
            result.getData ());

    if (data == nullptr)
        return {};

    const auto dataSize = result.getSize ();

    writeAsciiString (
        data,
        dataSize,
        presetNameOffset,
        presetNameMaxLength,
        preset.patchName);

    writeAsciiString (
        data,
        dataSize,
        authorOffset,
        presetNameMaxLength,
        preset.author);

    data[fxLoopSendOffset] =
        static_cast<juce::uint8> (
            juce::jlimit (
                1,
                10,
                preset.fxLoopSend));

    data[fxLoopReturnOffset] =
        static_cast<juce::uint8> (
            juce::jlimit (
                1,
                10,
                preset.fxLoopReturn));

    std::array<bool, effectBlockCount> routingSeen{};

    for (std::size_t i = 0;
         i < effectBlockCount;
         ++i)
    {
        const auto blockIndex =
            preset.routingOrder[i];

        if (blockIndex < 0 ||
            blockIndex >=
                static_cast<int> (effectBlockCount))
        {
            return {};
        }

        if (routingSeen[
                static_cast<std::size_t> (blockIndex)])
        {
            return {};
        }

        routingSeen[
            static_cast<std::size_t> (blockIndex)] = true;

        data[routingOrderOffset + i] =
            static_cast<juce::uint8> (blockIndex);
    }

    std::array<bool, effectBlockCount> blockSeen{};

    for (const auto& effect : preset.effects)
    {
        if (effect.blockIndex < 0 ||
            effect.blockIndex >=
                static_cast<int> (effectBlockCount))
        {
            return {};
        }

        const auto block =
            static_cast<std::size_t> (
                effect.blockIndex);

        if (blockSeen[block])
            return {};

        blockSeen[block] = true;

        if (effect.slotIndex != effect.blockIndex)
            return {};

        const auto base =
            effectBlockStart +
            block * effectBlockSize;

        data[base + slotIndexOffset] =
            static_cast<juce::uint8> (
                effect.slotIndex);

        data[base + enabledOffset] =
            effect.enabled ? 1 : 0;

        writeUInt32LE (
            data,
            dataSize,
            base + effectIdOffset,
            effect.effectId);

        for (std::size_t param = 0;
             param < effect.params.size ();
             ++param)
        {
            writeFloat32LE (
                data,
                dataSize,
                base +
                    paramsOffset +
                    param * sizeof (float),
                effect.params[param]);
        }
    }

    return result;
}
juce::String GP200PresetCodec::blockNameForSlotIndex (int slotIndex)
{
    switch (slotIndex)
    {
    case 0:
        return "PRE";
    case 1:
        return "WAH";
    case 2:
        return "DST";
    case 3:
        return "AMP";
    case 4:
        return "NR";
    case 5:
        return "CAB";
    case 6:
        return "EQ";
    case 7:
        return "MOD";
    case 8:
        return "DLY";
    case 9:
        return "RVB";
    case 10:
        return "VOL";
    default:
        return "???";
    }
}

juce::String GP200PresetCodec::effectNameForId (juce::uint32 effectId)
{
    return GP200EffectDatabase::getEffectName (effectId);
}

juce::String GP200PresetCodec::effectIdToHex (juce::uint32 effectId)
{
    return GP200EffectDatabase::effectIdToHex (effectId);
}

juce::String GP200PresetCodec::readAsciiString (const juce::uint8* data,
                                                std::size_t dataSize,
                                                std::size_t offset,
                                                std::size_t maxLength)
{
    if (data == nullptr || offset >= dataSize)
        return {};

    juce::String text;

    for (std::size_t i = 0; i < maxLength && offset + i < dataSize; ++i)
    {
        const auto value = data[offset + i];

        if (value == 0)
            break;

        text += juce::String::charToString (static_cast<juce_wchar> (value));
    }

    return text.trim ();
}

juce::uint32
GP200PresetCodec::readUInt32LE (const juce::uint8* data, std::size_t dataSize, std::size_t offset)
{
    if (data == nullptr || offset + 4 > dataSize)
        return 0;

    return static_cast<juce::uint32> (data[offset]) | (static_cast<juce::uint32> (data[offset + 1]) << 8) |
           (static_cast<juce::uint32> (data[offset + 2]) << 16) |
           (static_cast<juce::uint32> (data[offset + 3]) << 24);
}

float GP200PresetCodec::readFloat32LE (const juce::uint8* data, std::size_t dataSize, std::size_t offset)
{
    const auto raw = readUInt32LE (data, dataSize, offset);

    float value = 0.0f;
    std::memcpy (&value, &raw, sizeof (float));

    if (!std::isfinite (value))
        return 0.0f;

    return value;
}

void GP200PresetCodec::writeAsciiString (
    juce::uint8* data,
    std::size_t dataSize,
    std::size_t offset,
    std::size_t maxLength,
    const juce::String& text)
{
    if (data == nullptr ||
        offset + maxLength > dataSize)
    {
        return;
    }

    for (std::size_t i = 0; i < maxLength; ++i)
        data[offset + i] = 0;

    const auto count =
        juce::jmin (
            static_cast<int> (maxLength),
            text.length ());

    for (int i = 0; i < count; ++i)
    {
        const auto character = text[i];

        data[offset + static_cast<std::size_t> (i)] =
            character >= 32 && character <= 126
                ? static_cast<juce::uint8> (character)
                : static_cast<juce::uint8> (' ');
    }
}

void GP200PresetCodec::writeUInt32LE (
    juce::uint8* data,
    std::size_t dataSize,
    std::size_t offset,
    juce::uint32 value)
{
    if (data == nullptr ||
        offset + sizeof (juce::uint32) > dataSize)
    {
        return;
    }

    data[offset] =
        static_cast<juce::uint8> (
            value & 0xFFu);

    data[offset + 1] =
        static_cast<juce::uint8> (
            (value >> 8) & 0xFFu);

    data[offset + 2] =
        static_cast<juce::uint8> (
            (value >> 16) & 0xFFu);

    data[offset + 3] =
        static_cast<juce::uint8> (
            (value >> 24) & 0xFFu);
}

void GP200PresetCodec::writeFloat32LE (
    juce::uint8* data,
    std::size_t dataSize,
    std::size_t offset,
    float value)
{
    if (!std::isfinite (value))
        value = 0.0f;

    juce::uint32 raw = 0;

    std::memcpy (
        &raw,
        &value,
        sizeof (float));

    writeUInt32LE (
        data,
        dataSize,
        offset,
        raw);
}

} // namespace gp200