#pragma once

#include <cstdint>
#include "bus.hpp"

class CPU {
public:
    uint16_t A = 0;
    uint16_t X = 0;
    uint16_t Y = 0;
    uint16_t S = 0x01FF;
    uint16_t D = 0;

    uint8_t DB = 0;
    uint8_t PB = 0;

    uint16_t PC = 0;
    uint8_t P = 0x34;

    bool emulation = true;
    bool stopped = false;
    bool traceEnabled = true;
    uint64_t cycles = 0;

    void reset(Bus& bus);
    bool step(Bus& bus);

    bool accumulator8Bit() const;
    bool index8Bit() const;

private:
    uint8_t fetch8(Bus& bus);
    uint16_t fetch16(Bus& bus);
    uint16_t fetchImmediate(Bus& bus, bool eightBit);

    void setNZ(uint16_t value, bool eightBit);
    void setEmulationMode(bool enabled);

    void adc(uint16_t operand, bool eightBit);
    void sbc(uint16_t operand, bool eightBit);
    void compare(uint16_t lhs, uint16_t rhs, bool eightBit);

    uint32_t dataAddress(uint16_t address) const;
    uint32_t directPageAddress(uint8_t offset) const;
    uint32_t indexedDataAddress(uint16_t base, uint16_t index) const;
    uint16_t readDirectPagePointer(Bus& bus, uint8_t offset) const;
    uint32_t readDirectPageLongPointer(Bus& bus, uint8_t offset) const;
    void push8(Bus& bus, uint8_t value);
    uint8_t pull8(Bus& bus);
    void push16(Bus& bus, uint16_t value);
    uint16_t pull16(Bus& bus);
};
