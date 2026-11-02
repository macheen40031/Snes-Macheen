#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstdint>

#include "cpu.hpp"
#include "bus.hpp"

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
    std::cout << "       SuperNES ALU TEST SUITE\n";
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

        runSteps(cpu, bus, 14);

        check(cpu.A == 0x70,
              "AND, ORA and EOR results");

        check((cpu.P & 0x03) == 0x03,
              "CMP, CPX and CPY Zero/Carry flags");

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
    // FINAL RESULTS
    // --------------------------------------

    std::cout << "\n====================================\n";
    std::cout << "      ALL TESTS PASSED\n";
    std::cout << "      Successful checks: "
              << checks << "\n";
    std::cout << "====================================\n";

    return 0;
}
