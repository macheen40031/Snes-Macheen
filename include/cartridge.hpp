#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class CartridgeMapping {
    Auto,
    LoROM,
    HiROM
};

class Cartridge {
public:
    bool load(const std::string& path, CartridgeMapping requested = CartridgeMapping::Auto);
    void unload();

    bool loaded() const;
    bool maps(uint32_t address) const;
    uint8_t read(uint32_t address) const;
    bool mapsSram(uint32_t address) const;
    uint8_t readSram(uint32_t address) const;
    void writeSram(uint32_t address, uint8_t value);
    bool saveSram() const;
    std::size_t sramSize() const;
    std::size_t size() const;
    const std::string& mappingName() const;
    const std::string& fileName() const;

private:
    std::vector<uint8_t> rom;
    std::vector<uint8_t> sram;
    CartridgeMapping mapping = CartridgeMapping::Auto;
    std::string mappingLabel = "None";
    std::string sourcePath;
};
