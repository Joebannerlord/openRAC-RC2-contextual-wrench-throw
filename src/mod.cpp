// SPDX-License-Identifier: GPL-3.0-or-later
// Adapts the R2 fire mapping and wrench posing approach from Christian's GPL-3.0-or-later
// RC1 wrench-throw mod: https://github.com/chistianh/wrench-throw
#include "input.hpp"
#include "launch_addresses.hpp"
#include "throw_motion.hpp"

#include "openrac/mod.hpp"

#include <cmath>
#include <memory>
#include <vector>

namespace {

using openrac::gaddr;
using namespace contextual;

// SCUS_972.68 v1.01 resident hero record. Slot 0 and the health/body/state fields are corroborated
// by the stable host (rac2/host/play.c) and the public ReRAC2 hero and hand-item paths.
constexpr gaddr held_moby = 0x18B040;
constexpr gaddr held_trigger = 0x18B050;
constexpr gaddr melee_active = 0x18B059;
constexpr gaddr wrench_thrown = 0x18B05A;
constexpr gaddr held_ready = 0x18B064;
constexpr gaddr held_item = 0x18B068;
constexpr gaddr hero_moby = 0x18C0B0;
constexpr gaddr gravity_walk = 0x18A168;
constexpr gaddr catch_position = 0x18B020; // Translation of the native attachment 0 matrix.
constexpr gaddr vehicle = 0x18B650;
constexpr gaddr hero_state = 0x18C0B4;
constexpr gaddr hero_group = 0x18C0BC;
constexpr gaddr hero_body = 0x18C0D4;
constexpr gaddr no_menu = 0x18C0D7;
constexpr gaddr no_item = 0x18C0DC;
constexpr gaddr no_update = 0x18C0DD;
constexpr gaddr item_tucked = 0x18C0DF;
constexpr gaddr equip_request = 0x18C0E8;
constexpr gaddr next_item = 0x18C104;
constexpr gaddr health = 0x18C2EC;
constexpr gaddr pad_lock = 0x138180 + 0x1CC; // Resident PAD; see stable rac2/host/tools.c.
// The resident per-item byte read by func_L00_002CB038 on USA v1.01.
constexpr gaddr weapon_mods = 0x1A7A80;
constexpr std::uint8_t lock_on_bit = 0x20;
constexpr gaddr moby_position = 0x10;
constexpr gaddr moby_state = 0x20;
constexpr gaddr moby_class = 0xAA;
constexpr int wrench = 10;
constexpr std::int16_t wrench_class = 0x47;
constexpr std::uint8_t in_hand = 0;
constexpr std::uint8_t returning = 11;
constexpr const char* launcher_symbol = "func_L00_002BA508";

using Launch = void (*)();

struct Player {
    openrac_play_status status;
    bool on_foot = false;
    bool allow_throw = false;
    int item = -1;
    std::uint32_t trigger = 0;
    gaddr hand = 0;
};

constexpr bool ship_level(int level) {
    return level == 5 || level == 10 || level == 15 || level == 25;
}

Player player(const openrac::Mod& mod) {
    const auto status = mod.play_status();
    Player current{status};
    if (status.phase != OPENRAC_PLAY_PLAYING || ship_level(status.level)) {
        return current;
    }
    const auto memory = mod.memory();
    const auto state = memory.ref<std::int32_t>(hero_state);
    const auto group = memory.ref<std::int32_t>(hero_group);
    if (!is_foot_group(group) || state == 0x31 || state == 0x61
        || memory.ref<gaddr>(hero_moby) == 0
        || memory.ref<std::int32_t>(health) <= 0 || memory.ref<gaddr>(vehicle) != 0
        || memory.ref<std::uint8_t>(hero_body) != 0 || memory.ref<std::uint8_t>(no_menu) != 0
        || memory.ref<std::uint8_t>(no_item) != 0 || memory.ref<std::uint8_t>(no_update) != 0
        || memory.ref<std::uint8_t>(item_tucked) != 0
        || memory.ref<std::int32_t>(pad_lock) != 0) {
        return current;
    }
    current.on_foot = true;
    const auto hand = memory.ref<gaddr>(held_moby);
    if (hand == 0 || memory.ref<std::int32_t>(held_ready) != 2) {
        return current;
    }
    current.item = memory.ref<std::int32_t>(held_item);
    current.trigger = memory.ref<std::uint32_t>(held_trigger);
    current.hand = hand;
    if (current.item == wrench && current.trigger == Square && group != 0xC
        && state != 1 && state != 0x1D) {
        const auto request = memory.ref<std::int32_t>(equip_request);
        const auto selected = memory.ref<std::int32_t>(next_item);
        current.allow_throw = memory.ref<std::uint8_t>(gravity_walk) == 0
            && memory.ref<std::uint8_t>(melee_active) == 0
            && (request == 0 || request == wrench) && (selected == 0 || selected == wrench)
            && memory.ref<std::int16_t>(hand + moby_class) == wrench_class;
    }
    return current;
}

openrac_joint_offset turn(unsigned joint, float x, float y, float z) {
    const float cx = std::cos(x * 0.5f), sx = std::sin(x * 0.5f);
    const float cy = std::cos(y * 0.5f), sy = std::sin(y * 0.5f);
    const float cz = std::cos(z * 0.5f), sz = std::sin(z * 0.5f);
    return {joint,
            {sx * cy * cz + cx * sy * sz, cx * sy * cz - sx * cy * sz,
             cx * cy * sz + sx * sy * cz, cx * cy * cz - sx * sy * sz},
            {}};
}

std::vector<openrac_joint_offset> arm_joints(ArmPose pose) {
    const auto [aim, release, reach, close] = pose;
    if (aim + release + reach + close == 0.0f) {
        return {};
    }
    // The USA v1.01 Ratchet class's bind skeleton has R shoulder 53, R elbow 54,
    // R forearm 55, R wrist 56 and the left-arm mirror starting at 72.
    // Apply small local rotations over the game's own run/jump, with a separate catch blend.
    return {
        turn(53, 0.55f * aim - 0.25f * release - 0.30f * reach,
             0.18f * aim, -0.45f * aim + 0.82f * release + 0.55f * reach + 0.20f * close),
        turn(54, 0.10f * close, 0.0f, -1.12f * aim + 0.19f * release - 0.30f * reach - 0.68f * close),
        turn(56, 0.65f * aim - 0.18f * release, 0.0f, -0.14f * reach + 0.35f * close),
    };
}

float catch_reach(const openrac::Memory& memory, gaddr hand) {
    float squared = 0.0f;
    for (gaddr axis = 0; axis < 12; axis += 4) {
        const float delta = memory.ref<float>(hand + moby_position + axis)
            - memory.ref<float>(catch_position + axis);
        squared += delta * delta;
    }
    const float distance = std::sqrt(squared);
    return std::clamp((4.5f - distance) / 3.0f, 0.0f, 1.0f);
}

class Controls {
public:
    explicit Controls(openrac::Mod& mod)
        : mod_(mod), launcher_(mod.required_function<Launch>(launcher_symbol)) {}

    void pad(int port, std::uint8_t* bytes, int size) {
        if (port != 0 || bytes == nullptr || size != report_size) {
            return;
        }
        std::span<std::uint8_t, report_size> report(bytes, report_size);
        physical_ = sample(report);
        sampled_ = true;
        const auto current = player(mod_);
        const bool foot_input = current.on_foot && !physical_.opening_menu();
        const bool wrench_input = foot_input && current.allow_throw
            && !physical_.down(R1) && !physical_.down(Circle) && launch_at(current.status.level) != 0;
        const auto chord = square_.update(physical_, wrench_input, foot_input);
        aiming_ = chord.aim;
        if (!wrench_input) {
            cancel();
        } else if (chord.press) {
            if (!same_wrench(current)) {
                cancel();
            }
            hand_ = current.hand;
            loads_ = current.status.loads;
            level_ = current.status.level;
            motion_.request();
        }

        hide_wrench_buttons(report, chord);
        if (foot_input && physical_.down(R2) && current.trigger == Circle && is_weapon(current.item)) {
            const bool lock_on_enabled =
                (mod_.memory().read<std::uint8_t>(weapon_mods + current.item) & lock_on_bit) != 0;
            map_fire(report, physical_, lock_on_enabled);
        }
    }

    void frame() {
        const auto current = player(mod_);
        if (!current.allow_throw || physical_.opening_menu()
            || physical_.down(Circle) || physical_.down(R1)
            || launch_at(current.status.level) == 0) {
            sampled_ = false;
            cancel();
            return;
        }
        if (!sampled_) {
            return;
        }
        sampled_ = false;
        if (!same_wrench(current)) {
            cancel();
            hand_ = current.hand;
            loads_ = current.status.loads;
            level_ = current.status.level;
        }

        const auto memory = mod_.memory();
        const auto state = memory.ref<std::uint8_t>(hand_ + moby_state);
        const bool ready = state == in_hand && memory.ref<std::uint8_t>(wrench_thrown) == 0;
        const bool caught = custom_in_flight_ && ready;
        if (caught) {
            custom_in_flight_ = false;
        }
        const float reach = custom_in_flight_ && state == returning ? catch_reach(memory, hand_) : 0.0f;
        const auto step = motion_.step(aiming_, ready, reach, caught);
        if (step.launch) {
            launch(current.status.level, memory);
        }
        set_pose(step.pose);
    }

private:
    static gaddr launch_at(int level) {
        return level >= 0 && static_cast<std::size_t>(level) < launch_address.size()
            ? launch_address[level] : 0;
    }

    bool same_wrench(const Player& current) const {
        return hand_ == current.hand && loads_ == current.status.loads && level_ == current.status.level;
    }

    void launch(int level, const openrac::Memory& memory) {
        // Looking up the live address also sets the host's active entry for calls and data
        // whose targets vary across copies. The stable PE maps all 27 entries to this function.
        const auto launch_here = mod_.function_at<Launch>(launch_at(level));
        if (launch_here != launcher_) {
            if (!warned_) {
                mod_.warn("Wrench launch resolved to an unexpected function");
                warned_ = true;
            }
            return;
        }
        launch_here();
        custom_in_flight_ = memory.ref<std::uint8_t>(wrench_thrown) != 0;
    }

    void set_pose(ArmPose pose) {
        auto joints = arm_joints(pose);
        if (!joints.empty() || pose_active_) {
            if (mod_.hero_set_joints(joints)) {
                pose_active_ = !joints.empty();
            }
        }
    }

    void cancel() {
        motion_.cancel();
        custom_in_flight_ = false;
        hand_ = 0;
        set_pose({});
    }

    openrac::Mod& mod_;
    Launch launcher_;
    // The last raw sample remains independent of the report passed to the game's pad decoder.
    PadSample physical_{0xFF, false};
    SquarePress square_;
    ThrowMotion motion_;
    gaddr hand_ = 0;
    std::uint32_t loads_ = 0;
    int level_ = -1;
    bool sampled_ = false;
    bool aiming_ = false;
    bool custom_in_flight_ = false;
    bool pose_active_ = false;
    bool warned_ = false;
};

void load(openrac::Mod& mod) {
    if (mod.game() != "rac2" || mod.version() != "ntsc") {
        throw openrac::Error("Contextual Wrench and Fire requires Going Commando USA v1.01");
    }
    if (mod.openrac_version() != "1.1.0") {
        throw openrac::Error("Contextual Wrench and Fire was verified against OpenRAC 1.1.0");
    }
    if (!mod.has_api_12() || mod.api().hero_set_joints == nullptr) {
        throw openrac::Error("Contextual Wrench and Fire requires the joint-offset API");
    }
    auto controls = std::make_shared<Controls>(mod);
    mod.on_pad([controls](int port, std::uint8_t* bytes, int size) { controls->pad(port, bytes, size); });
    mod.on_frame([controls] { controls->frame(); });
    mod.info("R2 fires the drawn weapon and R2 + Square throws the drawn wrench");
}

} // namespace

OPENRAC_MOD_CPP("rac2-contextual-wrench", "0.1.1", load)
