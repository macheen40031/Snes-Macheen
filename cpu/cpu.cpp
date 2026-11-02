#include "cpu.hpp"

#include <iostream>
#include <iomanip>
#include <ostream>
#include <streambuf>

namespace {
class CpuNullBuffer final : public std::streambuf {
protected:
    int overflow(int character) override { return traits_type::not_eof(character); }
};

std::ostream& cpuNullStream() {
    static CpuNullBuffer buffer;
    static std::ostream stream(&buffer);
    return stream;
}
}

bool CPU::accumulator8Bit() const {
    return emulation || (P & 0x20);
}

bool CPU::index8Bit() const {
    return emulation || (P & 0x10);
}

uint8_t CPU::fetch8(Bus& bus) {
    uint32_t address =
        (static_cast<uint32_t>(PB) << 16) | PC;

    uint8_t value = bus.read(address);
    PC = static_cast<uint16_t>(PC + 1);

    return value;
}

uint16_t CPU::fetch16(Bus& bus) {
    uint16_t low = fetch8(bus);
    uint16_t high = fetch8(bus);

    return low | (high << 8);
}

uint16_t CPU::fetchImmediate(Bus& bus, bool eightBit) {
    uint16_t value = fetch8(bus);

    if (!eightBit)
        value |= static_cast<uint16_t>(fetch8(bus)) << 8;

    return value;
}

uint32_t CPU::dataAddress(uint16_t address) const {
    return (static_cast<uint32_t>(DB) << 16) | address;
}

uint32_t CPU::directPageAddress(uint8_t offset) const {
    return static_cast<uint16_t>(D + offset);
}

uint32_t CPU::indexedDataAddress(
    uint16_t base,
    uint16_t index
) const {
    return dataAddress(static_cast<uint16_t>(base + index));
}

uint16_t CPU::readDirectPagePointer(Bus& bus, uint8_t offset) const {
    const uint16_t pointerAddress = directPageAddress(offset);
    const uint8_t low = bus.read(pointerAddress);
    const uint8_t high = bus.read(static_cast<uint16_t>(pointerAddress + 1));
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
}

uint32_t CPU::readDirectPageLongPointer(Bus& bus, uint8_t offset) const {
    const uint16_t pointerAddress = directPageAddress(offset);
    const uint32_t low = bus.read(pointerAddress);
    const uint32_t middle = bus.read(static_cast<uint16_t>(pointerAddress + 1));
    const uint32_t high = bus.read(static_cast<uint16_t>(pointerAddress + 2));
    return low | (middle << 8) | (high << 16);
}

void CPU::push8(Bus& bus, uint8_t value) {
    bus.write(S, value);
    if (emulation) {
        S = static_cast<uint16_t>(0x0100 | ((S - 1) & 0x00FF));
    } else {
        S = static_cast<uint16_t>(S - 1);
    }
}

uint8_t CPU::pull8(Bus& bus) {
    if (emulation) {
        S = static_cast<uint16_t>(0x0100 | ((S + 1) & 0x00FF));
    } else {
        S = static_cast<uint16_t>(S + 1);
    }
    return bus.read(S);
}

void CPU::push16(Bus& bus, uint16_t value) {
    push8(bus, static_cast<uint8_t>(value >> 8));
    push8(bus, static_cast<uint8_t>(value));
}

uint16_t CPU::pull16(Bus& bus) {
    const uint16_t low = pull8(bus);
    const uint16_t high = pull8(bus);
    return static_cast<uint16_t>(low | (high << 8));
}

void CPU::setNZ(uint16_t value, bool eightBit) {
    P &= static_cast<uint8_t>(~0x82);

    uint16_t result =
        eightBit ? (value & 0x00FF) : value;

    if (result == 0)
        P |= 0x02;

    if (eightBit ? (result & 0x0080)
                 : (result & 0x8000))
        P |= 0x80;
}

void CPU::setEmulationMode(bool enabled) {
    emulation = enabled;

    if (emulation) {
        P |= 0x30;
        S = 0x0100 | (S & 0x00FF);
    }

    if (P & 0x10) {
        X &= 0x00FF;
        Y &= 0x00FF;
    }
}

void CPU::adc(uint16_t operand, bool eightBit) {
    const uint32_t mask = eightBit ? 0xFF : 0xFFFF;
    const uint32_t sign = eightBit ? 0x80 : 0x8000;

    const uint32_t lhs = A & mask;
    const uint32_t carryIn = (P & 0x01) ? 1 : 0;

    const uint32_t binarySum =
        lhs + operand + carryIn;

    const uint32_t binaryResult = binarySum & mask;

    const bool overflow =
        (~(lhs ^ operand) &
         (lhs ^ binaryResult) & sign) != 0;

    uint32_t result = 0;
    bool carryOut = false;

    if (P & 0x08) {
        // Binary Coded Decimal addition.
        uint32_t digitCarry = carryIn;

        for (unsigned shift = 0;
             shift < (eightBit ? 8u : 16u);
             shift += 4) {

            uint32_t digit =
                ((lhs >> shift) & 0x0F) +
                ((operand >> shift) & 0x0F) +
                digitCarry;

            if (digit > 9)
                digit += 6;

            digitCarry = digit > 0x0F;

            result |= (digit & 0x0F) << shift;
        }

        carryOut = digitCarry != 0;

    } else {
        result = binaryResult;
        carryOut = binarySum > mask;
    }

    if (eightBit)
        A = (A & 0xFF00) | (result & 0x00FF);
    else
        A = result & 0xFFFF;

    P &= 0x3C;

    if (carryOut)
        P |= 0x01;

    if (overflow)
        P |= 0x40;

    setNZ(A, eightBit);
}

void CPU::sbc(uint16_t operand, bool eightBit) {
    const uint32_t mask = eightBit ? 0xFF : 0xFFFF;
    const uint32_t sign = eightBit ? 0x80 : 0x8000;

    const uint32_t lhs = A & mask;
    const int32_t borrowIn = (P & 0x01) ? 0 : 1;

    const int32_t binaryDifference =
        static_cast<int32_t>(lhs) -
        static_cast<int32_t>(operand) -
        borrowIn;

    const uint32_t binaryResult =
        static_cast<uint32_t>(binaryDifference) & mask;

    const bool overflow =
        ((lhs ^ operand) &
         (lhs ^ binaryResult) & sign) != 0;

    uint32_t result = 0;
    bool carryOut = false;

    if (P & 0x08) {
        // Binary Coded Decimal subtraction.
        int32_t digitBorrow = borrowIn;

        for (unsigned shift = 0;
             shift < (eightBit ? 8u : 16u);
             shift += 4) {

            int32_t digit =
                static_cast<int32_t>((lhs >> shift) & 0x0F) -
                static_cast<int32_t>((operand >> shift) & 0x0F) -
                digitBorrow;

            if (digit < 0) {
                digit -= 6;
                digitBorrow = 1;
            } else {
                digitBorrow = 0;
            }

            result |=
                (static_cast<uint32_t>(digit) & 0x0F) << shift;
        }

        carryOut = digitBorrow == 0;

    } else {
        result = binaryResult;
        carryOut = binaryDifference >= 0;
    }

    if (eightBit)
        A = (A & 0xFF00) | (result & 0x00FF);
    else
        A = result & 0xFFFF;

    P &= 0x3C;

    if (carryOut)
        P |= 0x01;

    if (overflow)
        P |= 0x40;

    setNZ(A, eightBit);
}

void CPU::compare(
    uint16_t lhs,
    uint16_t rhs,
    bool eightBit
) {
    const uint32_t mask = eightBit ? 0xFF : 0xFFFF;

    lhs &= mask;
    rhs &= mask;

    uint32_t result = (lhs - rhs) & mask;

    // Compare modifies N, Z and C, but not V.
    P &= static_cast<uint8_t>(~0x83);

    if (lhs >= rhs)
        P |= 0x01;

    setNZ(result, eightBit);
}

void CPU::reset(Bus& bus) {
    A = X = Y = D = 0;
    S = 0x01FF;

    DB = PB = 0;
    P = 0x34;

    emulation = true;
    stopped = false;
    cycles = 0;

    PC = static_cast<uint16_t>(
        bus.read(0x00FFFC) |
        (static_cast<uint16_t>(bus.read(0x00FFFD)) << 8)
    );
}

bool CPU::step(Bus& bus) {
    if (stopped)
        return false;

    std::ostream& trace = traceEnabled ? std::cout : cpuNullStream();
    uint16_t instructionPC = PC;
    uint8_t opcode = fetch8(bus);

    trace << "PC: "
              << std::hex << std::uppercase
              << std::setw(4) << std::setfill('0')
              << instructionPC
              << " OPCODE: "
              << std::setw(2)
              << static_cast<int>(opcode);

    switch (opcode) {

        case 0x48: { // PHA
            const bool eightBit = accumulator8Bit();
            if (eightBit) push8(bus, static_cast<uint8_t>(A));
            else push16(bus, A);
            trace << " PHA\n";
            cycles += eightBit ? 3 : 4;
            break;
        }

        case 0x68: { // PLA
            const bool eightBit = accumulator8Bit();
            if (eightBit) A = static_cast<uint16_t>((A & 0xFF00) | pull8(bus));
            else A = pull16(bus);
            setNZ(A, eightBit);
            trace << " PLA\n";
            cycles += eightBit ? 4 : 5;
            break;
        }

        case 0x08: // PHP
            push8(bus, P);
            trace << " PHP\n";
            cycles += 3;
            break;

        case 0x28: { // PLP
            P = pull8(bus);
            if (emulation) P |= 0x30;
            if (P & 0x10) { X &= 0x00FF; Y &= 0x00FF; }
            trace << " PLP\n";
            cycles += 4;
            break;
        }

        case 0x20: { // JSR absolute
            const uint16_t destination = fetch16(bus);
            push16(bus, static_cast<uint16_t>(PC - 1));
            PC = destination;
            trace << " JSR $" << std::setw(4) << destination << "\n";
            cycles += 6;
            break;
        }

        case 0x60: { // RTS
            PC = static_cast<uint16_t>(pull16(bus) + 1);
            trace << " RTS\n";
            cycles += 6;
            break;
        }
        case 0xEA: // NOP
            trace << " NOP\n";
            cycles += 2;
            break;

        case 0x18: // CLC
            P &= 0xFE;
            trace << " CLC\n";
            cycles += 2;
            break;

        case 0x38: // SEC
            P |= 0x01;
            trace << " SEC\n";
            cycles += 2;
            break;

        case 0xF8: // SED
            P |= 0x08;
            trace << " SED\n";
            cycles += 2;
            break;

        case 0xD8: // CLD
            P &= 0xF7;
            trace << " CLD\n";
            cycles += 2;
            break;

        case 0xA9: { // LDA immediate
            bool eightBit = accumulator8Bit();

            uint16_t value = fetchImmediate(bus, eightBit);

            if (eightBit)
                A = (A & 0xFF00) | value;
            else
                A = value;

            setNZ(A, eightBit);

            trace << " LDA #$"
                      << std::setw(eightBit ? 2 : 4)
                      << value << "\n";

            cycles += eightBit ? 2 : 3;
            break;
        }

        case 0x8D: { // STA absolute
            uint16_t address = fetch16(bus);
            uint32_t target = dataAddress(address);
            bool eightBit = accumulator8Bit();

            bus.write(target, A & 0xFF);

            if (!eightBit) {
                bus.write(
                    dataAddress(static_cast<uint16_t>(address + 1)),
                    (A >> 8) & 0xFF
                );
            }

            trace << " STA $"
                      << std::setw(4) << address << "\n";

            cycles += eightBit ? 4 : 5;
            break;
        }

        case 0xA5: { // LDA direct page
            const uint8_t offset = fetch8(bus);
            const uint32_t target = directPageAddress(offset);
            const bool eightBit = accumulator8Bit();

            uint16_t value = bus.read(target);
            if (!eightBit) {
                value |= static_cast<uint16_t>(
                    bus.read(static_cast<uint16_t>(target + 1))
                ) << 8;
            }

            if (eightBit)
                A = (A & 0xFF00) | value;
            else
                A = value;

            setNZ(A, eightBit);
            trace << " LDA direct-page $" << std::setw(2)
                      << static_cast<int>(offset) << "\n";
            cycles += eightBit ? 3 : 4;
            break;
        }

        case 0x85: { // STA direct page
            const uint8_t offset = fetch8(bus);
            const uint32_t target = directPageAddress(offset);
            const bool eightBit = accumulator8Bit();

            bus.write(target, A & 0xFF);
            if (!eightBit) {
                bus.write(
                    static_cast<uint16_t>(target + 1),
                    (A >> 8) & 0xFF
                );
            }

            trace << " STA direct-page $" << std::setw(2)
                      << static_cast<int>(offset) << "\n";
            cycles += eightBit ? 3 : 4;
            break;
        }

        case 0xBD: // LDA absolute,X
        case 0xB9: { // LDA absolute,Y
            const uint16_t baseAddress = fetch16(bus);
            const uint16_t index = opcode == 0xBD ? X : Y;
            const uint16_t address =
                static_cast<uint16_t>(baseAddress + index);
            const uint32_t target = indexedDataAddress(baseAddress, index);
            const bool eightBit = accumulator8Bit();

            uint16_t value = bus.read(target);
            if (!eightBit) {
                value |= static_cast<uint16_t>(
                    bus.read(dataAddress(static_cast<uint16_t>(address + 1)))
                ) << 8;
            }

            if (eightBit)
                A = (A & 0xFF00) | value;
            else
                A = value;

            setNZ(A, eightBit);
            trace << (opcode == 0xBD ? " LDA absolute,X $" : " LDA absolute,Y $")
                      << std::setw(4) << address << "\n";
            cycles += (eightBit ? 4 : 5) + (index8Bit() ? 0 : 1);
            break;
        }

        case 0x9D: // STA absolute,X
        case 0x99: { // STA absolute,Y
            const uint16_t baseAddress = fetch16(bus);
            const uint16_t index = opcode == 0x9D ? X : Y;
            const uint16_t address =
                static_cast<uint16_t>(baseAddress + index);
            const uint32_t target = indexedDataAddress(baseAddress, index);
            const bool eightBit = accumulator8Bit();

            bus.write(target, A & 0xFF);
            if (!eightBit) {
                bus.write(
                    dataAddress(static_cast<uint16_t>(address + 1)),
                    (A >> 8) & 0xFF
                );
            }

            trace << (opcode == 0x9D ? " STA absolute,X $" : " STA absolute,Y $")
                      << std::setw(4) << address << "\n";
            cycles += eightBit ? 5 : 6;
            break;
        }

        case 0xA1: // LDA (direct page,X)
        case 0xB1: // LDA (direct page),Y
        case 0xA7: // LDA [direct page]
        case 0xB7: { // LDA [direct page],Y
            const uint8_t offset = fetch8(bus);
            uint32_t target = 0;

            if (opcode == 0xA1) {
                const uint16_t pointerAddress =
                    static_cast<uint16_t>(D + offset + X);
                const uint16_t pointer = static_cast<uint16_t>(
                    bus.read(pointerAddress) |
                    (static_cast<uint16_t>(bus.read(static_cast<uint16_t>(pointerAddress + 1))) << 8)
                );
                target = dataAddress(pointer);
            } else if (opcode == 0xB1) {
                const uint16_t pointer = readDirectPagePointer(bus, offset);
                target = dataAddress(static_cast<uint16_t>(pointer + Y));
            } else if (opcode == 0xA7) {
                target = readDirectPageLongPointer(bus, offset);
            } else {
                target = (readDirectPageLongPointer(bus, offset) + Y) & 0xFFFFFF;
            }

            const bool eightBit = accumulator8Bit();
            uint16_t value = bus.read(target);
            if (!eightBit) {
                const uint32_t next = opcode == 0xA1 || opcode == 0xB1
                    ? dataAddress(static_cast<uint16_t>((target & 0xFFFF) + 1))
                    : ((target + 1) & 0xFFFFFF);
                value |= static_cast<uint16_t>(bus.read(next)) << 8;
            }

            if (eightBit) A = (A & 0xFF00) | value;
            else A = value;
            setNZ(A, eightBit);

            const char* mode = opcode == 0xA1 ? " LDA (dp,X)"
                : opcode == 0xB1 ? " LDA (dp),Y"
                : opcode == 0xA7 ? " LDA [dp]" : " LDA [dp],Y";
            trace << mode << " $" << std::setw(6) << target << "\n";
            cycles += (opcode == 0xA1 || opcode == 0xA7 ? 6 : 5)
                + (eightBit ? 0 : 1);
            break;
        }

        case 0x81: // STA (direct page,X)
        case 0x91: // STA (direct page),Y
        case 0x87: // STA [direct page]
        case 0x97: { // STA [direct page],Y
            const uint8_t offset = fetch8(bus);
            uint32_t target = 0;

            if (opcode == 0x81) {
                const uint16_t pointerAddress =
                    static_cast<uint16_t>(D + offset + X);
                const uint16_t pointer = static_cast<uint16_t>(
                    bus.read(pointerAddress) |
                    (static_cast<uint16_t>(bus.read(static_cast<uint16_t>(pointerAddress + 1))) << 8)
                );
                target = dataAddress(pointer);
            } else if (opcode == 0x91) {
                const uint16_t pointer = readDirectPagePointer(bus, offset);
                target = dataAddress(static_cast<uint16_t>(pointer + Y));
            } else if (opcode == 0x87) {
                target = readDirectPageLongPointer(bus, offset);
            } else {
                target = (readDirectPageLongPointer(bus, offset) + Y) & 0xFFFFFF;
            }

            const bool eightBit = accumulator8Bit();
            bus.write(target, A & 0xFF);
            if (!eightBit) {
                const uint32_t next = opcode == 0x81 || opcode == 0x91
                    ? dataAddress(static_cast<uint16_t>((target & 0xFFFF) + 1))
                    : ((target + 1) & 0xFFFFFF);
                bus.write(next, (A >> 8) & 0xFF);
            }

            const char* mode = opcode == 0x81 ? " STA (dp,X)"
                : opcode == 0x91 ? " STA (dp),Y"
                : opcode == 0x87 ? " STA [dp]" : " STA [dp],Y";
            trace << mode << " $" << std::setw(6) << target << "\n";
            cycles += (opcode == 0x81 || opcode == 0x87 ? 6 : 5)
                + (eightBit ? 0 : 1);
            break;
        }

        case 0x80: // BRA
        case 0xD0: // BNE
        case 0xF0: { // BEQ
            const int8_t displacement = static_cast<int8_t>(fetch8(bus));
            const bool zero = (P & 0x02) != 0;
            const bool take = opcode == 0x80 ||
                (opcode == 0xD0 && !zero) ||
                (opcode == 0xF0 && zero);
            if (take) PC = static_cast<uint16_t>(PC + displacement);
            trace << (opcode == 0x80 ? " BRA" : opcode == 0xD0 ? " BNE" : " BEQ")
                      << " " << static_cast<int>(displacement)
                      << (take ? " [taken]\n" : " [not taken]\n");
            cycles += take ? 3 : 2;
            break;
        }

        case 0x4C: { // JMP absolute (within current program bank)
            const uint16_t destination = fetch16(bus);
            PC = destination;
            trace << " JMP $" << std::setw(4) << destination << "\n";
            cycles += 3;
            break;
        }

        case 0xAD: { // LDA absolute
            uint16_t address = fetch16(bus);
            uint32_t target = dataAddress(address);
            bool eightBit = accumulator8Bit();

            uint16_t value = bus.read(target);

            if (!eightBit) {
                value |=
                    static_cast<uint16_t>(
                        bus.read(
                            dataAddress(
                                static_cast<uint16_t>(address + 1)
                            )
                        )
                    ) << 8;
            }

            if (eightBit)
                A = (A & 0xFF00) | value;
            else
                A = value;

            setNZ(A, eightBit);

            trace << " LDA $"
                      << std::setw(4) << address << "\n";

            cycles += eightBit ? 4 : 5;
            break;
        }

        case 0x69: { // ADC immediate
            bool eightBit = accumulator8Bit();
            uint16_t value = fetchImmediate(bus, eightBit);

            adc(value, eightBit);

            trace << " ADC #$"
                      << std::setw(eightBit ? 2 : 4)
                      << value << "\n";

            cycles += eightBit ? 2 : 3;
            break;
        }

        case 0xE9: { // SBC immediate
            bool eightBit = accumulator8Bit();
            uint16_t value = fetchImmediate(bus, eightBit);

            sbc(value, eightBit);

            trace << " SBC #$"
                      << std::setw(eightBit ? 2 : 4)
                      << value << "\n";

            cycles += eightBit ? 2 : 3;
            break;
        }

        case 0x29: // AND immediate
        case 0x09: // ORA immediate
        case 0x49: { // EOR immediate

            bool eightBit = accumulator8Bit();
            uint16_t value = fetchImmediate(bus, eightBit);
            uint16_t current =
                eightBit ? (A & 0x00FF) : A;

            const char* name = "";

            if (opcode == 0x29) {
                current &= value;
                name = " AND ";
            } else if (opcode == 0x09) {
                current |= value;
                name = " ORA ";
            } else {
                current ^= value;
                name = " EOR ";
            }

            if (eightBit)
                A = (A & 0xFF00) | current;
            else
                A = current;

            setNZ(A, eightBit);

            trace << name << "#$"
                      << std::setw(eightBit ? 2 : 4)
                      << value << "\n";

            cycles += eightBit ? 2 : 3;
            break;
        }

        case 0xC9: { // CMP immediate
            bool eightBit = accumulator8Bit();
            uint16_t value = fetchImmediate(bus, eightBit);

            compare(
                eightBit ? A & 0x00FF : A,
                value,
                eightBit
            );

            trace << " CMP #$"
                      << std::setw(eightBit ? 2 : 4)
                      << value << "\n";

            cycles += eightBit ? 2 : 3;
            break;
        }

        case 0xE0: // CPX immediate
        case 0xC0: { // CPY immediate
            bool eightBit = index8Bit();
            uint16_t value = fetchImmediate(bus, eightBit);

            uint16_t source =
                opcode == 0xE0 ? X : Y;

            compare(source, value, eightBit);

            trace
                << (opcode == 0xE0 ? " CPX #$" : " CPY #$")
                << std::setw(eightBit ? 2 : 4)
                << value << "\n";

            cycles += eightBit ? 2 : 3;
            break;
        }

        case 0xE8: // INX
        case 0xC8: // INY
        case 0xCA: // DEX
        case 0x88: { // DEY

            bool xRegister =
                opcode == 0xE8 || opcode == 0xCA;

            bool increment =
                opcode == 0xE8 || opcode == 0xC8;

            bool eightBit = index8Bit();

            uint32_t mask = eightBit ? 0xFF : 0xFFFF;

            uint16_t& target = xRegister ? X : Y;

            target = increment
                ? (target + 1) & mask
                : (target - 1) & mask;

            setNZ(target, eightBit);

            if (opcode == 0xE8) trace << " INX\n";
            if (opcode == 0xC8) trace << " INY\n";
            if (opcode == 0xCA) trace << " DEX\n";
            if (opcode == 0x88) trace << " DEY\n";

            cycles += 2;
            break;
        }

        case 0xAA: // TAX
        case 0xA8: // TAY
        case 0x8A: // TXA
        case 0x98: { // TYA

            bool toIndex =
                opcode == 0xAA || opcode == 0xA8;

            bool xRegister =
                opcode == 0xAA || opcode == 0x8A;

            if (toIndex) {
                bool eightBit = index8Bit();

                uint16_t source = A;

                uint16_t& destination =
                    xRegister ? X : Y;

                destination = eightBit
                    ? source & 0x00FF
                    : source;

                setNZ(destination, eightBit);

            } else {
                bool eightBit = accumulator8Bit();

                uint16_t source = xRegister ? X : Y;

                if (eightBit)
                    A = (A & 0xFF00) | (source & 0x00FF);
                else
                    A = source;

                setNZ(A, eightBit);
            }

            if (opcode == 0xAA) trace << " TAX\n";
            if (opcode == 0xA8) trace << " TAY\n";
            if (opcode == 0x8A) trace << " TXA\n";
            if (opcode == 0x98) trace << " TYA\n";

            cycles += 2;
            break;
        }

        case 0xC2: { // REP
            uint8_t mask = fetch8(bus);

            P &= static_cast<uint8_t>(~mask);

            if (emulation)
                P |= 0x30;

            if (P & 0x10) {
                X &= 0x00FF;
                Y &= 0x00FF;
            }

            trace << " REP #$"
                      << std::setw(2)
                      << static_cast<int>(mask) << "\n";

            cycles += 3;
            break;
        }

        case 0xE2: { // SEP
            uint8_t mask = fetch8(bus);

            P |= mask;

            if (P & 0x10) {
                X &= 0x00FF;
                Y &= 0x00FF;
            }

            trace << " SEP #$"
                      << std::setw(2)
                      << static_cast<int>(mask) << "\n";

            cycles += 3;
            break;
        }

        case 0xFB: { // XCE
            bool oldCarry = (P & 0x01) != 0;
            bool oldEmulation = emulation;

            setEmulationMode(oldCarry);

            if (oldEmulation)
                P |= 0x01;
            else
                P &= 0xFE;

            trace << " XCE\n";
            cycles += 2;
            break;
        }

        case 0xDB: // STP
            stopped = true;
            trace << " STP\n";
            cycles += 3;
            break;

        default:
            trace << " UNKNOWN OPCODE\n";
            stopped = true;
            return false;
    }

    return true;
}
