#pragma once
#include <cstdint>
#include <string>
#include <vector>

class LibretroSession {
public:
    LibretroSession();
    ~LibretroSession();
    LibretroSession(const LibretroSession&) = delete;
    LibretroSession& operator=(const LibretroSession&) = delete;

    bool loadRom(const std::string& corePath, const std::string& romPath);
    void unload() noexcept;
    void runFrame();
    void setControllerState(unsigned player, uint16_t buttons) noexcept;
    bool loaded() const noexcept { return gameLoaded; }
    const std::string& status() const noexcept { return message; }
    const std::vector<uint32_t>& framebuffer() const noexcept { return pixels; }
    unsigned width() const noexcept { return frameWidth; }
    unsigned height() const noexcept { return frameHeight; }
    bool saveRam(const std::string& path) const;

private:
    void* library = nullptr;
    bool initialized = false;
    bool gameLoaded = false;
    std::string message = "Snes9x core not loaded.";
    std::vector<uint32_t> pixels;
    unsigned frameWidth = 256;
    unsigned frameHeight = 224;
    uint16_t pads[2]{};
    uint32_t audioDevice = 0;
    int audioFrequency = 0;

    static LibretroSession* active;
    static bool environment(unsigned command, void* data);
    static void videoRefresh(const void* data, unsigned width, unsigned height, std::size_t pitch);
    static void audioSample(int16_t left, int16_t right);
    static std::size_t audioBatch(const int16_t* data, std::size_t frames);
    static void inputPoll();
    static int16_t inputState(unsigned port, unsigned device, unsigned index, unsigned id);
};
