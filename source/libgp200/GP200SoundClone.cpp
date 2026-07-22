#include "GP200SoundClone.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace gp200
{
namespace
{
constexpr int modelBytes = 8192;
constexpr int wrapperBytes = 28;
constexpr int blobBytes = wrapperBytes + modelBytes;
constexpr int chunkBytes = 183;

juce::MidiMessage makeSysEx (const std::vector<juce::uint8>& full)
{
    return juce::MidiMessage::createSysExMessage (full.data () + 1,
                                                   static_cast<int> (full.size () - 2));
}

std::vector<juce::uint8> nibbleEncode (const juce::uint8* data, int size)
{
    std::vector<juce::uint8> encoded;
    encoded.reserve (static_cast<std::size_t> (size) * 2u);

    for (int i = 0; i < size; ++i)
    {
        encoded.push_back (static_cast<juce::uint8> ((data[i] >> 4) & 0x0f));
        encoded.push_back (static_cast<juce::uint8> (data[i] & 0x0f));
    }

    return encoded;
}

std::vector<juce::uint8> makePrepare (int globalSlot)
{
    const auto category = static_cast<juce::uint8> (globalSlot < 5 ? 0x03 : 0x02);

    return { 0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,0x14,
             0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
             0x00,0x00,0x00,0x0e,0x0f,0x00,0x00,0x01,0x06,0x00,0x00,0x00,
             0x08,0x00,0x00,0x00,category,0x0f,0x05,0x09,0x03,0x00,0x02,0x00,
             static_cast<juce::uint8> (globalSlot),0x00,0x00,0x00,0x00,0x00,0x0f,0xf7 };
}

std::vector<juce::uint8> makeCommit (int globalSlot)
{
    return { 0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,0x0c,
             0x00,0x00,0x00,0x01,0x04,0x01,0x00,0x00,0x08,0x00,0x00,0x00,
             static_cast<juce::uint8> (globalSlot),0x00,0x00,0x00,0x01,0x00,0x00,
             0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf7 };
}
} // namespace

juce::Result GP200SoundClone::buildUpload (const juce::File& cloFile,
                                            int globalSlot,
                                            GP200SoundCloneUpload& result)
{
    if (!cloFile.existsAsFile ())
        return juce::Result::fail ("The selected CLO file does not exist.");

    const bool isValetonClo = cloFile.hasFileExtension ("clo");
    const bool isHotoneTone = cloFile.hasFileExtension ("tone");

    if (!isValetonClo && !isHotoneTone)
        return juce::Result::fail ("Select a Valeton .clo or Hotone .tone file.");

    if (!juce::isPositiveAndBelow (globalSlot, 10))
        return juce::Result::fail ("The Sound Clone destination must be AMP 1-5 or DIST 1-5.");

    juce::FileInputStream input (cloFile);
    if (!input.openedOk ())
        return juce::Result::fail ("The CLO file cannot be opened.");

    if (input.getTotalLength () < modelBytes)
        return juce::Result::fail ("The CLO file is too small. At least 8192 bytes are required.");

    std::array<juce::uint8, modelBytes> model{};
    const auto bytesRead = input.read (model.data (), modelBytes);
    if (bytesRead != modelBytes)
        return juce::Result::fail ("The CLO model data could not be read completely.");

    const bool hasValetonSignature =
        model[0] == 'V' && model[1] == 'T' && model[2] == 'S' && model[3] == 'I';

    const bool hasHotoneSignature =
        model[0] == 'H' && model[1] == 'T' && model[2] == 'S' && model[3] == 'I';

    if (isValetonClo)
    {
        if (!hasValetonSignature)
            return juce::Result::fail (
                "This is not a GP-200/Valeton CLO file (expected VTSI header).");
    }
    else
    {
        if (!hasHotoneSignature)
            return juce::Result::fail (
                "This is not a supported Hotone TONE file (expected HTSI header).");

        // Experimental Hotone -> Valeton conversion derived from tested files:
        // HTSI -> VTSI, declared container size 0x2288 -> 0x1288,
        // and declared payload size 0x2200 -> 0x1200.
        // The remaining model bytes are preserved exactly as supplied.
        model[0] = 'V';
        model[1] = 'T';
        model[2] = 'S';
        model[3] = 'I';

        model[4] = 0x88;
        model[5] = 0x12;
        model[6] = 0x00;
        model[7] = 0x00;

        model[20] = 0x00;
        model[21] = 0x12;
        model[22] = 0x00;
        model[23] = 0x00;
    }

    std::array<juce::uint8, blobBytes> blob{};
    blob[0] = 0x13;
    blob[1] = 0x10;
    blob[2] = 0x18;
    blob[3] = 0x20;
    blob[4] = static_cast<juce::uint8> (globalSlot);
    blob[10] = 0x08;
    blob[11] = 0x00;

    const auto displayName = cloFile.getFileNameWithoutExtension ().substring (0, 16);
    const auto* nameBytes = displayName.toRawUTF8 ();
    for (int i = 0; i < 16 && nameBytes[i] != 0; ++i)
        blob[12 + i] = static_cast<juce::uint8> (nameBytes[i]);

    std::uint16_t checksum = 0;
    for (const auto byte : model)
        checksum = static_cast<std::uint16_t> (checksum + byte);

    blob[8] = static_cast<juce::uint8> (checksum & 0xff);
    blob[9] = static_cast<juce::uint8> ((checksum >> 8) & 0xff);

    std::copy (model.begin (), model.end (), blob.begin () + wrapperBytes);

    result = {};
    result.displayName = displayName;
    result.globalSlot = globalSlot;
    result.prepareMessage = makeSysEx (makePrepare (globalSlot));
    result.commitMessage = makeSysEx (makeCommit (globalSlot));

    for (int offset = 0; offset < blobBytes; offset += chunkBytes)
    {
        const auto amount = juce::jmin (chunkBytes, blobBytes - offset);

        std::vector<juce::uint8> full {
            0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,
            0x12,0x1c,0x40,
            static_cast<juce::uint8> (offset & 0x7f),
            static_cast<juce::uint8> ((offset >> 7) & 0x7f)
        };

        auto encoded = nibbleEncode (blob.data () + offset, amount);
        full.insert (full.end (), encoded.begin (), encoded.end ());
        full.push_back (0xf7);

        result.chunks.push_back (makeSysEx (full));
    }

    return juce::Result::ok ();
}
} // namespace gp200
