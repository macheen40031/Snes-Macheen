#include "joypad.hpp"

SnesJoypad::SnesJoypad() { reset(); }

void SnesJoypad::reset() noexcept {
    liveState.fill(0);
    // An unconnected/fully shifted controller reads as logic-high.
    shiftState.fill(0xFFFFu);
    strobe = false;
}

void SnesJoypad::setState(std::size_t player, uint16_t buttons) noexcept {
    if (player >= PLAYER_COUNT) return;
    liveState[player] = static_cast<uint16_t>(buttons & BUTTON_MASK);
}

uint16_t SnesJoypad::state(std::size_t player) const noexcept {
    return player < PLAYER_COUNT ? liveState[player] : 0u;
}

void SnesJoypad::latch() noexcept {
    for (std::size_t player = 0; player < PLAYER_COUNT; ++player) {
        // The twelve SNES buttons are followed by four unused logic-high bits.
        shiftState[player] = static_cast<uint16_t>(liveState[player] | 0xF000u);
    }
}

void SnesJoypad::writeStrobe(uint8_t value) noexcept {
    const bool next = (value & 0x01u) != 0;
    // A high strobe continuously exposes the live B button. The falling edge
    // captures a stable 12-button snapshot for serial shifting.
    if (next || strobe) latch();
    strobe = next;
}

uint8_t SnesJoypad::readPort(std::size_t player) noexcept {
    if (player >= PLAYER_COUNT) return 0x01u;
    if (strobe) return static_cast<uint8_t>(liveState[player] & 0x01u);

    const uint8_t result = static_cast<uint8_t>(shiftState[player] & 0x01u);
    // Hardware shifts in ones; after the twelve SNES buttons and four
    // trailing bits have been read, subsequent reads remain high.
    shiftState[player] = static_cast<uint16_t>(
        (shiftState[player] >> 1u) | 0x8000u);
    return result;
}

bool SnesJoypad::strobeEnabled() const noexcept { return strobe; }
