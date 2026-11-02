#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "cartridge.hpp"
#include "ppu.hpp"
#include "joypad.hpp"

class Bus {
public:
    static constexpr uint32_t SIZE = 1u << 24;
    static constexpr std::size_t WRAM_SIZE = 128u * 1024u;

    Bus();
    uint8_t read(uint32_t address);
    void write(uint32_t address, uint8_t value);
    bool loadCartridge(const std::string& path,
                       CartridgeMapping mapping = CartridgeMapping::Auto);
    void unloadCartridge();
    bool cartridgeLoaded() const;
    std::size_t cartridgeSize() const;
    std::size_t cartridgeSramSize() const;
    bool saveCartridgeSram() const;
    void setControllerState(std::size_t player, uint16_t buttons) noexcept;
    uint16_t controllerState(std::size_t player) const noexcept;
    const std::string& cartridgeMapping() const;
    PPU& ppu();
    const PPU& ppu() const;

private:
    static bool isWramMirror(uint32_t address);
    static std::size_t wramOffset(uint32_t address);
    std::vector<uint8_t> memory;
    std::vector<uint8_t> wram;
    Cartridge cartridge;
    PPU graphics;
    SnesJoypad joypad;
};
