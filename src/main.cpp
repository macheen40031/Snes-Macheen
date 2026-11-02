#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "cpu.hpp"
#include "bus.hpp"
#include "frontend_model.hpp"
#include "controller_mapper.hpp"
#include "emulator_session.hpp"
#include "frame_pacer.hpp"

int checks = 0;

void check(bool condition, const char* description) {
    ++checks;

    std::cout << (condition ? "[PASS] " : "[FAIL] ")
              << description << "\n";

    if (!condition)
        std::exit(1);
}

void loadProgram(
    Bus& bus,
    const uint8_t* program,
    size_t length
) {
    bus.write(0x00FFFC, 0x00);
    bus.write(0x00FFFD, 0x80);

    for (size_t i = 0; i < length; ++i)
        bus.write(0x008000 + i, program[i]);
}

void runSteps(CPU& cpu, Bus& bus, int count) {
    for (int i = 0; i < count; ++i) {
        if (!cpu.step(bus)) {
            std::cerr << "Unexpected CPU stop.\n";
            std::exit(1);
        }
    }
}

void runToStop(CPU& cpu, Bus& bus) {
    for (int i = 0; i < 100 && !cpu.stopped; ++i) {
        if (!cpu.step(bus))
            break;
    }

    check(cpu.stopped, "CPU reached STP instruction");
}

int main() {
    std::cout << "\n====================================\n";
    std::cout << "       SNES-MACHEEN TEST SUITE\n";
    std::cout << "====================================\n";

    // --------------------------------------
    // TEST 1: 8-bit arithmetic and logic
    // --------------------------------------

    {
        Bus bus;
        CPU cpu;

        const uint8_t program[] = {
            0xA9, 0x7F,       // LDA #$7F
            0x18,             // CLC
            0x69, 0x01,       // ADC #$01
            0x38,             // SEC
            0xE9, 0x01,       // SBC #$01
            0x29, 0x0F,       // AND #$0F
            0x09, 0x80,       // ORA #$80
            0x49, 0xFF,       // EOR #$FF
            0xC9, 0x70,       // CMP #$70
            0xE0, 0x00,       // CPX #$00
            0xC0, 0x00,       // CPY #$00
            0xE8,             // INX
            0xC8,             // INY
            0xCA,             // DEX
            0x88,             // DEY
            0xAA,             // TAX
            0xA8,             // TAY
            0x8A,             // TXA
            0x98,             // TYA
            0xDB              // STP
        };

        loadProgram(bus, program, sizeof(program));
        cpu.reset(bus);

        std::cout << "\n--- 8-BIT ALU TEST ---\n";

        runSteps(cpu, bus, 3);

        check(cpu.A == 0x80,
              "8-bit ADC result");

        check((cpu.P & 0xC1) == 0xC0,
              "ADC Negative and Overflow flags");

        runSteps(cpu, bus, 2);

        check(cpu.A == 0x7F,
              "8-bit SBC result");

        check((cpu.P & 0xC1) == 0x41,
              "SBC Carry and Overflow flags");

        // Execute comparisons first, then inspect their flags.
        runSteps(cpu, bus, 6);

        check(cpu.A == 0x70,
              "AND, ORA and EOR results");

        check((cpu.P & 0x03) == 0x03,
              "CMP, CPX and CPY Zero/Carry flags");

        // Index operations and transfers modify Zero/Negative.
        runSteps(cpu, bus, 8);

        check(cpu.X == 0x70,
              "INX, DEX and TAX/TXA operations");

        check(cpu.Y == 0x70,
              "INY, DEY and TAY/TYA operations");

        runToStop(cpu, bus);

        check(cpu.PC == 0x801D,
              "8-bit instruction sequence length");

        check(cpu.cycles == 41,
              "8-bit test cycle count");
    }

    // --------------------------------------
    // TEST 2: 16-bit arithmetic
    // --------------------------------------

    {
        Bus bus;
        CPU cpu;

        const uint8_t program[] = {
            0xFB,             // XCE
            0xC2, 0x20,       // REP #$20
            0xA9, 0xFF, 0x7F, // LDA #$7FFF
            0x18,             // CLC
            0x69, 0x01, 0x00, // ADC #$0001
            0x38,             // SEC
            0xE9, 0x01, 0x00, // SBC #$0001
            0xDB              // STP
        };

        loadProgram(bus, program, sizeof(program));
        cpu.reset(bus);

        std::cout << "\n--- 16-BIT ALU TEST ---\n";

        runSteps(cpu, bus, 5);

        check(!cpu.emulation,
              "Native processor mode");

        check(!cpu.accumulator8Bit(),
              "16-bit accumulator mode");

        check(cpu.A == 0x8000,
              "16-bit ADC result");

        check((cpu.P & 0xC1) == 0xC0,
              "16-bit ADC Negative and Overflow flags");

        runSteps(cpu, bus, 2);

        check(cpu.A == 0x7FFF,
              "16-bit SBC result");

        check((cpu.P & 0xC1) == 0x41,
              "16-bit SBC Carry and Overflow flags");

        runToStop(cpu, bus);

        check(cpu.PC == 0x800F,
              "16-bit instruction sequence length");

        check(cpu.cycles == 21,
              "16-bit test cycle count");
    }

    // --------------------------------------
    // TEST 3: Decimal / BCD arithmetic
    // --------------------------------------

    {
        Bus bus;
        CPU cpu;

        const uint8_t program[] = {
            0xA9, 0x45, // LDA #$45
            0xF8,       // SED
            0x18,       // CLC
            0x69, 0x55, // ADC #$55
            0xE9, 0x01, // SBC #$01
            0xD8,       // CLD
            0xDB        // STP
        };

        loadProgram(bus, program, sizeof(program));
        cpu.reset(bus);

        std::cout << "\n--- DECIMAL BCD TEST ---\n";

        runSteps(cpu, bus, 4);

        check(cpu.A == 0x00,
              "BCD 45 + 55 equals 00");

        check((cpu.P & 0x03) == 0x03,
              "BCD addition carry and zero");

        runSteps(cpu, bus, 1);

        check(cpu.A == 0x99,
              "BCD 00 - 01 equals 99");

        check((cpu.P & 0x81) == 0x80,
              "BCD subtraction borrow and negative");

        runToStop(cpu, bus);

        check((cpu.P & 0x08) == 0,
              "CLD clears Decimal flag");

        check(cpu.PC == 0x800A,
              "BCD instruction sequence length");

        check(cpu.cycles == 15,
              "BCD test cycle count");
    }

    // --------------------------------------
    // TEST 4: Memory addressing modes
    // --------------------------------------

    {
        Bus bus;
        CPU cpu;

        const uint8_t program[] = {
            0xA9, 0x42,       // LDA #$42
            0x85, 0x0A,       // STA $0A (direct page)
            0xA9, 0x00,       // LDA #$00
            0xA5, 0x0A,       // LDA $0A (direct page)
            0xA9, 0x5A,       // LDA #$5A
            0x9D, 0x00, 0x30, // STA $3000,X
            0xA9, 0x00,       // LDA #$00
            0xBD, 0x00, 0x30, // LDA $3000,X
            0xA9, 0xA5,       // LDA #$A5
            0x99, 0x00, 0x31, // STA $3100,Y
            0xA9, 0x00,       // LDA #$00
            0xB9, 0x00, 0x31, // LDA $3100,Y
            0xDB              // STP
        };

        loadProgram(bus, program, sizeof(program));
        cpu.reset(bus);
        cpu.D = 0x0200;
        cpu.X = 0x0003;
        cpu.Y = 0x0004;

        std::cout << "\n--- MEMORY ADDRESSING MODE TEST ---\n";

        runToStop(cpu, bus);

        check(bus.read(0x00020A) == 0x42,
              "STA direct-page stores at D + offset");

        check(bus.read(0x003003) == 0x5A,
              "STA absolute,X stores at indexed address");

        check(bus.read(0x003104) == 0xA5,
              "STA absolute,Y stores at indexed address");

        check(cpu.A == 0xA5,
              "LDA direct-page, absolute,X and absolute,Y load correctly");

        check(cpu.PC == 0x801D,
              "Addressing-mode instruction sequence length");

        check(cpu.cycles == 39,
              "Addressing-mode test cycle count");
    }

    // --------------------------------------
    // TEST 5: Indirect and long addressing
    // --------------------------------------

    {
        Bus bus;
        CPU cpu;

        const uint8_t program[] = {
            0xA1, 0x04,       // LDA ($04,X) -> pointer at D+$06 -> $4000
            0x91, 0x0A,       // STA ($0A),Y -> $4100 + Y
            0xB1, 0x0A,       // LDA ($0A),Y
            0xA7, 0x0E,       // LDA [$0E] -> $02:4200
            0x87, 0x0E,       // STA [$0E]
            0xB7, 0x12,       // LDA [$12],Y -> $03:4300 + Y
            0x97, 0x12,       // STA [$12],Y
            0xDB              // STP
        };

        loadProgram(bus, program, sizeof(program));
        cpu.reset(bus);
        cpu.D = 0x0200;
        cpu.X = 0x0002;
        cpu.Y = 0x0003;

        // (dp,X) pointer -> bank $00:$4000
        bus.write(0x0206, 0x00);
        bus.write(0x0207, 0x40);
        bus.write(0x004000, 0x5A);

        // (dp),Y pointer -> bank $00:$4100
        bus.write(0x020A, 0x00);
        bus.write(0x020B, 0x41);

        // [dp] long pointer -> bank $02:$4200
        bus.write(0x020E, 0x00);
        bus.write(0x020F, 0x42);
        bus.write(0x0210, 0x02);
        bus.write(0x024200, 0xA6);

        // [dp],Y long pointer -> bank $03:$4300
        bus.write(0x0212, 0x00);
        bus.write(0x0213, 0x43);
        bus.write(0x0214, 0x03);
        bus.write(0x034303, 0xC7);

        std::cout << "\n--- INDIRECT AND LONG ADDRESSING TEST ---\n";
        runSteps(cpu, bus, 3);
        check(cpu.A == 0x5A,
              "LDA (direct-page,X) and (direct-page),Y load correctly");
        check(bus.read(0x004103) == 0x5A,
              "STA (direct-page),Y stores through 16-bit pointer");

        runToStop(cpu, bus);
        check(bus.read(0x024200) == 0xA6,
              "STA [direct-page] stores through 24-bit pointer");
        check(bus.read(0x034303) == 0xC7,
              "STA [direct-page],Y stores through indexed 24-bit pointer");
        check(cpu.A == 0xC7,
              "LDA [direct-page],Y loads through indexed 24-bit pointer");
        check(cpu.PC == 0x800F,
              "Indirect-addressing instruction sequence length");
    }

    // --------------------------------------
    // TEST 6: Conditional branch
    // --------------------------------------

    {
        Bus bus;
        CPU cpu;
        const uint8_t program[] = {
            0xA9, 0x00,       // LDA #$00 (sets Z)
            0xD0, 0x02,       // BNE +2 (not taken)
            0xF0, 0x02,       // BEQ +2 (taken; skips LDA #$11)
            0xA9, 0x11,       // skipped
            0xA9, 0x77,       // LDA #$77
            0x80, 0x00,       // BRA +0
            0xDB              // STP
        };
        loadProgram(bus, program, sizeof(program));
        cpu.reset(bus);
        std::cout << "\n--- BRANCH TEST ---\n";
        runToStop(cpu, bus);
        check(cpu.A == 0x77, "BEQ branch skips the unwanted instruction");
        check(cpu.PC == 0x800D, "Branch sequence reaches expected PC");
    }

    // --------------------------------------
    // TEST 7: Stack operations and subroutines
    // --------------------------------------

    {
        Bus bus;
        CPU cpu;
        const uint8_t program[] = {
            0xA9, 0x42,       // LDA #$42
            0x48,             // PHA
            0xA9, 0x00,       // LDA #$00
            0x68,             // PLA -> $42
            0x38,             // SEC
            0x08,             // PHP
            0x18,             // CLC
            0x28,             // PLP -> restores carry
            0x20, 0x10, 0x80, // JSR $8010
            0xDB,             // STP
            0xEA, 0xEA,       // padding at $800E-$800F
            0xA9, 0x77,       // subroutine at $8010: LDA #$77
            0x60              // RTS
        };
        loadProgram(bus, program, sizeof(program));
        cpu.reset(bus);
        std::cout << "\n--- STACK AND SUBROUTINE TEST ---\n";
        runSteps(cpu, bus, 8);
        check(cpu.A == 0x42, "PHA and PLA preserve accumulator value");
        check((cpu.P & 0x01) != 0, "PHP and PLP restore processor status");
        check(cpu.S == 0x01FF, "Stack pointer restored after balanced pushes and pulls");
        runToStop(cpu, bus);
        check(cpu.A == 0x77, "JSR executes subroutine instructions");
        check(cpu.PC == 0x800E, "RTS returns to STP after JSR");
        check(cpu.S == 0x01FF, "JSR and RTS restore stack pointer");
    }

    // --------------------------------------
    // TEST 8: SNES WRAM mirrors and cartridge loading
    // --------------------------------------

    {
        Bus bus;
        std::cout << "\n--- SNES MEMORY MAP AND CARTRIDGE TEST ---\n";

        bus.write(0x7E0001, 0x5A);
        check(bus.read(0x000001) == 0x5A,
              "WRAM bank $7E mirrors into banks $00-$3F");
        bus.write(0x000002, 0xA5);
        check(bus.read(0x7E0002) == 0xA5,
              "Low-bank WRAM mirror writes reach bank $7E");
        bus.write(0x7F0003, 0xC3);
        check(bus.read(0x7F0003) == 0xC3,
              "Bank $7F accesses the upper 64 KiB of WRAM");

        const auto temp = std::filesystem::temp_directory_path();
        const auto loPath = temp / "snes_macheen_test_lorom.sfc";
        const auto hiPath = temp / "snes_macheen_test_hirom.sfc";

        // Synthetic 32 KiB LoROM: reset vector points to $008000.
        std::vector<uint8_t> loRom(0x8000, 0xEA);
        loRom[0x0000] = 0xA9; // LDA #$5C
        loRom[0x0001] = 0x5C;
        loRom[0x0002] = 0xDB; // STP
        loRom[0x7FD5] = 0x20; // LoROM map mode
        loRom[0x7FFC] = 0x00;
        loRom[0x7FFD] = 0x80;
        {
            std::ofstream out(loPath, std::ios::binary);
            out.write(reinterpret_cast<const char*>(loRom.data()),
                      static_cast<std::streamsize>(loRom.size()));
        }

        Bus loBus;
        check(loBus.loadCartridge(loPath.string()),
              "LoROM cartridge image loads from a file");
        check(loBus.cartridgeLoaded() && loBus.cartridgeMapping() == "LoROM",
              "Cartridge loader identifies LoROM mapping");
        check(loBus.read(0x008000) == 0xA9 && loBus.read(0x00FFFC) == 0x00,
              "LoROM maps program bytes and reset vector");
        loBus.write(0x008000, 0x00);
        check(loBus.read(0x008000) == 0xA9,
              "Writes to cartridge ROM are ignored");

        CPU cartCpu;
        cartCpu.reset(loBus);
        runToStop(cartCpu, loBus);
        check(cartCpu.A == 0x5C && cartCpu.PC == 0x8003,
              "CPU executes a program directly from a LoROM cartridge");

        // Synthetic 64 KiB HiROM: low-bank upper-half maps ROM offset $8000.
        std::vector<uint8_t> hiRom(0x10000, 0xEA);
        hiRom[0x8000] = 0xA9;
        hiRom[0x8001] = 0x6D;
        hiRom[0x8002] = 0xDB;
        hiRom[0xFFD5] = 0x21; // HiROM map mode
        hiRom[0xFFD8] = 1;    // 2 KiB SRAM
        hiRom[0xFFFC] = 0x00;
        hiRom[0xFFFD] = 0x80;
        {
            std::ofstream out(hiPath, std::ios::binary);
            out.write(reinterpret_cast<const char*>(hiRom.data()),
                      static_cast<std::streamsize>(hiRom.size()));
        }

        Bus hiBus;
        check(hiBus.loadCartridge(hiPath.string()),
              "HiROM cartridge image loads from a file");
        check(hiBus.cartridgeMapping() == "HiROM" && hiBus.read(0x008000) == 0xA9,
              "Cartridge loader identifies and maps HiROM");
        check(hiBus.cartridgeSramSize() == 2048,
              "HiROM header determines 2 KiB SRAM capacity");
        hiBus.write(0x206005, 0x3C);
        check(hiBus.read(0xA06005) == 0x3C,
              "HiROM SRAM reads and mirrors across low/high banks");
        check(hiBus.saveCartridgeSram(),
              "HiROM battery-backed SRAM saves to a sidecar file");
        Bus restoredHiBus;
        check(restoredHiBus.loadCartridge(hiPath.string()) &&
              restoredHiBus.read(0x206005) == 0x3C,
              "HiROM SRAM automatically restores from its save file");

        // Synthetic LoROM with 2 KiB SRAM declared in its header.
        const auto sramPath = temp / "snes_macheen_test_sram.sfc";
        std::vector<uint8_t> sramRom(0x8000, 0xEA);
        sramRom[0x7FD5] = 0x20;
        sramRom[0x7FD8] = 1; // 2 KiB SRAM (2^(1+10))
        sramRom[0x7FFC] = 0x00;
        sramRom[0x7FFD] = 0x80;
        {
            std::ofstream out(sramPath, std::ios::binary);
            out.write(reinterpret_cast<const char*>(sramRom.data()),
                      static_cast<std::streamsize>(sramRom.size()));
        }
        Bus sramBus;
        check(sramBus.loadCartridge(sramPath.string()),
              "LoROM with declared SRAM loads");
        check(sramBus.cartridgeSramSize() == 2048,
              "Cartridge header determines 2 KiB SRAM capacity");
        sramBus.write(0x700123, 0x6B);
        check(sramBus.read(0x700123) == 0x6B,
              "LoROM SRAM bank accepts writes and reads");
        check(sramBus.read(0xF00123) == 0x6B,
              "LoROM SRAM mirrors across high banks and small capacities");
        check(sramBus.read(0x008000) == 0xEA,
              "SRAM mapping does not interfere with cartridge ROM");
        check(sramBus.saveCartridgeSram(),
              "LoROM battery-backed SRAM saves to a sidecar file");
        Bus restoredLoBus;
        check(restoredLoBus.loadCartridge(sramPath.string()) &&
              restoredLoBus.read(0x700123) == 0x6B,
              "LoROM SRAM automatically restores from its save file");

        std::error_code ignored;
        std::filesystem::remove(loPath, ignored);
        std::filesystem::remove(hiPath, ignored);
        std::filesystem::remove(sramPath, ignored);
        std::filesystem::remove(hiPath.string() + ".srm", ignored);
        std::filesystem::remove(sramPath.string() + ".srm", ignored);
        std::filesystem::remove(hiPath.string() + ".srm.tmp", ignored);
        std::filesystem::remove(sramPath.string() + ".srm.tmp", ignored);
    }

    // --------------------------------------
    // TEST 9: PPU registers, VRAM, CGRAM, OAM, and BG1 pixel fetch
    // --------------------------------------

    {
        Bus bus;
        std::cout << "\n--- PPU FOUNDATION TEST ---\n";

        // Configure VRAM to increment after the high data byte.
        bus.write(0x002115, 0x80);
        bus.write(0x002116, 0x00);
        bus.write(0x002117, 0x00);
        bus.write(0x002118, 0x00); // BG1 tilemap entry: tile 0, palette 0
        bus.write(0x002119, 0x00);

        // Place color-1 pixels in the first two rows of tile 0.
        bus.write(0x002116, 0x00);
        bus.write(0x002117, 0x10);
        bus.write(0x002118, 0x80); // plane 0, row 0, leftmost pixel
        bus.write(0x002119, 0x80); // plane 0, row 1, leftmost pixel

        // CGRAM color 1 = $001F (15-bit red).
        bus.write(0x002121, 0x01);
        bus.write(0x002122, 0x1F);
        bus.write(0x002122, 0x00);

        // Mode 0, BG1 character base $1000, BG1 tilemap at $0000.
        bus.write(0x002105, 0x00);
        bus.write(0x002107, 0x00);
        bus.write(0x00210B, 0x01);
        bus.write(0x00212C, 0x01); // enable BG1 on main screen
        bus.write(0x002100, 0x0F); // display enabled, full brightness

        check(bus.read(0x802105) == 0x00,
              "PPU registers mirror into high banks");
        check(bus.ppu().vramByte(0x2000) == 0x80,
              "VRAM data port writes the selected VRAM word");
        check(bus.ppu().cgramByte(2) == 0x1F && bus.ppu().cgramByte(3) == 0x00,
              "CGRAM port stores 15-bit palette colors");
        check(bus.ppu().renderBg1Pixel(0, 0) == 0x001F,
              "Mode 0 BG1 renderer fetches a tile pixel and palette color");
        check(bus.ppu().renderBg1Pixel(0, 1) == 0x001F,
              "BG1 renderer selects the correct byte for odd tile rows");
        check(bus.ppu().renderBg1Pixel(1, 0) == 0,
              "Transparent BG1 tile pixels return color zero");

        // Configure a separate BG2 tilemap and character base.
        // BG2 map at VRAM word $4000; BG2 character data at word $2000.
        bus.write(0x002116, 0x00);
        bus.write(0x002117, 0x40);
        bus.write(0x002118, 0x00);
        bus.write(0x002119, 0x00);
        bus.write(0x002116, 0x00);
        bus.write(0x002117, 0x20);
        bus.write(0x002118, 0x80);
        bus.write(0x002119, 0x00);
        // BG2 palette 8, color 1 = $03E0 (15-bit green).
        bus.write(0x002121, 33);
        bus.write(0x002122, 0xE0);
        bus.write(0x002122, 0x03);
        bus.write(0x002108, 0x40); // BG2 tilemap base
        bus.write(0x00210B, 0x21); // BG1 base $1000, BG2 base $2000
        bus.write(0x00212C, 0x03); // enable BG1 and BG2
        check(bus.ppu().renderBg2Pixel(0, 0) == 0x03E0,
              "Mode 0 BG2 uses its own tilemap, character base and palette");
        bus.write(0x00210F, 0x01); // BG2 horizontal scroll low byte
        bus.write(0x00210F, 0x00); // BG2 horizontal scroll high bits
        check(bus.ppu().renderBg2Pixel(0, 0) == 0,
              "BG2 horizontal scrolling shifts tile pixels");
        check(bus.ppu().renderBg2Pixel(255, 0) == 0x03E0,
              "BG2 horizontal scrolling wraps at the 256-pixel tilemap edge");

        // BG3 and BG4 use the BG34 character-base register ($210C).
        // BG3 tilemap at word $6000, character data at byte $3000.
        bus.write(0x002116, 0x00);
        bus.write(0x002117, 0x60);
        bus.write(0x002118, 0x00);
        bus.write(0x002119, 0x00);
        bus.write(0x002116, 0x00);
        bus.write(0x002117, 0x30);
        bus.write(0x002118, 0x80);
        bus.write(0x002119, 0x00);
        bus.write(0x002121, 65); // BG3 palette 0, color 1 = blue
        bus.write(0x002122, 0x00);
        bus.write(0x002122, 0x7C);
        bus.write(0x002109, 0x60);
        bus.write(0x00210C, 0x03); // BG3 character base $3000
        bus.write(0x00212C, 0x07);
        check(bus.ppu().renderBg3Pixel(0, 0) == 0x7C00,
              "Mode 0 BG3 renders using its own map and character base");

        // BG4 tilemap at word $7000, character data at word $5000.
        bus.write(0x002116, 0x00);
        bus.write(0x002117, 0x70);
        bus.write(0x002118, 0x00);
        bus.write(0x002119, 0x00);
        bus.write(0x002116, 0x00);
        bus.write(0x002117, 0x50);
        bus.write(0x002118, 0x80);
        bus.write(0x002119, 0x00);
        bus.write(0x002121, 97); // BG4 palette 0, color 1 = yellow
        bus.write(0x002122, 0x1F);
        bus.write(0x002122, 0x03);
        bus.write(0x00210A, 0x70);
        bus.write(0x00210C, 0x53); // BG3 base $3000, BG4 base $5000
        bus.write(0x00212C, 0x0F);
        check(bus.ppu().renderBg4Pixel(0, 0) == 0x031F,
              "Mode 0 BG4 renders using its own map and character base");
        const auto fourLayerFrame = bus.ppu().renderFrame();
        check(fourLayerFrame[0] == 0x001F,
              "Mode 0 priority places BG1 low priority above lower BG layers");
        bus.write(0x00212C, 0x07);
        check(bus.ppu().renderBg4Pixel(0, 0) == 0,
              "Main-screen BG4 enable independently controls its pixels");

        bus.write(0x002100, 0x8F); // forced blank
        check(bus.ppu().renderBg1Pixel(0, 0) == 0,
              "Forced blank suppresses BG1 pixel output");
        bus.write(0x002100, 0x0F);
        bus.write(0x00212C, 0x00);
        check(bus.ppu().renderBg1Pixel(0, 0) == 0,
              "Main-screen BG1 enable controls pixel output");

        // OAM data port writes and the CPU-visible OAM read port.
        bus.write(0x002102, 0x05);
        bus.write(0x002103, 0x00);
        bus.write(0x002104, 0xA6);
        bus.write(0x002102, 0x05);
        bus.write(0x002103, 0x00);
        check(bus.read(0x002138) == 0xA6,
              "OAM data is readable through the OAMDATA port");

        // VRAM read ports are at $2139/$213A, not the write ports $2118/$2119.
        bus.write(0x002116, 0x00);
        bus.write(0x002117, 0x10);
        check(bus.read(0x002139) == 0x80,
              "VRAM low-byte read port returns stored data");
        check(bus.read(0x00213A) == 0x80,
              "VRAM high-byte read port returns stored data");

        // Re-enable BG1 after the earlier visibility-control test.
        bus.write(0x00212C, 0x03);
        const auto frame = bus.ppu().renderFrame();
        check(frame.size() == PPU::FRAME_PIXELS,
              "Framebuffer has the expected 256x224 pixel count");
        check(frame[0] == 0x001F,
              "Framebuffer renders the configured BG1 top-left pixel");
        check(frame[1] == 0x0000,
              "Framebuffer preserves transparent BG1 pixels");
        check(frame[PPU::SCREEN_WIDTH] == 0x001F,
              "Framebuffer renders the next tile row correctly");
        check(frame[255] == 0x03E0,
              "Framebuffer composites non-transparent BG2 pixels over BG1");
        check(frame == bus.ppu().renderFrame(),
              "Framebuffer rendering is deterministic");
    }

    // --------------------------------------
    // TEST 10: PPU 16x16 tiles and 64x32 tilemap layout
    // --------------------------------------
    {
        PPU ppu;
        std::cout << "\n--- PPU TILE SIZE AND TILEMAP DIMENSION TEST ---\n";
        auto writeVramWord = [&ppu](uint16_t address, uint16_t value) {
            ppu.writeRegister(0x16, static_cast<uint8_t>(address & 0xFFu));
            ppu.writeRegister(0x17, static_cast<uint8_t>(address >> 8));
            ppu.writeRegister(0x18, static_cast<uint8_t>(value & 0xFFu));
            ppu.writeRegister(0x19, static_cast<uint8_t>(value >> 8));
        };
        auto writeCgram = [&ppu](uint8_t index, uint16_t color) {
            ppu.writeRegister(0x21, index);
            ppu.writeRegister(0x22, static_cast<uint8_t>(color & 0xFFu));
            ppu.writeRegister(0x22, static_cast<uint8_t>(color >> 8));
        };

        // 16x16 BG1 tile: top-right quadrant is tile number 1.
        writeVramWord(0x0000, 0x0000); // map entry selects base tile 0
        writeVramWord(0x1010, 0x0080); // tile 1, plane 0, row 0, leftmost pixel
        writeCgram(1, 0x001F);
        ppu.writeRegister(0x05, 0x10); // BG1 16x16 tiles, Mode 0
        ppu.writeRegister(0x07, 0x00);
        ppu.writeRegister(0x0B, 0x01); // BG1 character base $1000 (word address)
        ppu.writeRegister(0x2C, 0x01);
        check(ppu.renderBg1Pixel(8, 0) == 0x001F,
              "16x16 BG1 tile fetch selects the top-right 8x8 subtile");

        // A 64x32 map places its right-hand 32x32 screen block at word +$400.
        PPU wideMap;
        auto writeWideWord = [&wideMap](uint16_t address, uint16_t value) {
            wideMap.writeRegister(0x16, static_cast<uint8_t>(address & 0xFFu));
            wideMap.writeRegister(0x17, static_cast<uint8_t>(address >> 8));
            wideMap.writeRegister(0x18, static_cast<uint8_t>(value & 0xFFu));
            wideMap.writeRegister(0x19, static_cast<uint8_t>(value >> 8));
        };
        wideMap.writeRegister(0x05, 0x00); // 8x8 tiles
        writeWideWord(0x0400, 0x0000);     // first entry in second horizontal screen block
        writeWideWord(0x1000, 0x0080);     // BG1 tile 0, plane 0 row 0 = leftmost pixel
        wideMap.writeRegister(0x21, 1);
        wideMap.writeRegister(0x22, 0x1F);
        wideMap.writeRegister(0x22, 0x00);
        wideMap.writeRegister(0x07, 0x01); // 64x32 tilemap
        wideMap.writeRegister(0x0B, 0x01);
        wideMap.writeRegister(0x2C, 0x01);
        wideMap.writeRegister(0x0D, 0x00); // BG1 scroll X low byte
        wideMap.writeRegister(0x0D, 0x01); // BG1 scroll X high bits = 1 => 256 pixels
        check(wideMap.renderBg1Pixel(0, 0) == 0x001F,
              "64x32 BG1 map fetches from its second horizontal screen block");
    }

    // --------------------------------------
    // TEST 11: Initial OBJ/sprite rendering
    // --------------------------------------
    {
        PPU ppu;
        ppu.writeRegister(0x00, 0x0F); // Full display brightness for framebuffer checks.
        std::cout << "\n--- PPU OBJ SPRITE FOUNDATION TEST ---\n";
        auto writeVramByte = [&ppu](uint16_t address, uint8_t value) {
            const uint16_t word = static_cast<uint16_t>(address / 2u);
            ppu.writeRegister(0x16, static_cast<uint8_t>(word & 0xFFu));
            ppu.writeRegister(0x17, static_cast<uint8_t>(word >> 8));
            ppu.writeRegister((address & 1u) ? 0x19 : 0x18, value);
        };
        auto writeCgram = [&ppu](uint8_t index, uint16_t color) {
            ppu.writeRegister(0x21, index);
            ppu.writeRegister(0x22, static_cast<uint8_t>(color & 0xFFu));
            ppu.writeRegister(0x22, static_cast<uint8_t>(color >> 8));
        };
        // Tile 0, plane 0, row 0: leftmost pixel has color index 1.
        writeVramByte(0, 0x80);
        writeCgram(129, 0x7C00); // OBJ palette 0, color 1 = blue.
        // OAM object 0 at (10,20), tile 0, palette 0, no flips.
        ppu.writeRegister(0x02, 0);
        ppu.writeRegister(0x03, 0);
        ppu.writeRegister(0x04, 10);
        ppu.writeRegister(0x04, 20);
        ppu.writeRegister(0x04, 0);
        ppu.writeRegister(0x04, 0);
        ppu.writeRegister(0x2C, 0x10); // Enable OBJ on main screen.
        check(ppu.renderObjPixel(10, 20) == 0x7C00,
              "OBJ renderer fetches a visible 4bpp sprite pixel");
        check(ppu.renderObjPixel(11, 20) == 0,
              "Transparent OBJ pixels do not produce color");
        const auto spriteFrame = ppu.renderFrame();
        check(spriteFrame[20u * PPU::SCREEN_WIDTH + 10u] == 0x7C00,
              "Framebuffer composites OBJ pixels over backgrounds");
        ppu.writeRegister(0x2C, 0x00);
        check(ppu.renderObjPixel(10, 20) == 0,
              "Main-screen OBJ enable independently controls sprite output");
    }

    // --------------------------------------
    // TEST 12: RGB15 color math arithmetic
    // --------------------------------------
    {
        PPU ppu;
        std::cout << "\n--- PPU COLOR MATH TEST ---\n";
        check(ppu.applyColorMath(0x001F, 0x0001) == 0x001F,
              "Color addition saturates at RGB15 channel maximum");
        ppu.writeRegister(0x31, 0x80); // Subtract.
        check(ppu.applyColorMath(0x03E0, 0x0020) == 0x03C0,
              "Color subtraction operates independently per RGB channel");
        ppu.writeRegister(0x31, 0x40); // Add and halve.
        check(ppu.applyColorMath(0x001F, 0x001F) == 0x000F,
              "Half color math halves the saturated channel result");
        ppu.writeRegister(0x31, 0xC0); // Subtract and halve.
        check(ppu.applyColorMath(0x0010, 0x0004) == 0x0006,
              "Half subtract color math clamps and halves the result");
        ppu.writeRegister(0x32, 0x7F); // Write red and green = 31.
        ppu.writeRegister(0x32, 0xBF); // Write red and blue = 31.
        check(ppu.fixedColorValue() == 0x7FFF,
              "COLDATA component-select writes update fixed RGB15 color");
    }

    // --------------------------------------
    // TEST 13: OBJ size pairs and character-table addressing
    // --------------------------------------
    {
        PPU ppu;
        std::cout << "\n--- PPU OBJ SIZE AND TILE-MAP TEST ---\n";
        auto writeVramByte = [&ppu](uint16_t address, uint8_t value) {
            const uint16_t word = static_cast<uint16_t>(address / 2u);
            ppu.writeRegister(0x16, static_cast<uint8_t>(word & 0xFFu));
            ppu.writeRegister(0x17, static_cast<uint8_t>(word >> 8));
            ppu.writeRegister((address & 1u) ? 0x19 : 0x18, value);
        };
        auto writeCgram = [&ppu](uint8_t index, uint16_t color) {
            ppu.writeRegister(0x21, index);
            ppu.writeRegister(0x22, static_cast<uint8_t>(color & 0xFFu));
            ppu.writeRegister(0x22, static_cast<uint8_t>(color >> 8));
        };
        auto writeOam = [&ppu](uint16_t address, const std::array<uint8_t, 4>& bytes) {
            const uint16_t wordAddress = static_cast<uint16_t>(address / 2u);
            ppu.writeRegister(0x02, static_cast<uint8_t>(wordAddress & 0xFFu));
            ppu.writeRegister(0x03, static_cast<uint8_t>((wordAddress >> 8) & 0x01u));
            for (uint8_t value : bytes) ppu.writeRegister(0x04, value);
        };

        // Four tiles forming a 16x16 object: 0, 1, $10, and $11.
        writeVramByte(0, 0x80);
        writeVramByte(32, 0x80);
        writeVramByte(0x200, 0x80);
        writeVramByte(0x220, 0x80);
        writeCgram(129, 0x7C00);
        writeOam(0, {10, 20, 0, 0});
        // OAM high-table bit 1 selects the large size; OBSEL mode 0 is 8x8/16x16.
        ppu.writeRegister(0x02, 0);
        ppu.writeRegister(0x03, 1);
        ppu.writeRegister(0x04, 0x02);
        ppu.writeRegister(0x01, 0x00);
        ppu.writeRegister(0x2C, 0x10);
        check(ppu.renderObjPixel(10, 20) == 0x7C00 &&
              ppu.renderObjPixel(18, 20) == 0x7C00 &&
              ppu.renderObjPixel(10, 28) == 0x7C00 &&
              ppu.renderObjPixel(18, 28) == 0x7C00,
              "16x16 OBJ fetches all four 8x8 character tiles");
        check(ppu.renderObjPixel(26, 20) == 0,
              "16x16 OBJ clips pixels outside its selected dimensions");

        // A separate horizontal-flip object verifies flipping across the full 16px width.
        writeOam(4, {40, 20, 0, 0x40});
        ppu.writeRegister(0x02, 0);
        ppu.writeRegister(0x03, 1);
        ppu.writeRegister(0x04, 0x02); // sprite 0 remains large
        ppu.writeRegister(0x04, 0x00); // sprite 1 remains small
        check(ppu.renderObjPixel(47, 20) == 0x7C00,
              "Horizontal flip is applied across the complete multi-tile OBJ");

        // OBSEL base=1 and name-select=1: N=1 selects base + 2*8 KiB.
        writeVramByte(0x8000, 0x80);
        writeOam(8, {70, 20, 0, 0x01});
        ppu.writeRegister(0x02, 0);
        ppu.writeRegister(0x03, 1);
        ppu.writeRegister(0x04, 0x00); // object 2 small
        ppu.writeRegister(0x01, 0x09); // base 1, name select 1, size pair 0
        check(ppu.renderObjPixel(70, 20) == 0x7C00,
              "OBSEL base and name-select address the second OBJ character table");

        // OBSEL mode 6: small objects are 16x32, including its lower tile rows.
        writeVramByte(0x4000 + 0xA07, 0x80); // tile $50, row seven
        writeOam(12, {90, 20, 0x20, 0});
        ppu.writeRegister(0x02, 0);
        ppu.writeRegister(0x03, 1);
        ppu.writeRegister(0x04, 0x00); // object 3 small
        ppu.writeRegister(0x01, 0xC1); // base 1, mode 6
        check(ppu.renderObjPixel(90, 51) == 0x7C00,
              "Rectangular OBSEL size mode supports a 16x32 small OBJ");

        // The 9-bit X coordinate is signed: $1FF places the left edge at -1.
        writeVramByte(0, 0xC0); // Make source columns 0 and 1 opaque.
        writeOam(16, {0xFF, 20, 0, 0}); // Sprite 4, X-low=$FF.
        ppu.writeRegister(0x02, 0);
        ppu.writeRegister(0x03, 1);
        ppu.writeRegister(0x04, 0x02); // Preserve sprite 0's large bit.
        ppu.writeRegister(0x04, 0x01); // Sprite 4 X-high bit.
        ppu.writeRegister(0x01, 0x00);
        check(ppu.renderObjPixel(0, 20) == 0x7C00,
              "9-bit OBJ X coordinate supports negative left-edge positions");
    }

    // --------------------------------------
    // TEST 14: Mode 0 BG/OBJ priority composition
    // --------------------------------------
    {
        PPU ppu;
        std::cout << "\n--- PPU MODE 0 PRIORITY TEST ---\n";
        auto writeVramByte = [&ppu](uint16_t address, uint8_t value) {
            const uint16_t word = static_cast<uint16_t>(address / 2u);
            ppu.writeRegister(0x16, static_cast<uint8_t>(word & 0xFFu));
            ppu.writeRegister(0x17, static_cast<uint8_t>(word >> 8));
            ppu.writeRegister((address & 1u) ? 0x19 : 0x18, value);
        };
        auto writeCgram = [&ppu](uint8_t index, uint16_t color) {
            ppu.writeRegister(0x21, index);
            ppu.writeRegister(0x22, static_cast<uint8_t>(color & 0xFFu));
            ppu.writeRegister(0x22, static_cast<uint8_t>(color >> 8));
        };
        auto writeOam = [&ppu](uint16_t byteAddress, const std::array<uint8_t, 4>& bytes) {
            const uint16_t wordAddress = static_cast<uint16_t>(byteAddress / 2u);
            ppu.writeRegister(0x02, static_cast<uint8_t>(wordAddress & 0xFFu));
            ppu.writeRegister(0x03, static_cast<uint8_t>((wordAddress >> 8) & 0x01u));
            for (uint8_t value : bytes) ppu.writeRegister(0x04, value);
        };

        // BG1 tile 0 is a high-priority red pixel at (0,0).
        writeVramByte(0, 0x00);
        writeVramByte(1, 0x20); // BG tilemap priority bit 13.
        writeVramByte(0x2000, 0x80);
        writeCgram(1, 0x001F);
        ppu.writeRegister(0x05, 0x00); // Mode 0.
        ppu.writeRegister(0x07, 0x00); // BG1 map base.
        ppu.writeRegister(0x0B, 0x01); // BG1 character base at byte $2000.

        // OBJ tile 0 is green, OBJ tile 1 is blue; OBSEL base 1 => byte $4000.
        writeVramByte(0x4000, 0x80);
        writeVramByte(0x4020, 0x80);
        writeCgram(129, 0x03E0);
        writeCgram(145, 0x7C00);
        writeOam(0, {0, 0, 0, 0x30}); // OBJ0, priority 3, palette 0.
        writeOam(4, {40, 20, 1, 0x32}); // OBJ1 is initially away from pixel (0,0).
        ppu.writeRegister(0x01, 0x01); // OBJ base 1, size pair 0.
        ppu.writeRegister(0x2C, 0x11); // BG1 + OBJ main screen.
        ppu.writeRegister(0x00, 0x0F);
        ppu.writeRegister(0x02, 0x00);
        ppu.writeRegister(0x03, 0x01);
        ppu.writeRegister(0x04, 0x00); // OAM high-table size flags: both small.
        check(ppu.renderFrame()[0] == 0x03E0,
              "OBJ priority 3 is in front of high-priority BG1 in Mode 0");
        writeCgram(129, 0x0000); // Opaque pixel index 1 can map to black.
        check(ppu.renderFrame()[0] == 0x0000,
              "Opaque black OBJ pixel still covers the background");
        writeCgram(129, 0x03E0);

        // OBJ priority 2 is behind high-priority BG1 but ahead of low-priority BG1.
        writeOam(0, {0, 0, 0, 0x20});
        check(ppu.renderFrame()[0] == 0x001F,
              "High-priority BG1 is in front of OBJ priority 2");
        writeVramByte(1, 0x00); // BG1 now low priority.
        check(ppu.renderFrame()[0] == 0x03E0,
              "OBJ priority 2 is in front of low-priority BG1");

        // Among two visible sprites in the same OBJ priority class, lower OAM index wins.
        writeVramByte(1, 0x20);
        writeOam(0, {0, 0, 0, 0x30});
        writeOam(4, {0, 0, 1, 0x32});
        check(ppu.renderObjPixel(0, 0) == 0x03E0,
              "Lower OAM index wins when overlapping OBJ pixels share priority");
    }

    // --------------------------------------
    // TEST 15: INIDISP brightness and forced blank
    // --------------------------------------
    {
        PPU ppu;
        std::cout << "\n--- PPU DISPLAY BRIGHTNESS TEST ---\n";
        auto writeVramByte = [&ppu](uint16_t address, uint8_t value) {
            const uint16_t word = static_cast<uint16_t>(address / 2u);
            ppu.writeRegister(0x16, static_cast<uint8_t>(word & 0xFFu));
            ppu.writeRegister(0x17, static_cast<uint8_t>(word >> 8));
            ppu.writeRegister((address & 1u) ? 0x19 : 0x18, value);
        };
        auto writeCgram = [&ppu](uint8_t index, uint16_t color) {
            ppu.writeRegister(0x21, index);
            ppu.writeRegister(0x22, static_cast<uint8_t>(color & 0xFFu));
            ppu.writeRegister(0x22, static_cast<uint8_t>(color >> 8));
        };
        writeVramByte(0, 0x80);
        writeCgram(1, 0x7FFF);
        ppu.writeRegister(0x05, 0x00);
        ppu.writeRegister(0x07, 0x08); // Keep tilemap away from tile character data.
        ppu.writeRegister(0x0B, 0x00);
        ppu.writeRegister(0x2C, 0x01);
        ppu.writeRegister(0x00, 0x0F);
        check(ppu.renderFrame()[0] == 0x7FFF,
              "INIDISP full brightness preserves RGB15 color");
        ppu.writeRegister(0x00, 0x08);
        check(ppu.renderFrame()[0] == 0x4210,
              "INIDISP half brightness scales all RGB channels");
        ppu.writeRegister(0x00, 0x00);
        check(ppu.renderFrame()[0] == 0x0000,
              "INIDISP zero brightness outputs black");
        ppu.writeRegister(0x00, 0x8F);
        check(ppu.renderFrame()[0] == 0x0000,
              "INIDISP forced blank outputs black at full brightness");
        ppu.writeRegister(0x00, 0x0F);
        const auto argb = ppu.renderFrameArgb8888();
        check(argb[0] == 0xFFFFFFFFu,
              "ARGB8888 framebuffer converts full-intensity RGB15 white");
        check((argb[1] >> 24) == 0xFFu,
              "ARGB8888 framebuffer supplies opaque alpha for every pixel");
        ppu.writeRegister(0x00, 0x08);
        check(ppu.renderFrameArgb8888()[0] == 0xFF848484u,
              "ARGB8888 framebuffer preserves PPU brightness scaling");
    }

    // --------------------------------------
    // TEST 16: Platform-neutral frontend model
    // --------------------------------------
    {
        std::cout << "\n--- FRONTEND NAVIGATION AND LIBRARY TEST ---\n";
        FrontendModel ui;
        check(ui.screen() == FrontendScreen::MainMenu,
              "Frontend starts on the main menu");
        check(ui.mainMenuItems().size() == 6,
              "Main menu exposes six planned sections");
        check(ui.selectedIndex() == 0,
              "Main menu initially selects Game Library");
        ui.handle(FrontendAction::Up);
        check(ui.selectedIndex() == 5,
              "Main menu selection wraps upward");
        ui.handle(FrontendAction::Down);
        check(ui.selectedIndex() == 0,
              "Main menu selection wraps downward");
        ui.handle(FrontendAction::Confirm);
        check(ui.screen() == FrontendScreen::GameLibrary,
              "Confirm opens Game Library");

        const auto tempRoot = std::filesystem::temp_directory_path() /
            "snes_macheen_frontend_test";
        std::error_code fsError;
        std::filesystem::remove_all(tempRoot, fsError);
        fsError.clear();
        std::filesystem::create_directories(tempRoot / "nested", fsError);
        check(!fsError, "Frontend test directory can be created");
        auto writeFixture = [](const std::filesystem::path& path, const char* contents) {
            std::ofstream output(path, std::ios::binary);
            output << contents;
        };
        writeFixture(tempRoot / "Zelda.sfc", "ROM-Z");
        writeFixture(tempRoot / "mario.SMC", "ROM-MARIO");
        writeFixture(tempRoot / "readme.txt", "not a ROM");
        writeFixture(tempRoot / "nested" / "hidden.sfc", "nested ROM");
        check(ui.scanGameDirectory(tempRoot),
              "Frontend scans a readable game directory");
        check(ui.games().size() == 2,
              "Library accepts supported ROM extensions and ignores other files");
        check(ui.games()[0].title == "mario" && ui.games()[1].title == "Zelda",
              "Game library sorts titles case-insensitively");
        check(ui.games()[0].fileSize == 9,
              "Game library records ROM file size");
        check(ui.selectGame(1) && ui.selectedGame() != nullptr &&
              ui.selectedGame()->title == "Zelda",
              "Game library selects a valid ROM entry");
        check(!ui.selectGame(99),
              "Game library rejects an out-of-range selection");
        ui.handle(FrontendAction::Back);
        check(ui.screen() == FrontendScreen::MainMenu,
              "Back returns from Game Library to main menu");
        check(!ui.scanGameDirectory(tempRoot / "missing"),
              "Frontend reports an invalid game directory");
        ui.enterPlaying("Zelda");
        check(ui.screen() == FrontendScreen::Playing,
              "Frontend can enter the Playing screen after a successful ROM load");
        check(ui.statusMessage().find("Zelda") != std::string::npos,
              "Playing screen status identifies the selected game");
        ui.handle(FrontendAction::Back);
        check(ui.screen() == FrontendScreen::GameLibrary,
              "Back from Playing returns to the game library");
        ui.setStatusMessage("ROM failed to load.");
        check(ui.statusMessage() == "ROM failed to load.",
              "Frontend can display session errors from the emulator layer");
        std::filesystem::remove_all(tempRoot, fsError);
    }

    // --------------------------------------
    // TEST 17: Standardized controller mapping
    // --------------------------------------
    {
        std::cout << "\n--- CONTROLLER MAPPING TEST ---\n";
        ControllerMapper mapper;
        const auto mask = [](SnesButton button) {
            return static_cast<uint16_t>(1u << static_cast<unsigned>(button));
        };
        mapper.setButton(0, GamepadControl::South, true);
        check(mapper.state(0) == mask(SnesButton::B),
              "Standard South face button maps to SNES B");
        mapper.setButton(0, GamepadControl::East, true);
        check(mapper.state(0) == static_cast<uint16_t>(mask(SnesButton::B) | mask(SnesButton::A)),
              "Two face buttons can be held simultaneously");
        mapper.setButton(0, GamepadControl::South, false);
        check(mapper.state(0) == mask(SnesButton::A),
              "Releasing one button preserves another held button");
        check(mapper.setMapping(0, GamepadControl::South, SnesButton::X),
              "Controller supports user button remapping");
        mapper.setButton(0, GamepadControl::South, true);
        check((mapper.state(0) & mask(SnesButton::X)) != 0,
              "Remapped face button emits its selected SNES bit");
        mapper.setButton(0, GamepadControl::South, false);
        mapper.setLeftStick(0, -20000, 0);
        check((mapper.state(0) & mask(SnesButton::Left)) != 0,
              "Left analog stick crosses left-direction threshold");
        mapper.setLeftStick(0, 0, 20000);
        check((mapper.state(0) & mask(SnesButton::Down)) != 0 &&
              (mapper.state(0) & mask(SnesButton::Left)) == 0,
              "Analog stick direction updates without stale axis state");
        mapper.setLeftStick(0, 2000, 2000);
        check(mapper.state(0) == mask(SnesButton::A),
              "Analog deadzone suppresses small stick drift");
        mapper.setDeadzone(1000);
        mapper.setLeftStick(0, 2000, 0);
        check((mapper.state(0) & mask(SnesButton::Right)) != 0,
              "Controller deadzone can be adjusted");
        mapper.setButton(1, GamepadControl::Start, true);
        check(mapper.state(1) == mask(SnesButton::Start) &&
              (mapper.state(0) & mask(SnesButton::Start)) == 0,
              "Player two input remains independent from player one");
        check(!mapper.setMapping(2, GamepadControl::South, SnesButton::B),
              "Controller rejects an invalid player index");
        mapper.reset();
        check(mapper.state(0) == 0 && mapper.state(1) == 0,
              "Controller reset clears both players");
    }

    // --------------------------------------
    // TEST 18: SNES serial joypad hardware
    // --------------------------------------
    {
        std::cout << "\n--- SNES SERIAL JOYPAD TEST ---\n";
        Bus bus;
        const uint16_t bOnly = static_cast<uint16_t>(1u << static_cast<unsigned>(SnesButton::B));
        const uint16_t playerOne = static_cast<uint16_t>(
            (1u << 0u) | (1u << 3u) | (1u << 8u) | (1u << 11u));
        const uint16_t playerTwo = static_cast<uint16_t>(
            (1u << 2u) | (1u << 7u) | (1u << 10u));

        check(bus.read(0x004016) == 1u,
              "Unconnected SNES joypad serial input reads high");
        bus.setControllerState(0, bOnly);
        bus.write(0x004016, 1u);
        check(bus.read(0x004016) == 1u && bus.read(0x004016) == 1u,
              "High strobe continuously exposes the live B button");
        bus.setControllerState(0, 0u);
        check(bus.read(0x004016) == 0u,
              "High-strobe B input updates without shifting");

        bus.setControllerState(0, playerOne);
        bus.write(0x004016, 1u);
        bus.write(0x004016, 0u);
        bus.setControllerState(0, 0u);
        uint16_t capturedOne = 0u;
        for (unsigned bit = 0; bit < 16u; ++bit) {
            capturedOne |= static_cast<uint16_t>(
                (bus.read(0x004016) & 1u) << bit);
        }
        check(capturedOne == static_cast<uint16_t>(playerOne | 0xF000u),
              "Player-one stream returns twelve buttons followed by four high bits");
        check(bus.read(0x004016) == 1u && bus.read(0x004016) == 1u,
              "Serial joypad reads shift in logic-high after sixteen bits");

        bus.setControllerState(1, playerTwo);
        bus.write(0x004016, 1u);
        bus.write(0x004016, 0u);
        uint16_t capturedTwo = 0u;
        for (unsigned bit = 0; bit < 16u; ++bit) {
            capturedTwo |= static_cast<uint16_t>(
                (bus.read(0x004017) & 1u) << bit);
        }
        check(capturedTwo == static_cast<uint16_t>(playerTwo | 0xF000u),
              "Player-two port independently serializes buttons and trailing high bits");

        bus.setControllerState(0, playerOne);
        bus.write(0x804016, 1u);
        check(bus.read(0x804016) == 1u,
              "Joypad registers mirror in upper system I/O banks");
        bus.write(0x804016, 0u);
        check(bus.read(0x804016) == 1u,
              "Mirrored joypad register preserves serial read behavior");
    }

    // --------------------------------------
    // TEST 19: Cartridge-to-emulator session integration
    // --------------------------------------
    {
        std::cout << "\n--- EMULATOR SESSION INTEGRATION TEST ---\n";
        const auto tempRoot = std::filesystem::temp_directory_path() /
            "snes_macheen_emulator_session_test";
        std::error_code fsError;
        std::filesystem::remove_all(tempRoot, fsError);
        fsError.clear();
        std::filesystem::create_directories(tempRoot, fsError);
        check(!fsError, "Emulator session test directory can be created");

        const auto romPath = tempRoot / "synthetic_lorom.sfc";
        std::vector<uint8_t> rom(0x8000u, 0xEAu);
        rom[0] = 0xA9; // LDA #$42
        rom[1] = 0x42;
        rom[2] = 0xDB; // STP
        rom[0x7FFCu] = 0x00; // Reset vector -> $8000
        rom[0x7FFDu] = 0x80;
        {
            std::ofstream output(romPath, std::ios::binary);
            output.write(reinterpret_cast<const char*>(rom.data()),
                         static_cast<std::streamsize>(rom.size()));
        }
        check(std::filesystem::file_size(romPath) == rom.size(),
              "Synthetic cartridge fixture is written completely");

        EmulatorSession session;
        check(!session.loaded() && session.state() == EmulatorSession::State::Empty,
              "Emulator session starts without a cartridge");
        check(session.loadRom(romPath.string(), CartridgeMapping::LoROM),
              "Emulator session loads a LoROM cartridge");
        check(session.loaded() && session.programCounter() == 0x8000,
              "Session resets CPU from the cartridge reset vector");
        check(session.runInstructions(8) == 2,
              "Session executes the synthetic LDA and STP instructions");
        check(session.cpuStopped() && session.state() == EmulatorSession::State::Halted,
              "Session reports the CPU STP state");
        check(session.cycles() == 5,
              "Session reports CPU cycle accumulation");
        const auto& sessionFrame = session.framebufferArgb8888();
        check(sessionFrame.size() == PPU::FRAME_PIXELS,
              "Session exposes a complete PPU framebuffer");
        check(sessionFrame[0] == 0xFF000000u,
              "Session framebuffer is opaque ARGB8888 output");
        check(!session.loadRom((tempRoot / "missing.sfc").string()),
              "Session rejects a missing ROM path");
        check(!session.loaded() && session.state() == EmulatorSession::State::Empty,
              "Failed replacement load leaves session safely empty");
        session.unload();
        check(session.state() == EmulatorSession::State::Empty,
              "Session unload returns to the empty state");
        std::filesystem::remove_all(tempRoot, fsError);
    }

    // --------------------------------------
    // TEST 20: NTSC/PAL host frame pacing
    // --------------------------------------
    {
        std::cout << "\n--- HOST FRAME PACER TEST ---\n";
        FramePacer ntsc;
        check(ntsc.frameIntervalNanoseconds() == FramePacer::NTSC_FRAME_NS,
              "Default host pacing uses the NTSC presentation interval");
        check(ntsc.advance(0u) == 0u,
              "No elapsed time produces no frame deadline");
        check(ntsc.advance(FramePacer::NTSC_FRAME_NS - 1u) == 0u,
              "NTSC pacer accumulates fractional frame time");
        check(ntsc.advance(1u) == 1u,
              "NTSC pacer emits a frame exactly at its interval");
        check(ntsc.advance(8000000u) == 0u &&
              ntsc.advance(FramePacer::NTSC_FRAME_NS - 8000000u) == 1u,
              "Sub-frame elapsed time carries across updates");

        FramePacer bounded(VideoStandard::Ntsc, 2u);
        check(bounded.advance(FramePacer::NTSC_FRAME_NS * 5u) == 2u,
              "Long stalls cap catch-up work to the configured frame limit");
        check(bounded.droppedFrames() == 3u &&
              bounded.fractionalRemainderNanoseconds() == 0u,
              "Excess overdue frames are counted and backlog is discarded");
        check(bounded.delayMilliseconds() >= 1u && bounded.delayMilliseconds() <= 17u,
              "Pacer reports a bounded sleep until its next frame");

        FramePacer pal(VideoStandard::Pal);
        check(pal.frameIntervalNanoseconds() == FramePacer::PAL_FRAME_NS &&
              pal.advance(FramePacer::PAL_FRAME_NS) == 1u,
              "PAL mode uses its slower presentation interval");
        pal.setVideoStandard(VideoStandard::Ntsc);
        check(pal.fractionalRemainderNanoseconds() == 0u &&
              pal.frameIntervalNanoseconds() == FramePacer::NTSC_FRAME_NS,
              "Changing video standard resets fractional frame timing");
        pal.reset();
        check(pal.droppedFrames() == 0u,
              "Pacer reset clears dropped-frame diagnostics");
    }

    // --------------------------------------
    // FINAL RESULTS
    // --------------------------------------

    std::cout << "\n====================================\n";
    std::cout << "      ALL TESTS PASSED\n";
    std::cout << "      Successful checks: "
              << std::dec << checks << "\n";
    std::cout << "====================================\n";

    return 0;
}
