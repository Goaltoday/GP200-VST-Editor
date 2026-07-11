#include "MidiDeviceScanner.h"

namespace gp200
{
namespace
{
bool findFirstMatchingDevice (const juce::Array<juce::MidiDeviceInfo>& devices, juce::MidiDeviceInfo& result)
{
    for (const auto& device : devices)
    {
        if (MidiDeviceScanner::looksLikeGP200 (device))
        {
            result = device;
            return true;
        }
    }

    return false;
}
} // namespace

MidiDeviceList MidiDeviceScanner::scan ()
{
    MidiDeviceList result;

    for (const auto& device : juce::MidiInput::getAvailableDevices ())
        result.inputs.add (device.name);

    for (const auto& device : juce::MidiOutput::getAvailableDevices ())
        result.outputs.add (device.name);

    return result;
}

bool MidiDeviceScanner::looksLikeGP200 (const juce::String& deviceName)
{
    const auto name = deviceName.toLowerCase ();

    return name.contains ("gp-200") || name.contains ("gp200") || name.contains ("valeton");
}

bool MidiDeviceScanner::looksLikeGP200 (const juce::MidiDeviceInfo& device)
{
    return looksLikeGP200 (device.name);
}

bool MidiDeviceScanner::findFirstGP200Input (juce::MidiDeviceInfo& result)
{
    return findFirstMatchingDevice (juce::MidiInput::getAvailableDevices (), result);
}

bool MidiDeviceScanner::findFirstGP200Output (juce::MidiDeviceInfo& result)
{
    return findFirstMatchingDevice (juce::MidiOutput::getAvailableDevices (), result);
}
} // namespace gp200
