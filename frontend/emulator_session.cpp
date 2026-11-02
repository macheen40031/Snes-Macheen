#include "emulator_session.hpp"

bool EmulatorSession::loadRom(const std::string& path, CartridgeMapping mapping) {
    if (loaded()) saveBatteryRam();
    memoryBus.unloadCartridge();
    if (!memoryBus.loadCartridge(path, mapping)) {
        currentState = State::Empty;
        currentRomPath.clear();
        currentStatus = "Unable to load ROM file.";
        return false;
    }

    currentRomPath = path;
    processor.traceEnabled = false;
    processor.reset(memoryBus);
    currentState = State::Ready;
    currentStatus = "Loaded " + path + " (" + memoryBus.cartridgeMapping() + ").";
    framePixels = memoryBus.ppu().renderFrameArgb8888();
    return true;
}

void EmulatorSession::unload() noexcept {
    if (loaded()) saveBatteryRam();
    memoryBus.unloadCartridge();
    currentRomPath.clear();
    currentState = State::Empty;
    currentStatus = "No cartridge loaded.";
    framePixels.fill(0xFF000000u);
}

std::size_t EmulatorSession::runInstructions(std::size_t instructionBudget) {
    if (!loaded() || currentState == State::Faulted || currentState == State::Halted)
        return 0;

    currentState = State::Running;
    std::size_t executed = 0;
    for (; executed < instructionBudget; ++executed) {
        if (!processor.step(memoryBus)) {
            currentState = State::Faulted;
            currentStatus = "CPU stopped on an unsupported opcode. Core instruction coverage is incomplete.";
            break;
        }
        if (processor.stopped) {
            currentState = State::Halted;
            currentStatus = "CPU executed STP. The current prototype has no interrupt/wake scheduler.";
            ++executed;
            break;
        }
    }
    return executed;
}

std::size_t EmulatorSession::runFrame(std::size_t instructionBudget) {
    const std::size_t executed = runInstructions(instructionBudget);
    framePixels = memoryBus.ppu().renderFrameArgb8888();
    return executed;
}

bool EmulatorSession::saveBatteryRam() const { return memoryBus.saveCartridgeSram(); }
void EmulatorSession::setControllerState(std::size_t player, uint16_t buttons) noexcept {
    memoryBus.setControllerState(player, buttons);
}
bool EmulatorSession::loaded() const noexcept { return memoryBus.cartridgeLoaded(); }
EmulatorSession::State EmulatorSession::state() const noexcept { return currentState; }
const std::string& EmulatorSession::status() const noexcept { return currentStatus; }
const std::string& EmulatorSession::romPath() const noexcept { return currentRomPath; }
uint16_t EmulatorSession::programCounter() const noexcept { return processor.PC; }
uint64_t EmulatorSession::cycles() const noexcept { return processor.cycles; }
bool EmulatorSession::cpuStopped() const noexcept { return processor.stopped; }
const std::array<uint32_t, PPU::FRAME_PIXELS>& EmulatorSession::framebufferArgb8888() {
    framePixels = memoryBus.ppu().renderFrameArgb8888();
    return framePixels;
}
const Bus& EmulatorSession::bus() const noexcept { return memoryBus; }
