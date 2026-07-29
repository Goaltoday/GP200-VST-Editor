#include "GP200PresetNameScanner.h"

namespace gp200
{
void GP200PresetNameScanner::start (int prioritySlot)
{
    if (cacheComplete)
    {
        scanning = false;
        pendingSlot = -1;
        ++revision;
        return;
    }

    queue.clear ();
    queue.reserve (256);

    const auto safeSlot = juce::jlimit (0, 255, prioritySlot >= 0 ? prioritySlot : 0);
    const int bankStart = (safeSlot / 4) * 4;

    for (int i = 0; i < 4; ++i)
        queue.push_back (bankStart + i);

    for (int slot = 0; slot < 256; ++slot)
        if (slot < bankStart || slot >= bankStart + 4)
            queue.push_back (slot);

    queueIndex = 0;
    pendingSlot = -1;
    completedCount = 0;
    requestSentAtMs = 0.0;
    nextRequestNotBeforeMs = 0.0;
    pendingUsesFastRead = true;
    readMode = ReadMode::probeFast;
    scanning = true;
    ++revision;
}

void GP200PresetNameScanner::cancel ()
{
    scanning = false;
    pendingSlot = -1;
    requestSentAtMs = 0.0;
    nextRequestNotBeforeMs = 0.0;
    queue.clear ();
    queueIndex = 0;
    completedCount = 0;
    ++revision;
}

bool GP200PresetNameScanner::shouldSendNextRequest (double nowMs) const noexcept
{
    return scanning && pendingSlot < 0
           && queueIndex < static_cast<int> (queue.size ())
           && nowMs >= nextRequestNotBeforeMs;
}

std::vector<juce::uint8> GP200PresetNameScanner::beginNextRequest (double nowMs)
{
    if (!shouldSendNextRequest (nowMs))
        return {};

    while (queueIndex < static_cast<int> (queue.size ())
           && valid[static_cast<std::size_t> (queue[queueIndex])])
    {
        ++queueIndex;
        ++completedCount;
    }

    if (queueIndex >= static_cast<int> (queue.size ()))
    {
        scanning = false;
        cacheComplete = true;
        ++revision;
        return {};
    }

    pendingSlot = queue[static_cast<std::size_t> (queueIndex)];
    pendingUsesFastRead = readMode != ReadMode::full;
    requestSentAtMs = nowMs;
    return buildReadRequest (pendingSlot, pendingUsesFastRead);
}

bool GP200PresetNameScanner::handleSysEx (const juce::uint8* data, int size, double nowMs)
{
    if (!scanning || pendingSlot < 0 || data == nullptr || size < 15)
        return false;

    if (data[8] != 0x12 || data[9] != 0x18)
        return false;

    const int offset = data[11] | (data[12] << 8);
    if (offset != 0)
        return false;

    const auto name = extractName (data, size);

    if (pendingUsesFastRead && readMode == ReadMode::probeFast)
        readMode = ReadMode::fast;

    finishPending (name, name.isNotEmpty (), nowMs);
    return true;
}

void GP200PresetNameScanner::handleTimeout (double nowMs)
{
    if (!scanning || pendingSlot < 0)
        return;

    const double timeout = pendingUsesFastRead ? fastTimeoutMs : fullTimeoutMs;
    if (nowMs - requestSentAtMs < timeout)
        return;

    if (pendingUsesFastRead && readMode == ReadMode::probeFast)
    {
        retryPendingWithFullRead (nowMs);
        return;
    }

    finishPending ({}, false, nowMs);
}

bool GP200PresetNameScanner::isScanning () const noexcept { return scanning; }
bool GP200PresetNameScanner::hasPendingRequest () const noexcept { return pendingSlot >= 0; }

float GP200PresetNameScanner::getProgress () const noexcept
{
    return juce::jlimit (0.0f, 1.0f, static_cast<float> (completedCount) / 256.0f);
}

int GP200PresetNameScanner::getCompletedCount () const noexcept { return completedCount; }
std::uint64_t GP200PresetNameScanner::getRevision () const noexcept { return revision; }

bool GP200PresetNameScanner::hasName (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, 256) && valid[static_cast<std::size_t> (slot)];
}

bool GP200PresetNameScanner::isCacheComplete () const noexcept
{
    return cacheComplete;
}

juce::String GP200PresetNameScanner::getName (int slot) const
{
    return hasName (slot) ? names[static_cast<std::size_t> (slot)] : juce::String{};
}

void GP200PresetNameScanner::finishPending (const juce::String& name, bool validName, double nowMs)
{
    if (pendingSlot >= 0 && pendingSlot < 256)
    {
        if (validName)
        {
            names[static_cast<std::size_t> (pendingSlot)] = name;
            valid[static_cast<std::size_t> (pendingSlot)] = true;
        }
        else
        {
            names[static_cast<std::size_t> (pendingSlot)].clear ();
            valid[static_cast<std::size_t> (pendingSlot)] = false;
        }
    }

    ++queueIndex;
    ++completedCount;
    pendingSlot = -1;
    requestSentAtMs = 0.0;
    nextRequestNotBeforeMs = pendingUsesFastRead ? nowMs : nowMs + fullReadDrainMs;
    ++revision;

    if (queueIndex >= static_cast<int> (queue.size ()))
    {
        scanning = false;
        cacheComplete = true;
        ++revision;
    }
}

void GP200PresetNameScanner::retryPendingWithFullRead (double nowMs)
{
    // Keep the same slot pending. MidiConnection will ask beginNextRequest()
    // again on the next timer tick, now using the proven-compatible full read.
    readMode = ReadMode::full;
    pendingSlot = -1;
    requestSentAtMs = 0.0;
    nextRequestNotBeforeMs = nowMs;
    ++revision;
}

std::vector<juce::uint8> GP200PresetNameScanner::buildReadRequest (int slot, bool fastNameOnly)
{
    const auto sh = static_cast<juce::uint8> ((slot >> 4) & 0x0F);
    const auto sl = static_cast<juce::uint8> (slot & 0x0F);

    return {0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32,
            0x11, static_cast<juce::uint8> (fastNameOnly ? 0x20 : 0x10),
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, sh,   sl,   0x00, 0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x04, 0x00, 0x00, sh,   sl,   0x00,
            0x00, sh,   sl,   0x00, 0x00, 0xF7};
}

std::vector<juce::uint8> GP200PresetNameScanner::nibbleDecode (const juce::uint8* data, int size)
{
    std::vector<juce::uint8> decoded;
    decoded.reserve (static_cast<std::size_t> (juce::jmax (0, size / 2)));

    for (int i = 0; i + 1 < size; i += 2)
        decoded.push_back (static_cast<juce::uint8> (((data[i] & 0x0F) << 4) | (data[i + 1] & 0x0F)));

    return decoded;
}

juce::String GP200PresetNameScanner::extractName (const juce::uint8* data, int size)
{
    const auto decoded = nibbleDecode (data + 13, size - 14);
    if (decoded.size () < 44)
        return {};

    juce::String name;
    for (int i = 28; i < 44; ++i)
    {
        const auto c = decoded[static_cast<std::size_t> (i)];
        if (c == 0)
            break;

        if (c >= 32 && c < 127)
            name += juce::String::charToString (static_cast<juce_wchar> (c));
    }

    return name.trim ().substring (0, 16);
}
} // namespace gp200
