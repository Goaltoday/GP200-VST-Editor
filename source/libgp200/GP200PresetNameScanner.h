#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace gp200
{
class GP200PresetNameScanner final
{
  public:
    void start (int prioritySlot);
    void cancel ();

    bool shouldSendNextRequest (double nowMs) const noexcept;
    std::vector<juce::uint8> beginNextRequest (double nowMs);
    bool handleSysEx (const juce::uint8* data, int size, double nowMs);
    void handleTimeout (double nowMs);

    bool isScanning () const noexcept;
    bool hasPendingRequest () const noexcept;
    float getProgress () const noexcept;
    int getCompletedCount () const noexcept;
    std::uint64_t getRevision () const noexcept;

    bool hasName (int slot) const noexcept;
    bool isCacheComplete () const noexcept;
    juce::String getName (int slot) const;
    void setCachedName (int slot, const juce::String& name);
    void invalidateSlot (int slot);

  private:
    enum class ReadMode
    {
        probeFast,
        fast,
        full
    };

    static std::vector<juce::uint8> buildReadRequest (int slot, bool fastNameOnly);
    static std::vector<juce::uint8> nibbleDecode (const juce::uint8* data, int size);
    static juce::String extractName (const juce::uint8* data, int size);

    void finishPending (const juce::String& name, bool validName, double nowMs);
    void retryPendingWithFullRead (double nowMs);

    std::array<juce::String, 256> names{};
    std::array<bool, 256> valid{};
    std::vector<int> queue;

    int queueIndex{0};
    int pendingSlot{-1};
    int completedCount{0};
    double requestSentAtMs{0.0};
    double nextRequestNotBeforeMs{0.0};
    bool pendingUsesFastRead{true};
    bool scanning{false};
    bool cacheComplete{false};
    ReadMode readMode{ReadMode::probeFast};
    std::uint64_t revision{0};

    static constexpr double fastTimeoutMs = 250.0;
    static constexpr double fullTimeoutMs = 650.0;
    static constexpr double fullReadDrainMs = 45.0;
};
} // namespace gp200
