#pragma once

#include <array>
#include <cstdint>

// SNES two-port serial joypad interface exposed at $4016/$4017.
// Button bit order: B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R.
class SnesJoypad {
public:
    static constexpr std::size_t PLAYER_COUNT = 2;
    static constexpr uint16_t BUTTON_MASK = 0x0FFFu;

    SnesJoypad();
    void reset() noexcept;
    void setState(std::size_t player, uint16_t buttons) noexcept;
    uint16_t state(std::size_t player) const noexcept;
    void writeStrobe(uint8_t value) noexcept;
    uint8_t readPort(std::size_t player) noexcept;
    bool strobeEnabled() const noexcept;

private:
    std::array<uint16_t, PLAYER_COUNT> liveState{};
    std::array<uint16_t, PLAYER_COUNT> shiftState{};
    bool strobe = false;
    void latch() noexcept;
};
