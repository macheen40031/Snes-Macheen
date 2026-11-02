#include "controller_mapper.hpp"

#include <algorithm>

namespace {
constexpr std::size_t controlIndex(GamepadControl control) {
    return static_cast<std::size_t>(control);
}
}

ControllerMapper::ControllerMapper() { reset(); }

bool ControllerMapper::validPlayer(std::size_t player) noexcept {
    return player < PLAYER_COUNT;
}

uint16_t ControllerMapper::buttonMask(SnesButton button) noexcept {
    const auto bit = static_cast<unsigned>(button);
    return bit < 12u ? static_cast<uint16_t>(1u << bit) : 0u;
}

void ControllerMapper::reset() {
    // SDL's standardized game-controller face layout maps naturally to the
    // conventional SNES diamond: South=B, East=A, West=Y, North=X.
    constexpr std::array<SnesButton, controlIndex(GamepadControl::Count)> defaults{
        SnesButton::B, SnesButton::A, SnesButton::Y, SnesButton::X,
        SnesButton::Select, SnesButton::Start, SnesButton::Up,
        SnesButton::Down, SnesButton::Left, SnesButton::Right,
        SnesButton::L, SnesButton::R
    };
    for (auto& player : players) {
        player.mappings = defaults;
        player.pressed.fill(false);
        player.leftX = 0;
        player.leftY = 0;
    }
    deadzone = DEFAULT_DEADZONE;
}

bool ControllerMapper::setMapping(std::size_t player, GamepadControl control,
                                  SnesButton target) {
    const auto index = controlIndex(control);
    if (!validPlayer(player) || index >= controlIndex(GamepadControl::Count) ||
        static_cast<unsigned>(target) >= 12u)
        return false;
    players[player].mappings[index] = target;
    return true;
}

SnesButton ControllerMapper::mapping(std::size_t player, GamepadControl control) const {
    const auto index = controlIndex(control);
    if (!validPlayer(player) || index >= controlIndex(GamepadControl::Count))
        return SnesButton::B;
    return players[player].mappings[index];
}

void ControllerMapper::setButton(std::size_t player, GamepadControl control, bool pressed) {
    const auto index = controlIndex(control);
    if (!validPlayer(player) || index >= controlIndex(GamepadControl::Count)) return;
    players[player].pressed[index] = pressed;
}

void ControllerMapper::setLeftStick(std::size_t player, int16_t x, int16_t y) {
    if (!validPlayer(player)) return;
    players[player].leftX = x;
    players[player].leftY = y;
}

void ControllerMapper::setDeadzone(int16_t value) noexcept {
    // Keep a nonzero deadzone and avoid signed overflow for abs(-32768).
    const int valueWide = static_cast<int>(value);
    deadzone = static_cast<int16_t>(std::clamp(valueWide, 1000, 20000));
}

uint16_t ControllerMapper::state(std::size_t player) const noexcept {
    if (!validPlayer(player)) return 0;
    const auto& current = players[player];
    uint16_t result = 0;
    for (std::size_t i = 0; i < current.pressed.size(); ++i) {
        if (current.pressed[i]) result |= buttonMask(current.mappings[i]);
    }

    const int x = static_cast<int>(current.leftX);
    const int y = static_cast<int>(current.leftY);
    const int threshold = static_cast<int>(deadzone);
    if (y <= -threshold) result |= buttonMask(SnesButton::Up);
    if (y >= threshold) result |= buttonMask(SnesButton::Down);
    if (x <= -threshold) result |= buttonMask(SnesButton::Left);
    if (x >= threshold) result |= buttonMask(SnesButton::Right);
    return result;
}
