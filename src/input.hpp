// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace contextual {

enum Button : std::uint8_t { L2 = 1, R2 = 2, R1 = 8, Triangle = 16, Circle = 32, Square = 128 };
constexpr std::uint8_t Start = 8; // First report byte.
constexpr int report_size = 18;
constexpr int buttons = 1;
constexpr int circle_pressure = 11;
constexpr int square_pressure = 13;
constexpr int r2_pressure = 17;

struct PadSample {
    std::uint8_t physical;
    bool start;

    bool down(Button button) const { return (physical & button) == 0; }
    bool opening_menu() const { return start || down(Triangle); }
};

inline PadSample sample(std::span<const std::uint8_t, report_size> report) {
    return {report[buttons], (report[0] & Start) == 0};
}

// Base IDs from the stable build's save_edit_rac2.rs weapon table. Upgrade rows retain the same
// held item ID. The native slot must also have a drawn moby, which excludes passive weapons.
constexpr bool is_weapon(int item) {
    return (item >= 22 && item <= 32) || (item >= 41 && item <= 45)
        || item == 12 || item == 14 || item == 16 || item == 17 || item == 18 || item == 37
        || item == 53 || item == 9;
}

constexpr bool is_foot_group(int group) {
    return group == 0 || group == 1 || group == 2 || group == 4 || group == 0xC;
}

// Keep the physical byte separate so a later gadget action can use Circle before remapping.
// This mapping retains a single native Circle hold when both inputs are held.
inline void map_fire(std::span<std::uint8_t, report_size> report, PadSample physical, bool lock_on_enabled) {
    if (!physical.down(R2)) {
        return;
    }
    report[buttons] &= static_cast<std::uint8_t>(~Circle);
    report[circle_pressure] = std::max(report[circle_pressure], report[r2_pressure]);
    // Native lock-on reads both shoulder buttons. Keep that chord alongside Circle fire.
    if (!lock_on_enabled || !physical.down(L2)) {
        report[buttons] |= R2;
        report[r2_pressure] = 0;
    }
}

struct WrenchChord {
    bool press = false;
    bool aim = false;
    bool hide_square = false;
};

class SquarePress {
public:
    WrenchChord update(PadSample physical, bool allow_wrench, bool on_foot) {
        const bool square = physical.down(Square);
        const bool aim = allow_wrench && physical.down(R2);
        const bool press = square && !was_down_ && aim;
        was_down_ = square;
        consumed_ = square && (consumed_ || press);
        // After consuming the press, keep Square up until its physical release. If R2 is
        // released first, sending a held Square here would fabricate a melee press.
        return {press, aim, on_foot && consumed_};
    }

private:
    bool was_down_ = true;
    bool consumed_ = false;
};

inline void hide_wrench_buttons(std::span<std::uint8_t, report_size> report, WrenchChord chord) {
    if (chord.aim) {
        report[buttons] |= R2;
        report[r2_pressure] = 0;
    }
    if (chord.hide_square) {
        report[buttons] |= Square;
        report[square_pressure] = 0;
    }
}

} // namespace contextual
