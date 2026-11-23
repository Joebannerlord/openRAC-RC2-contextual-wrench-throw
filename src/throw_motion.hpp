// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>

namespace contextual {

struct ArmPose {
    float aim = 0.0f;
    float release = 0.0f;
    float catch_reach = 0.0f;
    float catch_close = 0.0f;
};

struct ThrowStep {
    ArmPose pose;
    bool launch = false;
};

// Counts hero ticks while the same wrench and gameplay context are valid.
class ThrowMotion {
public:
    void request() {
        if (pending_ == 0) {
            pending_ = buffer_ticks;
        }
    }

    ThrowStep step(bool aim, bool ready, float returning, bool caught) {
        if (pending_ > 0) {
            --pending_;
        }
        if (swing_ >= 0 && ++swing_ == final_tick) {
            swing_ = -1;
        }
        if (swing_ >= 0 && swing_ <= release_tick && (!ready || pending_ == 0)) {
            swing_ = -1;
        }
        if (swing_ < 0 && pending_ > 0 && ready) {
            swing_ = 0;
        }
        const bool launch = swing_ == release_tick;
        if (launch) {
            pending_ = 0;
        }

        aim_ = std::clamp(aim_ + (aim && ready && swing_ < 0 ? 0.125f : -0.125f), 0.0f, 1.0f);
        reach_ += std::clamp(returning - reach_, -0.18f, 0.25f);
        close_ = caught ? 1.0f : std::max(0.0f, close_ - 0.125f);

        float wind = 0.0f;
        float release = 0.0f;
        if (swing_ >= 0) {
            const float load = std::min(1.0f, swing_ / 4.0f);
            const float snap = std::clamp((swing_ - 4) / 3.0f, 0.0f, 1.0f);
            const float follow = std::max(0.0f, (swing_ - release_tick) / 12.0f);
            wind = ease(load) * (1.0f - ease(snap));
            release = ease(snap) * (1.0f - ease(follow));
        }
        return {{std::max(ease(aim_), wind), release, ease(reach_), ease(close_)}, launch};
    }

    void cancel() { *this = {}; }

private:
    static constexpr int buffer_ticks = 90;
    static constexpr int release_tick = 8;
    static constexpr int final_tick = 21;

    static float ease(float t) { return t * t * (3.0f - 2.0f * t); }

    int pending_ = 0;
    int swing_ = -1;
    float aim_ = 0.0f;
    float reach_ = 0.0f;
    float close_ = 0.0f;
};

} // namespace contextual
