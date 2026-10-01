#pragma once
#include <array>
#include <cstdint>
namespace gp200 {
template<class Order> bool validFlexibleRouting (const Order& order, int s, int p, int r)
{
    if (order.size () != 11 || s < 0 || s > p || p > r || r > 11) return false;
    std::array<bool, 11> seen{};
    for (auto id : order) {
        if (id < 0 || id >= 11 || seen[static_cast<std::size_t> (id)]) return false;
        seen[static_cast<std::size_t> (id)] = true;
    }
    return true;
}
inline std::array<std::uint8_t, 46> flexibleRoutingModeMessage (std::uint8_t value)
{
    std::array<std::uint8_t, 46> bytes{
        0xF0, 0x21, 0x25, 0x7E, 0x47, 0x50, 0x2D, 0x32,
        0x12, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x0D, 0x0B, 0x00, 0x00, 0x00, 0x06, 0x00,
        0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xF7
    };
    bytes[41] = static_cast<std::uint8_t> (value >> 4);
    bytes[42] = static_cast<std::uint8_t> (value & 15);
    return bytes;
}
inline bool validRoutingModeValue (int mode)
{ return mode == 0 || mode == 1 || (mode >= 0x80 && mode <= 0x8b) || (mode >= 0x90 && mode <= 0x9b); }
inline bool isExtendedRoutingMode (int mode)
{ return validRoutingModeValue (mode) && mode >= 0x80; }
inline bool routingModeIsParallel (int mode)
{ return mode == 0 || (mode >= 0x80 && mode <= 0x8b); }
inline std::array<std::uint8_t,46> routingModeQueryMessage ()
{ auto bytes = flexibleRoutingModeMessage (0); bytes[8] = 0x11; return bytes; }
// The native receiver emits an 8-byte decoded path/value notification (30 MIDI bytes).
// Also accept the full 16-byte parameter frame used by the editor, without trusting an outgoing echo.
inline int routingModeResponse (const std::uint8_t* data, int size)
{
    if (data == nullptr || (size != 30 && size != 46)) return -1;
    const auto header = flexibleRoutingModeMessage (0);
    for (int i = 0; i < 9; ++i) if (data[i] != header[static_cast<std::size_t> (i)]) return -1;
    // Bytes 9/10 carry decoded payload length, not a fixed 0x10 subcommand.
    // Native FX Loop notifications contain 8 bytes: header length 08 00.
    const int payloadBytes = (size - 14) / 2;
    if (data[9] != payloadBytes || data[10] != 0 || data[11] != 0 || data[12] != 0) return -1;
    if (data[size - 1] != 0xf7) return -1;
    for (int i = 13; i < size - 1; ++i) if (data[i] > 15) return -1;
    const int start = size - 17;
    const std::array<int,6> path {6,0,4,0,5,0};
    for (int i = 0; i < 6; ++i)
        if ((data[start+2*i] << 4 | data[start+2*i+1]) != path[static_cast<std::size_t> (i)]) return -1;
    if (data[size-3] != 0 || data[size-2] != 0) return -1;
    if (size == 46) for (int i=13; i<29; ++i) if (data[i] != header[static_cast<std::size_t> (i)]) return -1;
    const int mode = data[size-5] << 4 | data[size-4];
    return validRoutingModeValue (mode) ? mode : -1;
}

}
