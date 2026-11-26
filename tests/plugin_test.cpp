// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/input.hpp"
#include "../src/launch_addresses.hpp"
#include "openrac/openrac_mod.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using namespace contextual;
using Pad = std::array<std::uint8_t, report_size>;
openrac_play_status status{sizeof(openrac_play_status), OPENRAC_PLAY_PLAYING, 1, 31, 1};
std::array<std::uint8_t, 0x400000> ram{};
openrac_pad_fn pad_callback;
void* pad_user;
openrac_frame_fn frame_callback;
void* frame_user;
int launch_calls = 0;
bool bad_address = false;
bool refusing = false;
bool provide_launcher = true;
int joint_calls = 0;
int joints_set = 0;
bool pose_valid = true;
constexpr std::uint32_t hand = 0x301000;
constexpr std::uint32_t wrench_state = hand + 0x20;
constexpr std::uint32_t wrench_thrown = 0x18B05A;

#ifdef _WIN32
using library_handle = HMODULE;
library_handle open_library(const wchar_t* path) { return LoadLibraryW(path); }
void* load_symbol(library_handle library, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(library, name));
}
void close_library(library_handle library) { FreeLibrary(library); }
constexpr const char* platform = "windows-x86_64";
#else
using library_handle = void*;
library_handle open_library(const char* path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
void* load_symbol(library_handle library, const char* name) { return dlsym(library, name); }
void close_library(library_handle library) { dlclose(library); }
constexpr const char* platform = "linux-x86_64";
#endif

void check(bool ok, const char* why) {
    if (!ok) {
        std::fprintf(stderr, "%s\n", why);
        std::exit(EXIT_FAILURE);
    }
}

template <class T>
void set(std::uint32_t addr, T value) {
    std::memcpy(ram.data() + addr, &value, sizeof value);
}

void native_launch() {
    ++launch_calls;
    set(wrench_state, std::uint8_t{10});
    set(wrench_thrown, std::uint8_t{1});
}

Pad pad(std::uint8_t held = 0, bool start = false, int port = 0, int size = report_size) {
    Pad report{};
    report[0] = start ? static_cast<std::uint8_t>(0xFFu & ~Start) : 0xFF;
    report[buttons] = static_cast<std::uint8_t>(~held);
    report[circle_pressure] = (held & Circle) ? 255 : 0;
    report[square_pressure] = (held & Square) ? 255 : 0;
    report[r2_pressure] = (held & R2) ? 255 : 0;
    pad_callback(pad_user, port, report.data(), size);
    return report;
}

void tick(std::uint8_t held = 0, int count = 1) {
    for (int n = 0; n < count; ++n) {
        pad(held);
        frame_callback(frame_user);
    }
}

void draw(int item) {
    set(0x18B040, hand);
    set(0x18C0B0, std::uint32_t{0x300000});
    set(0x18B050, std::uint32_t{item == 10 ? Square : Circle});
    set(0x18B064, 2);
    set(0x18B068, item);
    set(hand + 0xAA, std::int16_t{item == 10 ? 0x47 : 0x5D2});
    set(0x18C2EC, 4);
    set(wrench_state, std::uint8_t{0});
    set(wrench_thrown, std::uint8_t{0});
}

void check_fire() {
    draw(30);
    auto report = pad(R2 | L2);
    check(report[buttons] == static_cast<std::uint8_t>(~(Circle | L2))
              && report[circle_pressure] == 255 && report[r2_pressure] == 0,
          "R2 maps to native Circle and preserves L2");
    check(pad(Circle)[buttons] == static_cast<std::uint8_t>(~Circle), "physical Circle remains");
    check(pad(Circle | R2)[buttons] == static_cast<std::uint8_t>(~Circle), "both give one Circle hold");
    check(pad(R2, false, 1)[buttons] == static_cast<std::uint8_t>(~R2), "port 1 remains raw");
    check(pad(R2, false, 0, 17)[buttons] == static_cast<std::uint8_t>(~R2), "short report remains raw");
    check(pad(R2, true)[buttons] == static_cast<std::uint8_t>(~R2), "pause press passes raw");
    check(pad(R2 | Triangle)[buttons] == static_cast<std::uint8_t>(~(R2 | Triangle)), "Quick Select press passes raw");
    status.phase = OPENRAC_PLAY_MENU;
    check(pad(R2)[buttons] == static_cast<std::uint8_t>(~R2), "menu R2 passes raw");
    status.phase = OPENRAC_PLAY_PLAYING;
    set(0x13834C, 1);
    check(pad(R2)[buttons] == static_cast<std::uint8_t>(~R2), "locked pad R2 passes raw");
    set(0x13834C, 0);
    set(0x18C0DC, std::uint8_t{1});
    check(pad(R2)[buttons] == static_cast<std::uint8_t>(~R2), "item lock R2 passes raw");
    set(0x18C0DC, std::uint8_t{0});
    set(0x18B650, hand);
    set(0x18C0B4, 0x31);
    check(pad(R2)[buttons] == static_cast<std::uint8_t>(~R2), "mounted R2 passes raw");
    set(0x18B650, std::uint32_t{0});
    set(0x18C0B4, 0);
    status.level = 5;
    check(pad(R2)[buttons] == static_cast<std::uint8_t>(~R2), "space combat R2 passes raw");
    status.level = 1;
    draw(36);
    check(pad(R2)[buttons] == static_cast<std::uint8_t>(~R2), "gadget R2 passes raw");
}

void check_lock_on() {
    constexpr std::uint32_t mods = 0x1A7A80;
    draw(30);
    set(mods + 30, std::uint8_t{0x10});
    check(pad(L2 | R2)[buttons] == static_cast<std::uint8_t>(~(L2 | Circle)),
          "ownership bit alone does not enable the lock-on chord");

    set(mods + 30, std::uint8_t{0x30});
    auto report = pad(R2);
    check(report[buttons] == static_cast<std::uint8_t>(~Circle) && report[r2_pressure] == 0,
          "R2 alone fires with Lock-On equipped");
    for (std::uint8_t held : {L2 | R2, L2 | R2 | Circle}) {
        report = pad(held);
        check(report[buttons] == static_cast<std::uint8_t>(~(L2 | R2 | Circle))
                  && report[circle_pressure] == 255 && report[r2_pressure] == 255,
              "upgraded weapon receives the native shoulder chord and Circle fire");
    }
    check(pad(L2)[buttons] == static_cast<std::uint8_t>(~L2), "releasing R2 releases native chord and fire");
    check(pad(R2)[buttons] == static_cast<std::uint8_t>(~Circle), "releasing L2 releases native chord");
    check(pad(L2 | Circle)[buttons] == static_cast<std::uint8_t>(~(L2 | Circle)), "physical Circle and L2 stay raw");
    draw(42);
    check(pad(L2 | R2)[buttons] == static_cast<std::uint8_t>(~(L2 | Circle)),
          "equipping another weapon reads its own Lock-On bit");
    set(mods + 30, std::uint8_t{0});
}

void check_wrench() {
    draw(10);
    set(0x18C0B4, 5);
    set(0x18C0BC, 1);
    tick();
    check(pad(Square)[buttons] == static_cast<std::uint8_t>(~Square), "normal Square remains");
    check(pad(Square | R2)[buttons] == static_cast<std::uint8_t>(~Square), "held Square gives no new throw");
    tick();
    check(pad(R1 | Square)[buttons] == static_cast<std::uint8_t>(~(R1 | Square)), "stock comet remains");
    tick();
    const auto chord = pad(L2 | R2 | Square);
    check(chord[buttons] == static_cast<std::uint8_t>(~L2)
              && chord[r2_pressure] == 0 && chord[square_pressure] == 0,
          "throw chord suppresses its buttons and preserves L2");
    check(pad(Square)[buttons] == 0xFF, "releasing R2 first does not fabricate a Square press");
    tick(Square, 20);
    check(launch_calls == 1 && !bad_address && pose_valid && joint_calls > 0, "running throw and release pose");
    tick(R2 | Square, 25);
    check(launch_calls == 1, "held Square does not repeat");
    tick();
    set(0x18C0B4, 7);
    set(0x18C0BC, 4);
    tick(R2 | Square, 5);
    check(launch_calls == 1, "new chord waits for the existing throw to return");
    set(wrench_state, std::uint8_t{11});
    set(hand + 0x10, 1.5f);
    tick(0, 5);
    check(joints_set == 3, "return near the hand reaches for the wrench");
    const auto last_pose_update = joint_calls;
    set(wrench_state, std::uint8_t{0});
    set(wrench_thrown, std::uint8_t{0});
    tick();
    check(launch_calls == 1 && joint_calls > last_pose_update && joints_set == 3, "catch pose at native return");
    tick(0, 19);
    check(launch_calls == 2 && !bad_address && pose_valid, "buffered jumping throw follows the catch");
}

void check_cancellation() {
    for (int transition = 0; transition < 5; ++transition) {
        status.level = 1;
        draw(10);
        tick();
        tick(R2 | Square, 4);
        switch (transition) {
            case 0: status.phase = OPENRAC_PLAY_MENU; break;
            case 1: draw(30); break;
            case 2: ++status.loads; break;
            case 3: set(0x18B650, hand); break;
            case 4: set(0x18C2EC, 0); break;
        }
        tick(R2 | Square);
        status.phase = OPENRAC_PLAY_PLAYING;
        draw(10);
        set(0x18B650, std::uint32_t{0});
        tick(R2 | Square, 22);
        check(launch_calls == 2, "context change cancels pending and held chord cannot re-arm it");
    }
    draw(10);
    set(wrench_state, std::uint8_t{10});
    set(wrench_thrown, std::uint8_t{1});
    tick();
    tick(R2 | Square);
    tick(0, 100);
    draw(10);
    tick(0, 22);
    check(launch_calls == 2 && joints_set == 0, "expired request stays cancelled at catch");
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    check(argc == 2, "expected path to packaged plugin");
    const auto library = open_library(argv[1]);
    check(library != nullptr, "opening plugin library failed");
    const auto query = reinterpret_cast<openrac_mod_query_fn>(load_symbol(library, OPENRAC_MOD_QUERY));
    const auto load = reinterpret_cast<openrac_mod_load_fn>(load_symbol(library, OPENRAC_MOD_LOAD));
    check(query && load, "mod exports missing");
    openrac_mod_info info{};
    info.size = sizeof info;
    query(&info);
    check(info.api_major == 1 && info.api_minor == 12
              && std::strcmp(info.id, "rac2-contextual-wrench") == 0
              && std::strcmp(info.version, "0.1.2") == 0, "plugin query matches manifest");

    openrac_api api{};
    api.size = sizeof api;
    api.api_major = 1;
    api.api_minor = 20;
    api.game = "rac2";
    api.version = "ntsc";
    api.openrac_version = "1.1.0";
    api.platform = platform;
    api.memory = ram.data();
    api.fail = [](openrac_mod*, const char*) { refusing = true; };
    api.log = [](openrac_mod*, int, const char*) {};
    api.on_pad = [](openrac_mod*, openrac_pad_fn fn, void* user) {
        pad_callback = fn;
        pad_user = user;
        return 0;
    };
    api.on_frame = [](openrac_mod*, openrac_frame_fn fn, void* user) {
        frame_callback = fn;
        frame_user = user;
        return 0;
    };
    api.function = [](const char* name) -> void* {
        return provide_launcher && std::strcmp(name, "func_L00_002BA508") == 0
            ? reinterpret_cast<void*>(&native_launch) : nullptr;
    };
    api.function_at = [](openrac_gaddr address) -> void* {
        bad_address |= address != launch_address.at(status.level);
        return bad_address ? nullptr : reinterpret_cast<void*>(&native_launch);
    };
    api.play_status = [](openrac_play_status* out) {
        *out = status;
        return 0;
    };
    api.hero_set_joints = [](openrac_mod*, const openrac_joint_offset* joints, int count) {
        ++joint_calls;
        joints_set = count;
        constexpr std::array<unsigned, 3> indices{53, 54, 56};
        for (int i = 0; i < count; ++i) {
            float norm = 0.0f;
            for (float component : joints[i].rotation) {
                norm += component * component;
            }
            pose_valid &= i < 3 && joints[i].joint == indices[i] && std::fabs(1.0f - norm) < 1e-5f;
        }
        return 0;
    };
    auto* mod = reinterpret_cast<openrac_mod*>(1);
    check(load(&api, mod) == 0 && !refusing && pad_callback && frame_callback, "plugin load failed");
    check_fire();
    check_lock_on();
    check_wrench();
    check_cancellation();
    provide_launcher = false;
    check(load(&api, mod) != 0 && refusing, "missing native symbol must refuse load");
    refusing = false;
    api.version = "ntsc2";
    check(load(&api, mod) != 0 && refusing, "USA v2.00 must refuse load");
    close_library(library);
}
