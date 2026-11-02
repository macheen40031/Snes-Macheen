#pragma once

#include <array>
#include <cstdint>

// Early, deliberately bounded PPU foundation. This models key CPU-visible
// VRAM/CGRAM/OAM ports and diagnostic Mode 0, 2bpp BG1-BG4 pixel fetches.
class PPU {
public:
    static constexpr std::size_t VRAM_BYTES = 64u * 1024u;
    static constexpr std::size_t CGRAM_BYTES = 512u;
    static constexpr std::size_t OAM_BYTES = 544u;
    static constexpr std::size_t SCREEN_WIDTH = 256u;
    static constexpr std::size_t SCREEN_HEIGHT = 224u;
    static constexpr std::size_t FRAME_PIXELS = SCREEN_WIDTH * SCREEN_HEIGHT;

    PPU();
    void reset();
    uint8_t readRegister(uint16_t address);
    void writeRegister(uint16_t address, uint8_t value);

    // Returns a 15-bit CGRAM color. Zero represents transparency or a disabled layer.
    uint16_t renderBg1Pixel(uint16_t x, uint16_t y) const;
    uint16_t renderBg2Pixel(uint16_t x, uint16_t y) const;
    uint16_t renderBg3Pixel(uint16_t x, uint16_t y) const;
    uint16_t renderBg4Pixel(uint16_t x, uint16_t y) const;
    // OBJ diagnostic fetch with OBSEL size/base selection and multi-tile objects.
    uint16_t renderObjPixel(uint16_t x, uint16_t y) const;
    uint16_t applyColorMath(uint16_t mainColor, uint16_t mathColor) const;
    uint16_t fixedColorValue() const;
    // Diagnostic RGB15 framebuffer; layers are composited BG4 > BG3 > BG2 > BG1.
    std::array<uint16_t, FRAME_PIXELS> renderFrame() const;
    // ARGB8888 framebuffer for SDL2/Android presentation layers.
    std::array<uint32_t, FRAME_PIXELS> renderFrameArgb8888() const;

    uint8_t vramByte(std::size_t address) const;
    uint8_t cgramByte(std::size_t address) const;
    uint8_t oamByte(std::size_t address) const;

private:
    struct PixelSample {
        uint16_t color = 0;
        bool highPriority = false;
        bool opaque = false;
    };
    struct ObjSample {
        uint16_t color = 0;
        uint8_t priority = 0;
        uint8_t index = 0xFFu;
        bool valid = false;
    };

    std::array<uint8_t, VRAM_BYTES> vram{};
    std::array<uint8_t, CGRAM_BYTES> cgram{};
    std::array<uint8_t, OAM_BYTES> oam{};
    std::array<uint8_t, 0x40> registers{};
    uint16_t vramAddress = 0;
    uint16_t oamAddress = 0;
    uint8_t cgramAddress = 0;
    bool cgramHighByte = false;
    uint16_t fixedColor = 0;
    uint8_t oamWriteLatch = 0;
    bool oamByteToggle = false;
    std::array<uint16_t, 4> bgScrollX{};
    std::array<uint16_t, 4> bgScrollY{};
    std::array<uint8_t, 8> scrollWriteLatch{};
    std::array<bool, 8> scrollHighWrite{};

    uint16_t vramIncrement() const;
    void incrementVramAddress();
    uint16_t readVramWord(uint16_t wordAddress) const;
    uint16_t readCgramColor(uint8_t index) const;
    PixelSample renderBackgroundSample(uint8_t background, uint16_t x, uint16_t y) const;
    uint16_t renderBackgroundPixel(uint8_t background, uint16_t x, uint16_t y) const;
    ObjSample renderObjSample(uint16_t x, uint16_t y) const;
};
