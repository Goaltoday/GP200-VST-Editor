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
}
