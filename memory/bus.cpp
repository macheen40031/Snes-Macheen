#include "bus.hpp"

Bus::Bus() : memory(SIZE, 0), wram(WRAM_SIZE, 0) {}

bool Bus::isWramMirror(uint32_t address) {
    address &= 0xFFFFFF;
    const uint8_t bank = static_cast<uint8_t>(address >> 16);
    const uint16_t offset = static_cast<uint16_t>(address);
    return bank == 0x7E || bank == 0x7F ||
        (((bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF)) && offset < 0x2000);
}

std::size_t Bus::wramOffset(uint32_t address) {
    address &= 0xFFFFFF;
    const uint8_t bank = static_cast<uint8_t>(address >> 16);
    const uint16_t offset = static_cast<uint16_t>(address);
    if (bank == 0x7F) return 0x10000u + offset;
    if (bank == 0x7E) return offset;
    return offset & 0x1FFF;
}

uint8_t Bus::read(uint32_t address) {
    address &= 0xFFFFFF;
    if (isWramMirror(address)) return wram[wramOffset(address)];
    const uint8_t bank = static_cast<uint8_t>(address >> 16);
    const uint16_t offset = static_cast<uint16_t>(address);
    const bool ioBank = (bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF);
    if (ioBank && offset == 0x4016) return joypad.readPort(0);
    if (ioBank && offset == 0x4017) return joypad.readPort(1);
    if (ioBank && offset >= 0x2100 && offset <= 0x213F)
        return graphics.readRegister(offset);
    if (cartridge.mapsSram(address)) return cartridge.readSram(address);
    if (cartridge.maps(address)) return cartridge.read(address);
    return memory[address];
}

void Bus::write(uint32_t address, uint8_t value) {
    address &= 0xFFFFFF;
    if (isWramMirror(address)) {
        wram[wramOffset(address)] = value;
        return;
    }
    const uint8_t bank = static_cast<uint8_t>(address >> 16);
    const uint16_t offset = static_cast<uint16_t>(address);
    const bool ioBank = (bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF);
    if (ioBank && offset == 0x4016) {
        joypad.writeStrobe(value);
        return;
    }
    if (ioBank && offset >= 0x2100 && offset <= 0x213F) {
        graphics.writeRegister(offset, value);
        return;
    }
    if (cartridge.mapsSram(address)) {
        cartridge.writeSram(address, value);
        return;
    }
    if (cartridge.maps(address)) return;
    memory[address] = value;
}

bool Bus::loadCartridge(const std::string& path, CartridgeMapping mapping) {
    return cartridge.load(path, mapping);
}
void Bus::unloadCartridge() { cartridge.unload(); }
bool Bus::cartridgeLoaded() const { return cartridge.loaded(); }
std::size_t Bus::cartridgeSize() const { return cartridge.size(); }
std::size_t Bus::cartridgeSramSize() const { return cartridge.sramSize(); }
bool Bus::saveCartridgeSram() const { return cartridge.saveSram(); }
void Bus::setControllerState(std::size_t player, uint16_t buttons) noexcept {
    joypad.setState(player, buttons);
}
uint16_t Bus::controllerState(std::size_t player) const noexcept {
    return joypad.state(player);
}
const std::string& Bus::cartridgeMapping() const { return cartridge.mappingName(); }

PPU& Bus::ppu() { return graphics; }
const PPU& Bus::ppu() const { return graphics; }
