#include "ppu.hpp"

PPU::PPU() { reset(); }

void PPU::reset() {
    vram.fill(0);
    cgram.fill(0);
    oam.fill(0);
    registers.fill(0);
    vramAddress = 0;
    oamAddress = 0;
    cgramAddress = 0;
    cgramHighByte = false;
    fixedColor = 0;
    oamWriteLatch = 0;
    oamByteToggle = false;
    bgScrollX.fill(0);
    bgScrollY.fill(0);
    scrollWriteLatch.fill(0);
    scrollHighWrite.fill(false);
}

uint16_t PPU::vramIncrement() const {
    switch (registers[0x15] & 0x03) {
        case 1: return 32;
        case 2:
        case 3: return 128;
        default: return 1;
    }
}

void PPU::incrementVramAddress() { vramAddress = static_cast<uint16_t>(vramAddress + vramIncrement()); }

uint16_t PPU::readVramWord(uint16_t wordAddress) const {
    const std::size_t byteAddress = (static_cast<std::size_t>(wordAddress) * 2u) & (VRAM_BYTES - 1u);
    return static_cast<uint16_t>(vram[byteAddress] |
        (static_cast<uint16_t>(vram[(byteAddress + 1u) & (VRAM_BYTES - 1u)]) << 8));
}

uint16_t PPU::readCgramColor(uint8_t index) const {
    const std::size_t offset = static_cast<std::size_t>(index) * 2u;
    return static_cast<uint16_t>(cgram[offset] |
        (static_cast<uint16_t>(cgram[offset + 1u] & 0x7Fu) << 8));
}

uint8_t PPU::readRegister(uint16_t address) {
    const uint8_t reg = static_cast<uint8_t>(address & 0x3Fu);
    switch (reg) {
        case 0x39: {
            const uint8_t value = vram[(static_cast<std::size_t>(vramAddress) * 2u) & 0xFFFFu];
            if ((registers[0x15] & 0x80u) == 0) incrementVramAddress();
            return value;
        }
        case 0x3A: {
            const uint8_t value = vram[((static_cast<std::size_t>(vramAddress) * 2u) + 1u) & 0xFFFFu];
            if ((registers[0x15] & 0x80u) != 0) incrementVramAddress();
            return value;
        }
        case 0x3B: {
            const std::size_t offset = static_cast<std::size_t>(cgramAddress) * 2u;
            const uint8_t value = cgram[offset + (cgramHighByte ? 1u : 0u)];
            if (cgramHighByte) ++cgramAddress;
            cgramHighByte = !cgramHighByte;
            return value;
        }
        case 0x38: {
            // OAMADDR is a 9-bit word address. Its upper region addresses
            // the 32-byte high table after the 512-byte low table.
            const std::size_t word = static_cast<std::size_t>(oamAddress) % (OAM_BYTES / 2u);
            const std::size_t byteAddress = word < 0x100u ?
                word * 2u + (oamByteToggle ? 1u : 0u) :
                512u + (word - 0x100u) * 2u + (oamByteToggle ? 1u : 0u);
            const uint8_t value = oam[byteAddress];
            oamByteToggle = !oamByteToggle;
            if (!oamByteToggle) {
                oamAddress = static_cast<uint16_t>((oamAddress + 1u) % (OAM_BYTES / 2u));
            }
            return value;
        }
        default: return registers[reg];
    }
}

void PPU::writeRegister(uint16_t address, uint8_t value) {
    const uint8_t reg = static_cast<uint8_t>(address & 0x3Fu);
    registers[reg] = value;
    switch (reg) {
        case 0x02:
            oamAddress = static_cast<uint16_t>((oamAddress & 0x0100u) | value);
            oamByteToggle = false;
            break;
        case 0x03:
            oamAddress = static_cast<uint16_t>(((value & 1u) << 8) | (oamAddress & 0x00FFu));
            oamByteToggle = false;
            break;
        case 0x04: {
            oamWriteLatch = value;
            const std::size_t word = static_cast<std::size_t>(oamAddress) % (OAM_BYTES / 2u);
            const std::size_t byteAddress = word < 0x100u ?
                word * 2u + (oamByteToggle ? 1u : 0u) :
                512u + (word - 0x100u) * 2u + (oamByteToggle ? 1u : 0u);
            oam[byteAddress] = oamWriteLatch;
            oamByteToggle = !oamByteToggle;
            if (!oamByteToggle) {
                oamAddress = static_cast<uint16_t>((oamAddress + 1u) % (OAM_BYTES / 2u));
            }
            break;
        }
        case 0x16: vramAddress = static_cast<uint16_t>((vramAddress & 0xFF00u) | value); break;
        case 0x17: vramAddress = static_cast<uint16_t>((vramAddress & 0x00FFu) | (static_cast<uint16_t>(value) << 8)); break;
        case 0x18:
            vram[(static_cast<std::size_t>(vramAddress) * 2u) & 0xFFFFu] = value;
            if ((registers[0x15] & 0x80u) == 0) incrementVramAddress();
            break;
        case 0x19:
            vram[((static_cast<std::size_t>(vramAddress) * 2u) + 1u) & 0xFFFFu] = value;
            if ((registers[0x15] & 0x80u) != 0) incrementVramAddress();
            break;
        case 0x0D: case 0x0F: case 0x11: case 0x13:
        case 0x0E: case 0x10: case 0x12: case 0x14: {
            // Simplified two-write 10-bit BG scroll register handling.
            // Each register has its own latch to keep software tests deterministic.
            const uint8_t slot = static_cast<uint8_t>(reg - 0x0Du);
            const uint8_t bg = static_cast<uint8_t>(slot / 2u);
            const bool vertical = (slot & 1u) != 0;
            uint16_t& scroll = vertical ? bgScrollY[bg] : bgScrollX[bg];
            if (!scrollHighWrite[slot]) {
                scrollWriteLatch[slot] = value;
                scroll = static_cast<uint16_t>((scroll & 0x0300u) | value);
            } else {
                scroll = static_cast<uint16_t>((scrollWriteLatch[slot]) |
                    ((static_cast<uint16_t>(value & 0x03u)) << 8));
            }
            scrollHighWrite[slot] = !scrollHighWrite[slot];
            break;
        }
        case 0x21: cgramAddress = value; cgramHighByte = false; break;
        case 0x32:
            // COLDATA ($2132): bit 5 writes red, bit 6 green, bit 7 blue.
            if ((value & 0x20u) != 0) fixedColor = static_cast<uint16_t>((fixedColor & 0x7FE0u) | (value & 0x1Fu));
            if ((value & 0x40u) != 0) fixedColor = static_cast<uint16_t>((fixedColor & 0x7C1Fu) | ((value & 0x1Fu) << 5));
            if ((value & 0x80u) != 0) fixedColor = static_cast<uint16_t>((fixedColor & 0x03FFu) | ((value & 0x1Fu) << 10));
            break;
        case 0x22: {
            const std::size_t offset = static_cast<std::size_t>(cgramAddress) * 2u;
            if (!cgramHighByte) cgram[offset] = value;
            else { cgram[offset + 1u] = static_cast<uint8_t>(value & 0x7Fu); ++cgramAddress; }
            cgramHighByte = !cgramHighByte;
            break;
        }
        default: break;
    }
}

PPU::PixelSample PPU::renderBackgroundSample(uint8_t background, uint16_t x, uint16_t y) const {
    if (background >= 4u || (registers[0x00] & 0x80u) != 0 ||
        (registers[0x05] & 0x07u) != 0) return {};
    const uint8_t screenMask = static_cast<uint8_t>(1u << background);
    if ((registers[0x2C] & screenMask) == 0) return {};

    const uint8_t modeRegister = registers[0x05];
    const uint16_t tileSize = (modeRegister & static_cast<uint8_t>(0x10u << background)) ? 16u : 8u;
    const uint8_t mapRegister = static_cast<uint8_t>(0x07u + background);
    const uint8_t mapSize = static_cast<uint8_t>(registers[mapRegister] & 0x03u);
    const uint16_t mapWidthEntries = (mapSize & 0x01u) ? 64u : 32u;
    const uint16_t mapHeightEntries = (mapSize & 0x02u) ? 64u : 32u;
    const uint16_t pixelWidth = static_cast<uint16_t>(mapWidthEntries * tileSize);
    const uint16_t pixelHeight = static_cast<uint16_t>(mapHeightEntries * tileSize);
    const uint16_t sourceX = static_cast<uint16_t>((x + bgScrollX[background]) % pixelWidth);
    const uint16_t sourceY = static_cast<uint16_t>((y + bgScrollY[background]) % pixelHeight);
    const uint16_t tileX = static_cast<uint16_t>(sourceX / tileSize);
    const uint16_t tileY = static_cast<uint16_t>(sourceY / tileSize);
    const uint16_t blocksWide = static_cast<uint16_t>(mapWidthEntries / 32u);
    const uint16_t blockX = static_cast<uint16_t>(tileX / 32u);
    const uint16_t blockY = static_cast<uint16_t>(tileY / 32u);
    const uint16_t blockOffset = static_cast<uint16_t>((blockY * blocksWide + blockX) * 1024u);
    const uint16_t mapOffset = static_cast<uint16_t>(blockOffset +
        (tileY % 32u) * 32u + (tileX % 32u));
    const uint16_t mapBase = static_cast<uint16_t>((registers[mapRegister] & 0xFCu) << 8);
    const uint16_t entry = readVramWord(static_cast<uint16_t>(mapBase + mapOffset));
    const uint16_t baseTile = static_cast<uint16_t>(entry & 0x03FFu);
    const uint8_t palette = static_cast<uint8_t>((entry >> 10) & 0x07u);
    uint8_t pixelX = static_cast<uint8_t>(sourceX % tileSize);
    uint8_t pixelY = static_cast<uint8_t>(sourceY % tileSize);
    if ((entry & 0x4000u) != 0) pixelX = static_cast<uint8_t>(tileSize - 1u - pixelX);
    if ((entry & 0x8000u) != 0) pixelY = static_cast<uint8_t>(tileSize - 1u - pixelY);
    const uint16_t subTileX = static_cast<uint16_t>(pixelX / 8u);
    const uint16_t subTileY = static_cast<uint16_t>(pixelY / 8u);
    const uint16_t tile = static_cast<uint16_t>((baseTile + subTileY * 16u + subTileX) & 0x03FFu);
    const uint8_t column = static_cast<uint8_t>(pixelX & 7u);
    const uint8_t row = static_cast<uint8_t>(pixelY & 7u);
    const uint8_t baseRegister = (background < 2u) ?
        static_cast<uint8_t>((registers[0x0B] >> (background * 4u)) & 0x0Fu) :
        static_cast<uint8_t>((registers[0x0C] >> ((background - 2u) * 4u)) & 0x0Fu);
    const uint16_t charBase = static_cast<uint16_t>(baseRegister << 12);
    const uint16_t tileBase = static_cast<uint16_t>(charBase + tile * 16u);
    const uint8_t bit = static_cast<uint8_t>(7u - column);
    const uint16_t p0Word = readVramWord(static_cast<uint16_t>(tileBase + row / 2u));
    const uint16_t p1Word = readVramWord(static_cast<uint16_t>(tileBase + 8u + row / 2u));
    const uint8_t byteShift = static_cast<uint8_t>((row & 1u) * 8u);
    const uint8_t p0 = static_cast<uint8_t>(p0Word >> byteShift);
    const uint8_t p1 = static_cast<uint8_t>(p1Word >> byteShift);
    const uint8_t color = static_cast<uint8_t>(((p0 >> bit) & 1u) | (((p1 >> bit) & 1u) << 1));
    if (color == 0) return {};
    const uint8_t paletteIndex = static_cast<uint8_t>((background * 32u) + palette * 4u + color);
    return {readCgramColor(paletteIndex), (entry & 0x2000u) != 0, true};
}


uint16_t PPU::renderBackgroundPixel(uint8_t background, uint16_t x, uint16_t y) const {
    return renderBackgroundSample(background, x, y).color;
}

PPU::ObjSample PPU::renderObjSample(uint16_t x, uint16_t y) const {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT ||
        (registers[0x00] & 0x80u) != 0 || (registers[0x2C] & 0x10u) == 0) {
        return {};
    }

    struct ObjectSize {
        uint8_t smallWidth;
        uint8_t smallHeight;
        uint8_t largeWidth;
        uint8_t largeHeight;
    };
    // OBSEL object-size pairs. Modes 6 and 7 are documented by SNESdev
    // references as less commonly used rectangular combinations.
    static constexpr std::array<ObjectSize, 8> sizePairs{{
        {8, 8, 16, 16},
        {8, 8, 32, 32},
        {8, 8, 64, 64},
        {16, 16, 32, 32},
        {16, 16, 64, 64},
        {32, 32, 64, 64},
        {16, 32, 32, 64},
        {16, 32, 32, 32}
    }};

    const uint8_t objectSelect = registers[0x01]; // OBSEL ($2101)
    const uint8_t sizeMode = static_cast<uint8_t>((objectSelect >> 5) & 0x07u);
    const std::size_t objBase = static_cast<std::size_t>(objectSelect & 0x07u) * 0x4000u;
    const std::size_t nameSelectOffset =
        (static_cast<std::size_t>((objectSelect >> 3) & 0x03u) + 1u) * 0x2000u;

    ObjSample best{};
    for (int index = 0; index < 128; ++index) {
        const std::size_t base = static_cast<std::size_t>(index) * 4u;
        const uint8_t xLow = oam[base];
        const uint8_t yTop = oam[base + 1u];
        const uint8_t tileNumber = oam[base + 2u];
        const uint8_t attributes = oam[base + 3u];
        const uint8_t high = oam[512u + static_cast<std::size_t>(index / 4)];
        const uint8_t pairShift = static_cast<uint8_t>((index & 3) * 2);
        const bool xHigh = ((high >> pairShift) & 1u) != 0;
        const bool large = ((high >> (pairShift + 1u)) & 1u) != 0;
        const int rawX = static_cast<int>(xLow) | (xHigh ? 0x100 : 0);
        const int spriteX = rawX >= 256 ? rawX - 512 : rawX;
        const int spriteY = static_cast<int>(yTop);
        const uint8_t width = large ? sizePairs[sizeMode].largeWidth : sizePairs[sizeMode].smallWidth;
        const uint8_t height = large ? sizePairs[sizeMode].largeHeight : sizePairs[sizeMode].smallHeight;

        int localX = static_cast<int>(x) - spriteX;
        int localY = static_cast<int>(y) - spriteY;
        if (localX < 0 || localX >= width || localY < 0 || localY >= height) continue;
        if ((attributes & 0x40u) != 0) localX = static_cast<int>(width) - 1 - localX;
        if ((attributes & 0x80u) != 0) localY = static_cast<int>(height) - 1 - localY;

        // OBJ character data is arranged as a 16x16 grid of 8x8 tiles.
        // Horizontal/vertical tile-number increments wrap within each nibble.
        const uint8_t tileColumn = static_cast<uint8_t>(
            ((tileNumber & 0x0Fu) + static_cast<unsigned>(localX / 8)) & 0x0Fu);
        const uint8_t tileRow = static_cast<uint8_t>(
            (((tileNumber >> 4) & 0x0Fu) + static_cast<unsigned>(localY / 8)) & 0x0Fu);
        const uint8_t selectedTile = static_cast<uint8_t>((tileRow << 4) | tileColumn);
        const bool nameTable = (attributes & 0x01u) != 0;
        const std::size_t nameBase = objBase + (nameTable ? nameSelectOffset : 0u);
        const std::size_t tileBase =
            (nameBase + static_cast<std::size_t>(selectedTile) * 32u) & (VRAM_BYTES - 1u);
        const std::size_t row = static_cast<std::size_t>(localY & 7);
        const uint8_t bit = static_cast<uint8_t>(7 - (localX & 7));
        uint8_t color = 0;
        for (unsigned plane = 0; plane < 4; ++plane) {
            const std::size_t planeOffset = (plane < 2 ? 0u : 16u) +
                (plane & 1u) * 8u + row;
            const uint8_t byte = vram[(tileBase + planeOffset) & (VRAM_BYTES - 1u)];
            color |= static_cast<uint8_t>(((byte >> bit) & 1u) << plane);
        }
        if (color == 0) continue;
        const uint8_t palette = static_cast<uint8_t>((attributes >> 1) & 0x07u);
        const uint8_t paletteIndex = static_cast<uint8_t>(128u + palette * 16u + color);
        const uint8_t priority = static_cast<uint8_t>((attributes >> 4) & 0x03u);
        if (!best.valid || priority > best.priority ||
            (priority == best.priority && static_cast<uint8_t>(index) < best.index)) {
            best.color = readCgramColor(paletteIndex);
            best.priority = priority;
            best.index = static_cast<uint8_t>(index);
            // Pixel index zero is transparent; a nonzero palette entry may
            // legitimately contain RGB15 black and must remain opaque.
            best.valid = true;
        }
    }
    return best;
}

uint16_t PPU::renderObjPixel(uint16_t x, uint16_t y) const {
    return renderObjSample(x, y).color;
}

uint16_t PPU::renderBg1Pixel(uint16_t x, uint16_t y) const {
    return renderBackgroundPixel(0, x, y);
}

uint16_t PPU::renderBg2Pixel(uint16_t x, uint16_t y) const {
    return renderBackgroundPixel(1, x, y);
}

uint16_t PPU::renderBg3Pixel(uint16_t x, uint16_t y) const {
    return renderBackgroundPixel(2, x, y);
}

uint16_t PPU::renderBg4Pixel(uint16_t x, uint16_t y) const {
    return renderBackgroundPixel(3, x, y);
}

uint16_t PPU::applyColorMath(uint16_t mainColor, uint16_t mathColor) const {
    // Diagnostic color math controlled by CGADSUB ($2131): bit 7 subtract,
    // bit 6 half result. Component arithmetic saturates to RGB15 range.
    const bool subtract = (registers[0x31] & 0x80u) != 0;
    const bool half = (registers[0x31] & 0x40u) != 0;
    uint16_t result = 0;
    for (unsigned shift : {0u, 5u, 10u}) {
        const int a = static_cast<int>((mainColor >> shift) & 0x1Fu);
        const int b = static_cast<int>((mathColor >> shift) & 0x1Fu);
        int component = subtract ? a - b : a + b;
        component = subtract ? (component < 0 ? 0 : component) : (component > 31 ? 31 : component);
        if (half) component /= 2;
        result |= static_cast<uint16_t>(component << shift);
    }
    return result;
}

uint16_t PPU::fixedColorValue() const { return fixedColor; }

std::array<uint16_t, PPU::FRAME_PIXELS> PPU::renderFrame() const {
    std::array<uint16_t, FRAME_PIXELS> frame{};
    // Mode 0 front-to-back priority ranks from the SNES PPU priority chart:
    // OBJ3, BG1H, BG2H, OBJ2, BG1L, BG2L, OBJ1, BG3H, BG4H,
    // OBJ0, BG3L, BG4L, backdrop.
    static constexpr std::array<uint8_t, 4> bgLowRank{{8, 7, 2, 1}};
    static constexpr std::array<uint8_t, 4> bgHighRank{{11, 10, 5, 4}};
    static constexpr std::array<uint8_t, 4> objRank{{3, 6, 9, 12}};

    for (std::size_t y = 0; y < SCREEN_HEIGHT; ++y) {
        for (std::size_t x = 0; x < SCREEN_WIDTH; ++x) {
            const uint16_t px = static_cast<uint16_t>(x);
            const uint16_t py = static_cast<uint16_t>(y);
            uint16_t mainColor = 0;
            uint8_t bestRank = 0;
            uint8_t selectedLayer = 0xFFu; // 0..3 BG1..BG4, 4 OBJ, FF backdrop.

            for (uint8_t background = 0; background < 4u; ++background) {
                const PixelSample sample = renderBackgroundSample(background, px, py);
                if (!sample.opaque) continue;
                const uint8_t rank = sample.highPriority ?
                    bgHighRank[background] : bgLowRank[background];
                if (rank > bestRank) {
                    bestRank = rank;
                    mainColor = sample.color;
                    selectedLayer = background;
                }
            }

            const ObjSample object = renderObjSample(px, py);
            if (object.valid) {
                const uint8_t rank = objRank[object.priority & 0x03u];
                if (rank > bestRank) {
                    bestRank = rank;
                    mainColor = object.color;
                    selectedLayer = 4u;
                }
            }

            bool mathSelected = false;
            if (selectedLayer <= 4u) {
                mathSelected = (registers[0x31] &
                    static_cast<uint8_t>(1u << selectedLayer)) != 0;
            }
            uint16_t outputColor = (mathSelected && mainColor != 0) ?
                applyColorMath(mainColor, fixedColor) : mainColor;

            // INIDISP ($2100) brightness is a 4-bit global multiplier.
            // Forced blank is handled by the layer samplers and produces black.
            const uint8_t brightness = static_cast<uint8_t>(registers[0x00] & 0x0Fu);
            if ((registers[0x00] & 0x80u) != 0 || brightness == 0u) {
                outputColor = 0;
            } else if (brightness < 15u) {
                uint16_t scaled = 0;
                for (unsigned shift : {0u, 5u, 10u}) {
                    const uint16_t component = static_cast<uint16_t>((outputColor >> shift) & 0x1Fu);
                    const uint16_t adjusted = static_cast<uint16_t>((component * brightness) / 15u);
                    scaled |= static_cast<uint16_t>(adjusted << shift);
                }
                outputColor = scaled;
            }
            frame[y * SCREEN_WIDTH + x] = outputColor;
        }
    }
    return frame;
}


std::array<uint32_t, PPU::FRAME_PIXELS> PPU::renderFrameArgb8888() const {
    const auto rgb15 = renderFrame();
    std::array<uint32_t, FRAME_PIXELS> output{};
    for (std::size_t i = 0; i < FRAME_PIXELS; ++i) {
        const uint8_t red5 = static_cast<uint8_t>(rgb15[i] & 0x1Fu);
        const uint8_t green5 = static_cast<uint8_t>((rgb15[i] >> 5) & 0x1Fu);
        const uint8_t blue5 = static_cast<uint8_t>((rgb15[i] >> 10) & 0x1Fu);
        const uint8_t red8 = static_cast<uint8_t>((red5 << 3) | (red5 >> 2));
        const uint8_t green8 = static_cast<uint8_t>((green5 << 3) | (green5 >> 2));
        const uint8_t blue8 = static_cast<uint8_t>((blue5 << 3) | (blue5 >> 2));
        output[i] = 0xFF000000u | (static_cast<uint32_t>(red8) << 16) |
                    (static_cast<uint32_t>(green8) << 8) | blue8;
    }
    return output;
}

uint8_t PPU::vramByte(std::size_t address) const { return vram[address % VRAM_BYTES]; }
uint8_t PPU::cgramByte(std::size_t address) const { return cgram[address % CGRAM_BYTES]; }
uint8_t PPU::oamByte(std::size_t address) const { return oam[address % OAM_BYTES]; }
