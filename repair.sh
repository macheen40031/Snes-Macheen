
#!/data/data/com.termux/files/usr/bin/bash
set -e

cd "$(dirname "$0")"

mkdir -p src cpu memory ppu apu cartridge tests include

cat > include/bus.hpp <<'EOF'
#pragma once
#include <cstdint>
#include <vector>

class Bus {
public:
    static constexpr uint32_t SIZE = 1u << 24;

    Bus();
    uint8_t read(uint32_t address) const;
    void write(uint32_t address, uint8_t value);

private:
    std::vector<uint8_t> memory;
};
EOF

cat > memory/bus.cpp <<'EOF'
#include "bus.hpp"

Bus::Bus() : memory(SIZE, 0) {}

uint8_t Bus::read(uint32_t address) const {
    return memory[address & 0xFFFFFF];
}

void Bus::write(uint32_t address, uint8_t value) {
    memory[address & 0xFFFFFF] = value;
}
EOF

cat > include/cpu.hpp <<'EOF'
#pragma once
#include <cstdint>
#include "bus.hpp"

class CPU {
public:
    uint16_t A = 0, X = 0, Y = 0;
    uint16_t S = 0x01FF, D = 0;
    uint8_t DB = 0, PB = 0;
    uint16_t PC = 0;
    uint8_t P = 0x34;

    bool emulation = true;
    bool stopped = false;
    uint64_t cycles = 0;

    void reset(Bus& bus);
    bool step(Bus& bus);

private:
    uint8_t fetch8(Bus& bus);
};
EOF

cat > cpu/cpu.cpp <<'EOF'
#include "cpu.hpp"
#include <iostream>
#include <iomanip>

uint8_t CPU::fetch8(Bus& bus) {
    uint32_t address =
        (static_cast<uint32_t>(PB) << 16) | PC;

    uint8_t value = bus.read(address);
    PC = static_cast<uint16_t>(PC + 1);
    return value;
}

void CPU::reset(Bus& bus) {
    A = X = Y = D = 0;
    S = 0x01FF;
    DB = PB = 0;
    P = 0x34;
    emulation = true;
    stopped = false;
    cycles = 0;

    uint8_t low = bus.read(0x00FFFC);
    uint8_t high = bus.read(0x00FFFD);

    PC = static_cast<uint16_t>(
        low | (static_cast<uint16_t>(high) << 8)
    );
}

bool CPU::step(Bus& bus) {
    if (stopped)
        return false;

    uint16_t instructionPC = PC;
    uint8_t opcode = fetch8(bus);

    std::cout << "PC: "
              << std::hex << std::uppercase
              << std::setw(4) << std::setfill('0')
              << instructionPC
              << " OPCODE: "
              << std::setw(2)
              << static_cast<int>(opcode);

    switch (opcode) {
        case 0xEA:
            std::cout << " NOP\n";
            cycles += 2;
            break;

        case 0x18:
            P &= 0xFE;
            std::cout << " CLC\n";
            cycles += 2;
            break;

        case 0x38:
            P |= 0x01;
            std::cout << " SEC\n";
            cycles += 2;
            break;

        case 0xDB:
            stopped = true;
            std::cout << " STP\n";
            cycles += 3;
            break;

        default:
            std::cout << " UNKNOWN OPCODE\n";
            stopped = true;
            return false;
    }

    return true;
}
EOF

cat > src/main.cpp <<'EOF'
#include <iostream>
#include "cpu.hpp"
#include "bus.hpp"

int main() {
    Bus bus;
    CPU cpu;

    bus.write(0x00FFFC, 0x00);
    bus.write(0x00FFFD, 0x80);

    bus.write(0x008000, 0xEA);
    bus.write(0x008001, 0x18);
    bus.write(0x008002, 0x38);
    bus.write(0x008003, 0xDB);

    cpu.reset(bus);

    std::cout << "\n===== SuperNES CPU TEST =====\n\n";

    while (cpu.step(bus)) {}

    std::cout << "\n===== CPU RESULTS =====\n";
    std::cout << "Accumulator: " << cpu.A << "\n";
    std::cout << "Program Counter: " << std::hex
              << cpu.PC << "\n";
    std::cout << "Status Register: "
              << static_cast<int>(cpu.P) << "\n";
    std::cout << "Total Cycles: " << std::dec
              << cpu.cycles << "\n";
    std::cout << "\nCPU TEST COMPLETE!\n";

    return 0;
}
EOF

cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.20)

project(SuperNES LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(supernes
    src/main.cpp
    cpu/cpu.cpp
    memory/bus.cpp
)

target_include_directories(supernes PRIVATE include)

target_compile_options(supernes PRIVATE
    -Wall -Wextra -Wpedantic
)
EOF

echo "SuperNES source files repaired."
echo "Building project..."

cmake -S . -B build -G Ninja
cmake --build build

echo ""
echo "Running CPU test..."
./build/supernes

