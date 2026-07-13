#include "GP200IR.h"

#include <array>
#include <cmath>

namespace gp200
{
namespace
{
constexpr int sampleCount = 1024;
constexpr int blobSize = 4124;
constexpr int audioOffset = 28;
constexpr int chunkSize = 183;

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

std::int32_t floatToPcm24 (float sample)
{
    sample = juce::jlimit (-1.0f, 1.0f, sample);

    const auto scaled = static_cast<std::int64_t> (
        std::llround (static_cast<double> (sample) * 8388608.0));

    return static_cast<std::int32_t> (
        juce::jlimit<std::int64_t> (-8388608, 8388607, scaled));
}

std::vector<juce::uint8> makePrepare (int slot)
{
    return { 0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,0x14,
             0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
             0x00,0x00,0x00,0x08,0x0b,0x00,0x00,0x01,0x06,0x00,0x00,0x00,0x08,
             0x00,0x00,0x00,0x05,0x0f,0x04,0x08,0x0f,0x00,0x00,0x00,
             static_cast<juce::uint8> (slot),0x00,0x00,0x01,0x00,0x00,0x0a,0xf7 };
}

std::vector<juce::uint8> makeCommit (int slot)
{
    return { 0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,0x12,0x0c,
             0x00,0x00,0x00,0x00,0x0b,0x01,0x00,0x00,0x08,0x00,0x00,0x00,
             static_cast<juce::uint8> (slot),0x00,0x00,0x00,0x01,0x00,0x00,
             0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf7 };
}
}

juce::Result GP200IR::buildUpload (const juce::File& wavFile,
                                   int zeroBasedUserIRSlot,
                                   GP200IRUpload& result)
{
    if (!wavFile.existsAsFile())
{
    return juce::Result::fail(
        "The selected IR file does not exist.");
}

if (!wavFile.hasFileExtension("wav"))
{
    return juce::Result::fail(
        "The selected IR must be a WAV file.");
}

if (!juce::isPositiveAndBelow(
        zeroBasedUserIRSlot,
        20))
{
    return juce::Result::fail(
        "The destination must be between User IR 1 and User IR 20.");
}

juce::AudioFormatManager formats;
formats.registerBasicFormats();

std::unique_ptr<juce::AudioFormatReader> reader(
    formats.createReaderFor(wavFile));

if (reader == nullptr)
{
    return juce::Result::fail(
        "The WAV file is damaged, unsupported or cannot be read.");
}

if (reader->lengthInSamples <= 0)
{
    return juce::Result::fail(
        "The WAV file contains no audio samples.");
}

if (reader->numChannels != 1)
{
    return juce::Result::fail(
        "The IR must be mono. Stereo WAV files are not supported yet.");
}

if (std::abs(reader->sampleRate - 44100.0) > 0.5)
{
    return juce::Result::fail(
        "The IR must use a sample rate of 44.1 kHz.");
}

    juce::AudioBuffer<float> buffer (1, sampleCount);
    buffer.clear ();
    const auto samplesToRead = static_cast<int> (juce::jmin<juce::int64> (sampleCount, reader->lengthInSamples));
    reader->read (&buffer, 0, samplesToRead, 0, true, false);

    std::array<juce::uint8, blobSize> blob{};
    blob[0]=0x0a; blob[1]=0x10; blob[2]=0x18; blob[3]=0x10;
    blob[4]=static_cast<juce::uint8> (zeroBasedUserIRSlot);

    const auto displayName =
        wavFile.getFileNameWithoutExtension ().substring (0, 16);
    const auto* nameBytes = displayName.toRawUTF8 ();

    for (int i = 0; i < 16 && nameBytes[i] != 0; ++i)
        blob[12 + i] = static_cast<juce::uint8> (nameBytes[i]);

    for (int i=0; i<sampleCount; ++i)
    {
        const auto value = floatToPcm24 (buffer.getSample (0, i));
        const auto p = audioOffset + i*4;
        blob[p+0]=static_cast<juce::uint8> (value & 0xff);
        blob[p+1]=static_cast<juce::uint8> ((value >> 8) & 0xff);
        blob[p+2]=static_cast<juce::uint8> ((value >> 16) & 0xff);
        blob[p+3]=static_cast<juce::uint8> ((value >> 24) & 0xff);
    }

    std::uint16_t checksum=0;
    for (int i=audioOffset; i<blobSize; ++i)
        checksum=static_cast<std::uint16_t> (checksum + blob[i]);
    blob[8]=static_cast<juce::uint8> (checksum & 0xff);
    blob[9]=static_cast<juce::uint8> ((checksum >> 8) & 0xff);

    result = {};
    result.displayName = displayName;
    result.prepareMessage = makeSysEx (makePrepare (zeroBasedUserIRSlot));
    result.commitMessage = makeSysEx (makeCommit (zeroBasedUserIRSlot));

    for (int offset=0; offset<blobSize; offset+=chunkSize)
    {
        const auto amount = juce::jmin (chunkSize, blobSize-offset);
        std::vector<juce::uint8> full { 0xf0,0x21,0x25,0x7e,0x47,0x50,0x2d,0x32,
                                       0x12,0x1c,0x20,
                                       static_cast<juce::uint8> (offset & 0x7f),
                                       static_cast<juce::uint8> ((offset >> 7) & 0x7f) };
        auto encoded = nibbleEncode (blob.data () + offset, amount);
        full.insert (full.end (), encoded.begin (), encoded.end ());
        full.push_back (0xf7);
        result.chunks.push_back (makeSysEx (full));
    }
    return juce::Result::ok ();
}
} // namespace gp200
