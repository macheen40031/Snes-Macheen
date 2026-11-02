#pragma once

#include "bus.hpp"
#include "cpu.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// Owns one cartridge, CPU and PPU-facing bus for a frontend session.
// This is an integration layer, not yet a cycle-accurate SNES scheduler.
class EmulatorSession {
public:
    enum class State { Empty, Ready, Running, Halted, Faulted };

    bool loadRom(const std::string& path,
                 CartridgeMapping mapping = CartridgeMapping::Auto);
    void unload() noexcept;
    std::size_t runInstructions(std::size_t instructionBudget);
    std::size_t runFrame(std::size_t instructionBudget = 2000);
    bool saveBatteryRam() const;
    void setControllerState(std::size_t player, uint16_t buttons) noexcept;

    bool loaded() const noexcept;
    State state() const noexcept;
    const std::string& status() const noexcept;
    const std::string& romPath() const noexcept;
    uint16_t programCounter() const noexcept;
    uint64_t cycles() const noexcept;
    bool cpuStopped() const noexcept;
    const std::array<uint32_t, PPU::FRAME_PIXELS>& framebufferArgb8888();
    const Bus& bus() const noexcept;

private:
    Bus memoryBus;
    CPU processor;
    State currentState = State::Empty;
    std::string currentStatus = "No cartridge loaded.";
    std::string currentRomPath;
    std::array<uint32_t, PPU::FRAME_PIXELS> framePixels{};
};
