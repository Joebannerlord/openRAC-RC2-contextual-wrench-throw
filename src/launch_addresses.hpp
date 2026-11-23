// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>

namespace contextual {

// SCUS_972.68 USA v1.01, func_L00_002BA508 in the stable host's function table.
// Indexed by overlay ID, including the museum at 30. See tests/verify_target.py.
constexpr std::array<std::uint32_t, 31> launch_address = {
    0x2BA508, 0x2B9EF0, 0x2BD988, 0x2BF620, 0x2E01D8, 0x2D2D88, 0x2F9C88, 0x2B8348,
    0x2CB418, 0x2BC1C0, 0x2D5288, 0x2D1FF8, 0x2BFD38, 0x2C5490, 0x2C1E08, 0x2D3388,
    0x2B9D20, 0x2BBA10, 0x2D7380, 0x2BC780, 0x2CD658,        0, 0x2BEEE0, 0x2C35E0,
    0x2B9608, 0x2CD708, 0x2BAC88,        0,        0,        0, 0x2C1308,
};

} // namespace contextual
