#include "cartridge.hpp"

#include <fstream>
#include <iterator>
#include <filesystem>

namespace {
constexpr uint32_t kAddressMask = 0xFFFFFF;
}

bool Cartridge::load(const std::string& path, CartridgeMapping requested) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    const std::istreambuf_iterator<char> begin(input);
    const std::istreambuf_iterator<char> end;
    std::vector<uint8_t> candidate(begin, end);
    if (candidate.empty()) return false;

    CartridgeMapping selected = requested;
    if (selected == CartridgeMapping::Auto) {
        const uint8_t loMode = candidate.size() > 0x7FD5 ? candidate[0x7FD5] : 0;
        const uint8_t hiMode = candidate.size() > 0xFFD5 ? candidate[0xFFD5] : 0;
        const bool hiLooksValid = (hiMode & 0x2F) == 0x21;
        const bool loLooksValid = (loMode & 0x2F) == 0x20;
        selected = hiLooksValid && !loLooksValid
            ? CartridgeMapping::HiROM : CartridgeMapping::LoROM;
    }

    // The cartridge header stores SRAM size as an exponent in 1 KiB units.
    // A zero value means the cartridge declares no SRAM. Keep allocation
    // bounded so malformed images cannot request unreasonable amounts.
    const std::size_t headerOffset =
        selected == CartridgeMapping::HiROM ? 0xFFD8u : 0x7FD8u;
    const uint8_t sramSizeCode = candidate.size() > headerOffset
        ? candidate[headerOffset] : 0;
    const std::size_t boundedCode = sramSizeCode > 13 ? 13 : sramSizeCode;
    std::vector<uint8_t> candidateSram;
    if (sramSizeCode != 0)
        candidateSram.resize(std::size_t{1} << (boundedCode + 10), 0xFF);

    rom = std::move(candidate);
    sram = std::move(candidateSram);
    mapping = selected;
    mappingLabel = mapping == CartridgeMapping::HiROM ? "HiROM" : "LoROM";
    sourcePath = path;

    // Restore a previous battery-backed save when its sidecar exists.
    // Unwritten bytes retain the hardware-like erased value of 0xFF.
    if (!sram.empty()) {
        std::ifstream save(sourcePath + ".srm", std::ios::binary);
        if (save) {
            save.read(reinterpret_cast<char*>(sram.data()),
                      static_cast<std::streamsize>(sram.size()));
        }
    }
    return true;
}

void Cartridge::unload() {
    rom.clear();
    sram.clear();
    mapping = CartridgeMapping::Auto;
    mappingLabel = "None";
    sourcePath.clear();
}

bool Cartridge::loaded() const { return !rom.empty(); }

bool Cartridge::maps(uint32_t address) const {
    if (rom.empty()) return false;
    address &= kAddressMask;
    const uint8_t bank = static_cast<uint8_t>(address >> 16);
    const uint16_t offset = static_cast<uint16_t>(address);
    if (bank == 0x7E || bank == 0x7F) return false;
    if (mapping == CartridgeMapping::LoROM) return offset >= 0x8000;
    if ((bank >= 0x40 && bank <= 0x7D) || bank >= 0xC0) return true;
    return ((bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF)) && offset >= 0x8000;
}

uint8_t Cartridge::read(uint32_t address) const {
    if (!maps(address)) return 0xFF;
    address &= kAddressMask;
    const uint8_t bank = static_cast<uint8_t>(address >> 16);
    const uint16_t offset = static_cast<uint16_t>(address);
    std::size_t romOffset = 0;
    if (mapping == CartridgeMapping::LoROM)
        romOffset = (static_cast<std::size_t>(bank & 0x7F) << 15) | (offset & 0x7FFF);
    else
        romOffset = (static_cast<std::size_t>(bank & 0x3F) << 16) | offset;
    return romOffset < rom.size() ? rom[romOffset] : 0xFF;
}

bool Cartridge::mapsSram(uint32_t address) const {
    if (sram.empty()) return false;
    address &= kAddressMask;
    const uint8_t bank = static_cast<uint8_t>(address >> 16);
    const uint16_t offset = static_cast<uint16_t>(address);

    if (mapping == CartridgeMapping::LoROM) {
        const bool sramBank = (bank >= 0x70 && bank <= 0x7D) || bank >= 0xF0;
        return sramBank && offset < 0x8000;
    }

    // HiROM SRAM is visible in the $6000-$7FFF window of banks
    // $20-$3F and their $A0-$BF mirrors.
    const bool sramBank = (bank >= 0x20 && bank <= 0x3F) ||
                          (bank >= 0xA0 && bank <= 0xBF);
    return sramBank && offset >= 0x6000 && offset < 0x8000;
}

uint8_t Cartridge::readSram(uint32_t address) const {
    if (!mapsSram(address)) return 0xFF;
    address &= kAddressMask;
    const uint8_t bank = static_cast<uint8_t>(address >> 16);
    const uint16_t offset = static_cast<uint16_t>(address);
    std::size_t index = 0;
    if (mapping == CartridgeMapping::LoROM) {
        index = ((static_cast<std::size_t>(bank & 0x0F) << 15) | offset) % sram.size();
    } else {
        index = ((static_cast<std::size_t>(bank & 0x1F) << 13) |
                 (offset - 0x6000)) % sram.size();
    }
    return sram[index];
}

void Cartridge::writeSram(uint32_t address, uint8_t value) {
    if (!mapsSram(address)) return;
    address &= kAddressMask;
    const uint8_t bank = static_cast<uint8_t>(address >> 16);
    const uint16_t offset = static_cast<uint16_t>(address);
    std::size_t index = 0;
    if (mapping == CartridgeMapping::LoROM) {
        index = ((static_cast<std::size_t>(bank & 0x0F) << 15) | offset) % sram.size();
    } else {
        index = ((static_cast<std::size_t>(bank & 0x1F) << 13) |
                 (offset - 0x6000)) % sram.size();
    }
    sram[index] = value;
}

bool Cartridge::saveSram() const {
    if (sram.empty() || sourcePath.empty()) return false;

    const std::filesystem::path destination(sourcePath + ".srm");
    const std::filesystem::path temporary(sourcePath + ".srm.tmp");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(reinterpret_cast<const char*>(sram.data()),
                     static_cast<std::streamsize>(sram.size()));
        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (!error) return true;

    // Some platforms do not replace an existing destination on rename.
    std::error_code ignored;
    std::filesystem::remove(destination, ignored);
    error.clear();
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

std::size_t Cartridge::sramSize() const { return sram.size(); }
std::size_t Cartridge::size() const { return rom.size(); }
const std::string& Cartridge::mappingName() const { return mappingLabel; }
const std::string& Cartridge::fileName() const { return sourcePath; }
