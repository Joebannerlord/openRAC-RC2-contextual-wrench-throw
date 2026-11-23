// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/input.hpp"

#include <array>
#include <cstdlib>

using namespace contextual;

int main() {
    for (unsigned raw = 0; raw <= 255; ++raw) {
        std::array<std::uint8_t, report_size> report{};
        report[0] = 0xFF;
        report[buttons] = static_cast<std::uint8_t>(raw);
        report[circle_pressure] = (raw & Circle) ? 0 : 255;
        report[r2_pressure] = (raw & R2) ? 0 : 255;
        const auto physical = sample(report);
        const auto original = report;
        for (bool lock_on : {false, true}) {
            report = original;
            map_fire(report, physical, lock_on);
            if (physical.physical != raw || physical.down(Circle) != !(raw & Circle)) {
                return EXIT_FAILURE;
            }
            if (raw & R2) {
                if (original != report) {
                    return EXIT_FAILURE;
                }
                continue;
            }
            const bool keep_r2 = lock_on && !(raw & L2);
            const unsigned expected = ((keep_r2 ? raw : raw | R2) & ~Circle);
            if (report[buttons] != expected || report[circle_pressure] != 255
                || report[r2_pressure] != (keep_r2 ? 255 : 0)) {
                return EXIT_FAILURE;
            }
        }
    }
    static_assert(is_weapon(30) && is_weapon(42) && is_weapon(16) && is_weapon(53));
    static_assert(!is_weapon(10) && !is_weapon(13) && !is_weapon(36) && !is_weapon(46));
    return EXIT_SUCCESS;
}
