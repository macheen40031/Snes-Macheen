#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

enum class SnesButton : uint8_t {
    B = 0,
    Y = 1,
    Select = 2,
    Start = 3,
    Up = 4,
    Down = 5,
    Left = 6,
    Right = 7,
    A = 8,
    X = 9,
    L = 10,
    R = 11
};

enum class GamepadControl : uint8_t {
    South,
    East,
    West,
    North,
    Back,
    Start,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    LeftShoulder,
    RightShoulder,
    Count
};

enum class StickAxis : uint8_t { X, Y };

// Converts standardized game-controller controls into the SNES 12-button
// serial-controller bit layout. SDL2 and Android input adapters can feed this
// class without coupling emulator code to a platform API.
class ControllerMapper {
public:
    static constexpr std::size_t PLAYER_COUNT = 2;
    static constexpr int16_t DEFAULT_DEADZONE = 8000;

    ControllerMapper();
    void reset();
    bool setMapping(std::size_t player, GamepadControl control, SnesButton target);
    SnesButton mapping(std::size_t player, GamepadControl control) const;
    void setButton(std::size_t player, GamepadControl control, bool pressed);
    void setLeftStick(std::size_t player, int16_t x, int16_t y);
    void setDeadzone(int16_t deadzone) noexcept;
    uint16_t state(std::size_t player) const noexcept;

private:
    struct PlayerState {
        std::array<SnesButton, static_cast<std::size_t>(GamepadControl::Count)> mappings{};
        std::array<bool, static_cast<std::size_t>(GamepadControl::Count)> pressed{};
        int16_t leftX = 0;
        int16_t leftY = 0;
    };

    std::array<PlayerState, PLAYER_COUNT> players{};
    int16_t deadzone = DEFAULT_DEADZONE;
    static bool validPlayer(std::size_t player) noexcept;
    static uint16_t buttonMask(SnesButton button) noexcept;
};
