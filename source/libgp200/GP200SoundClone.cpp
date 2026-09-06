#include "GP200SoundClone.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace gp200
{
namespace
{
constexpr int modelBytes = 8192;
constexpr int wrapperBytes = 28;
constexpr int blobBytes = wrapperBytes + modelBytes;
constexpr int chunkBytes = 183;

// Sound Clone container layout confirmed for the GP-200/Valeton files used here.
constexpr std::size_t physicalContainerBytes = 0x2288;
constexpr std::size_t gp200DeclaredBytes = 0x1288;
constexpr std::uint32_t largeDeclaredBytes = 0x2288;
constexpr std::uint32_t gp200PayloadBytes = 0x1200;
constexpr std::uint32_t largePayloadBytes = 0x2200;
constexpr std::uint32_t blockACount = 0x80;
constexpr std::uint32_t gp200BlockBCount = 0x400;
constexpr std::uint32_t largeBlockBCount = 0x800;

constexpr std::size_t declaredOffset = 0x04;
constexpr std::size_t crcOffset = 0x08;
constexpr std::size_t blockACountOffset = 0x80;
constexpr std::size_t blockBCountOffset = 0x84;
constexpr std::size_t payloadOffset = 0x14;
constexpr std::size_t crcDataOffset = 0x0c;

std::uint32_t readLe32 (const juce::uint8* data)
{
    return static_cast<std::uint32_t> (data[0])
         | (static_cast<std::uint32_t> (data[1]) << 8)
         | (static_cast<std::uint32_t> (data[2]) << 16)
         | (static_cast<std::uint32_t> (data[3]) << 24);
}

void writeLe32 (juce::uint8* data, std::uint32_t value)
{
    data[0] = static_cast<juce::uint8> (value & 0xff);
    data[1] = static_cast<juce::uint8> ((value >> 8) & 0xff);
    data[2] = static_cast<juce::uint8> ((value >> 16) & 0xff);
    data[3] = static_cast<juce::uint8> ((value >> 24) & 0xff);
}

std::uint16_t crc16Modbus (const juce::uint8* data, std::size_t size)
{
    std::uint16_t crc = 0xffff;

    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= static_cast<std::uint16_t> (data[i]);

        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) != 0u
                ? static_cast<std::uint16_t> ((crc >> 1) ^ 0xa001u)
                : static_cast<std::uint16_t> (crc >> 1);
    }

    return crc;
}

void updateInternalCrc (std::array<juce::uint8, physicalContainerBytes>& container)
{
    const auto declared = readLe32 (container.data () + declaredOffset);
    const auto crc = crc16Modbus (container.data () + crcDataOffset,
                                  static_cast<std::size_t> (declared) - crcDataOffset);

    // Stored little-endian in the VTSI header.
    container[crcOffset] = static_cast<juce::uint8> (crc & 0xff);
    container[crcOffset + 1] = static_cast<juce::uint8> ((crc >> 8) & 0xff);
}

juce::Result prepareSoundCloneModelForGP200 (
    const juce::File& sourceFile,
    std::array<juce::uint8, physicalContainerBytes>& prepared)
{
    juce::FileInputStream input (sourceFile);
    if (!input.openedOk ())
        return juce::Result::fail ("The Sound Clone file cannot be opened.");

    if (input.getTotalLength () < static_cast<juce::int64> (physicalContainerBytes))
        return juce::Result::fail (
            "The Sound Clone file is too small. At least 0x2288 (8840) bytes are required.");

    const auto bytesRead = input.read (prepared.data (),
                                       static_cast<int> (physicalContainerBytes));
    if (bytesRead != static_cast<int> (physicalContainerBytes))
        return juce::Result::fail ("The Sound Clone model data could not be read completely.");

    const bool isVtsi = prepared[0] == 'V' && prepared[1] == 'T'
                     && prepared[2] == 'S' && prepared[3] == 'I';
    const bool isHtsi = prepared[0] == 'H' && prepared[1] == 'T'
                     && prepared[2] == 'S' && prepared[3] == 'I';

    if (!isVtsi && !isHtsi)
        return juce::Result::fail (
            "Unsupported Sound Clone format (expected VTSI or HTSI header).");

    const auto declared = readLe32 (prepared.data () + declaredOffset);
    const auto blockA = readLe32 (prepared.data () + blockACountOffset);
    const auto blockB = readLe32 (prepared.data () + blockBCountOffset);
    const auto payload = readLe32 (prepared.data () + payloadOffset);

    const bool isGp2001024 = declared == gp200DeclaredBytes
                          && payload == gp200PayloadBytes
                          && blockA == blockACount
                          && blockB == gp200BlockBCount;

    const bool isLarge2048 = declared == largeDeclaredBytes
                          && payload == largePayloadBytes
                          && blockA == blockACount
                          && blockB == largeBlockBCount;

    if (isVtsi && isGp2001024)
    {
        // Already in the native GP-200 representation. Do not alter it.
        return juce::Result::ok ();
    }

    if (!isLarge2048)
    {
        return juce::Result::fail (
            "Unsupported Sound Clone structure. Expected 128+1024 GP-200 VTSI "
            "or 128+2048 VTSI/HTSI model data.");
    }

    // The confirmed Valeton 2048 -> 1024 conversion keeps the complete
    // 128-float Block A and the first 1024 floats of Block B. Since both
    // blocks start inside the common prefix, truncating the declared model at
    // 0x1288 preserves exactly those bytes. The physical file remains 0x2288.
    prepared[0] = 'V';
    prepared[1] = 'T';
    prepared[2] = 'S';
    prepared[3] = 'I';

    writeLe32 (prepared.data () + declaredOffset,
               static_cast<std::uint32_t> (gp200DeclaredBytes));
    writeLe32 (prepared.data () + payloadOffset, gp200PayloadBytes);
    writeLe32 (prepared.data () + blockBCountOffset, gp200BlockBCount);

    std::fill (prepared.begin () + static_cast<std::ptrdiff_t> (gp200DeclaredBytes),
               prepared.end (),
               static_cast<juce::uint8> (0));

    updateInternalCrc (prepared);
    return juce::Result::ok ();
}

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
             0x00,0x00,0x00,0x0d,0x0e,0x00,0x00,0x01,0x06,0x00,0x00,0x00,
             0x08,0x00,0x00,0x00,category,0x0f,0x07,0x0e,0x0f,0x00,0x02,0x00,
             static_cast<juce::uint8> (globalSlot),0x00,0x00,0x00,0x00,0x00,0x0f,0xf7 };
}

std::vector<juce::uint8> makeUserIRStagePrepare ()
{
    return { 0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,0x14,
             0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
             0x00,0x00,0x00,0x08,0x0b,0x00,0x00,0x01,0x06,0x00,0x00,0x00,0x08,
             0x00,0x00,0x00,0x05,0x0f,0x04,0x08,0x0f,0x00,0x00,0x00,
             0x00,0x00,0x00,0x01,0x00,0x00,0x0a,0xf7 };
}

std::vector<juce::uint8> makeUserIRStageCommit ()
{
    return { 0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,0x0c,
             0x00,0x00,0x00,0x00,0x0b,0x01,0x00,0x00,0x08,0x00,0x00,0x00,
             0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
             0x00,0x00,0x00,0x00,0xf7 };
}
} // namespace

juce::Result GP200SoundClone::buildUpload (const juce::File& cloFile,
                                            int globalSlot,
                                            GP200SoundCloneUpload& result)
{
    if (!cloFile.existsAsFile ())
        return juce::Result::fail ("The selected Sound Clone file does not exist.");

    if (!juce::isPositiveAndBelow (globalSlot, 10))
        return juce::Result::fail ("The Sound Clone destination must be AMP 1-5 or DIST 1-5.");

    std::array<juce::uint8, physicalContainerBytes> preparedContainer{};
    const auto preparationResult = prepareSoundCloneModelForGP200 (cloFile, preparedContainer);
    if (preparationResult.failed ())
        return preparationResult;

    // Keep the existing GP-200 transfer contract unchanged: the upload uses
    // exactly the first 8192 bytes of the prepared 0x2288-byte container.
    std::array<juce::uint8, modelBytes> model{};
    std::copy_n (preparedContainer.begin (), modelBytes, model.begin ());

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

juce::Result GP200SoundClone::buildFactoryAmpUpload (const juce::File& cloFile,
                                                      int zeroBasedFactoryAmpIndex,
                                                      GP200IRUpload& result)
{
    constexpr int compactBytes = static_cast<int> (gp200DeclaredBytes);
    constexpr int stagedBlobBytes = wrapperBytes + compactBytes;

    if (!cloFile.existsAsFile ())
        return juce::Result::fail ("The selected Factory AMP CLO file does not exist.");
    if (!juce::isPositiveAndBelow (zeroBasedFactoryAmpIndex, 71))
        return juce::Result::fail ("The Factory AMP destination must be between 1 and 71.");

    std::array<juce::uint8, physicalContainerBytes> preparedContainer{};
    const auto preparationResult = prepareSoundCloneModelForGP200 (cloFile, preparedContainer);
    if (preparationResult.failed ())
        return preparationResult;

    std::array<juce::uint8, stagedBlobBytes> blob{};
    blob[0] = 0x0a;
    blob[1] = 0x10;
    blob[2] = 0x18;
    blob[3] = 0x20;
    blob[6] = 0x43; // little-endian 0x4C43 = "CL"
    blob[7] = 0x4c;
    blob[10] = static_cast<juce::uint8> (zeroBasedFactoryAmpIndex);
    blob[11] = 0xa1; // HOT1: activate a verified, unselected Factory AMP destination.

    const auto displayName = cloFile.getFileNameWithoutExtension ().substring (0, 20);
    const auto* nameBytes = displayName.toRawUTF8 ();
    for (int i = 0; i < 16 && nameBytes[i] != 0; ++i)
        blob[12 + i] = static_cast<juce::uint8> (nameBytes[i]);

    std::uint16_t checksum = 0;
    for (int i = 0; i < compactBytes; ++i)
    {
        const auto byte = preparedContainer[static_cast<std::size_t> (i)];
        blob[wrapperBytes + i] = byte;
        checksum = static_cast<std::uint16_t> (checksum + byte);
    }
    blob[8] = static_cast<juce::uint8> (checksum & 0xff);
    blob[9] = static_cast<juce::uint8> ((checksum >> 8) & 0xff);

    result = {};
    result.displayName = displayName;
    result.prepareMessage = makeSysEx (makeUserIRStagePrepare ());
    result.commitMessage = makeSysEx (makeUserIRStageCommit ());

    for (int offset = 0; offset < stagedBlobBytes; offset += chunkBytes)
    {
        const auto amount = juce::jmin (chunkBytes, stagedBlobBytes - offset);
        std::vector<juce::uint8> full {
            0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,
            static_cast<juce::uint8> (stagedBlobBytes & 0x7f),
            static_cast<juce::uint8> ((stagedBlobBytes >> 7) & 0x7f),
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
