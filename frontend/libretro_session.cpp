#include "libretro_session.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>

#include <SDL.h>
#if defined(__ANDROID__)
#include <dlfcn.h>
#else
#include <dlfcn.h>
#endif

namespace {
constexpr unsigned kApiVersion = 1;
constexpr unsigned kEnvSetPixelFormat = 10;
constexpr unsigned kPixelXrgb8888 = 1;
constexpr unsigned kPixelRgb565 = 2;
constexpr unsigned kDeviceJoypad = 1;
constexpr unsigned kJoyB = 0, kJoyY = 1, kJoySelect = 2, kJoyStart = 3;
constexpr unsigned kJoyUp = 4, kJoyDown = 5, kJoyLeft = 6, kJoyRight = 7;
constexpr unsigned kJoyA = 8, kJoyX = 9, kJoyL = 10, kJoyR = 11;
struct GameInfo { const char* path; const void* data; std::size_t size; const char* meta; };
struct SystemInfo { const char* name; const char* version; const char* extensions; bool needFullpath; bool blockExtract; };
struct Geometry { unsigned baseWidth, baseHeight, maxWidth, maxHeight; float aspectRatio; };
struct Timing { double fps, sampleRate; };
struct AvInfo { Geometry geometry; Timing timing; };
using EnvironmentFn = bool(*)(unsigned, void*);
using VideoFn = void(*)(const void*, unsigned, unsigned, std::size_t);
using AudioFn = void(*)(int16_t, int16_t);
using AudioBatchFn = std::size_t(*)(const int16_t*, std::size_t);
using InputPollFn = void(*)();
using InputStateFn = int16_t(*)(unsigned, unsigned, unsigned, unsigned);
}

#define CORE_FN(name, signature) using name##Fn = signature; name##Fn name = nullptr
class CoreApi {
public:
    CORE_FN(apiVersion, unsigned(*)());
    CORE_FN(setEnvironment, void(*)(EnvironmentFn));
    CORE_FN(setVideo, void(*)(VideoFn));
    CORE_FN(setAudio, void(*)(AudioFn));
    CORE_FN(setAudioBatch, void(*)(AudioBatchFn));
    CORE_FN(setInputPoll, void(*)(InputPollFn));
    CORE_FN(setInputState, void(*)(InputStateFn));
    CORE_FN(setControllerPortDevice, void(*)(unsigned, unsigned));
    CORE_FN(init, void(*)());
    CORE_FN(deinit, void(*)());
    CORE_FN(getSystemInfo, void(*)(SystemInfo*));
    CORE_FN(getAvInfo, void(*)(AvInfo*));
    CORE_FN(loadGame, bool(*)(const GameInfo*));
    CORE_FN(unloadGame, void(*)());
    CORE_FN(run, void(*)());
    CORE_FN(reset, void(*)());
    CORE_FN(getMemoryData, void*(*)(unsigned));
    CORE_FN(getMemorySize, std::size_t(*)(unsigned));

    template<class T> bool bind(void* handle, T& target, const char* symbol) {
        target = reinterpret_cast<T>(dlsym(handle, symbol));
        return target != nullptr;
    }
    bool load(void* h) {
        return bind(h, apiVersion, "retro_api_version") &&
            bind(h, setEnvironment, "retro_set_environment") &&
            bind(h, setVideo, "retro_set_video_refresh") &&
            bind(h, setAudio, "retro_set_audio_sample") &&
            bind(h, setAudioBatch, "retro_set_audio_sample_batch") &&
            bind(h, setInputPoll, "retro_set_input_poll") &&
            bind(h, setInputState, "retro_set_input_state") &&
            bind(h, setControllerPortDevice, "retro_set_controller_port_device") &&
            bind(h, init, "retro_init") && bind(h, deinit, "retro_deinit") &&
            bind(h, getSystemInfo, "retro_get_system_info") &&
            bind(h, getAvInfo, "retro_get_system_av_info") &&
            bind(h, loadGame, "retro_load_game") &&
            bind(h, unloadGame, "retro_unload_game") &&
            bind(h, run, "retro_run") && bind(h, reset, "retro_reset") &&
            bind(h, getMemoryData, "retro_get_memory_data") &&
            bind(h, getMemorySize, "retro_get_memory_size");
    }
};
#undef CORE_FN

static CoreApi api;
LibretroSession* LibretroSession::active = nullptr;

LibretroSession::LibretroSession() = default;
LibretroSession::~LibretroSession() { unload(); }

bool LibretroSession::loadRom(const std::string& corePath, const std::string& romPath) {
    unload();
    library = dlopen(corePath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) { message = std::string("Unable to load Snes9x core: ") + dlerror(); return false; }
    if (!api.load(library)) { message = "Snes9x core is missing required Libretro functions."; unload(); return false; }
    if (api.apiVersion() != kApiVersion) { message = "Unsupported Libretro API version."; unload(); return false; }

    std::ifstream file(romPath, std::ios::binary);
    if (!file) { message = "Unable to open selected ROM."; unload(); return false; }
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), {});
    if (rom.empty()) { message = "Selected ROM is empty."; unload(); return false; }

    active = this;
    api.setEnvironment(environment);
    api.setVideo(videoRefresh);
    api.setAudio(audioSample);
    api.setAudioBatch(audioBatch);
    api.setInputPoll(inputPoll);
    api.setInputState(inputState);
    api.init();
    initialized = true;

    SystemInfo info{};
    api.getSystemInfo(&info);
    GameInfo game{romPath.c_str(), rom.data(), rom.size(), nullptr};
    if (!api.loadGame(&game)) {
        message = "Snes9x rejected this ROM format.";
        unload();
        return false;
    }
    gameLoaded = true;
    // Restore the cartridge battery RAM sidecar when one exists.
    constexpr unsigned kSaveRam = 0;
    const auto saveSize = api.getMemorySize(kSaveRam);
    auto* saveData = static_cast<char*>(api.getMemoryData(kSaveRam));
    if (saveData && saveSize > 0) {
        std::ifstream saveFile(romPath + ".srm", std::ios::binary);
        if (saveFile) {
            saveFile.read(saveData, static_cast<std::streamsize>(saveSize));
        }
    }
    AvInfo av{};
    api.getAvInfo(&av);
    frameWidth = std::max(1u, av.geometry.baseWidth);
    frameHeight = std::max(1u, av.geometry.baseHeight);
    message = std::string("Snes9x ") + (info.version ? info.version : "loaded");
    return true;
}

void LibretroSession::unload() noexcept {
    if (gameLoaded && api.unloadGame) api.unloadGame();
    gameLoaded = false;
    if (initialized && api.deinit) api.deinit();
    initialized = false;
    if (library) dlclose(library);
    library = nullptr;
    if (active == this) active = nullptr;
    if (audioDevice) { SDL_CloseAudioDevice(audioDevice); audioDevice = 0; }
    pixels.clear();
}

void LibretroSession::runFrame() { if (gameLoaded && api.run) api.run(); }
void LibretroSession::setControllerState(unsigned player, uint16_t buttons) noexcept {
    if (player < 2) pads[player] = buttons;
}

bool LibretroSession::saveRam(const std::string& path) const {
    if (!gameLoaded || !api.getMemoryData || !api.getMemorySize) return false;
    constexpr unsigned kSaveRam = 0;
    const auto size = api.getMemorySize(kSaveRam);
    const auto* data = static_cast<const char*>(api.getMemoryData(kSaveRam));
    if (!data || size == 0) return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(data, static_cast<std::streamsize>(size));
    return out.good();
}

bool LibretroSession::environment(unsigned command, void* data) {
    if (command == kEnvSetPixelFormat && data) {
        const auto format = *static_cast<unsigned*>(data);
        return format == kPixelXrgb8888;
    }
    return false;
}

void LibretroSession::videoRefresh(const void* data, unsigned width, unsigned height, std::size_t pitch) {
    if (!active || !data || width == 0 || height == 0 || width > 1024 || height > 1024) return;
    active->frameWidth = width;
    active->frameHeight = height;
    active->pixels.resize(static_cast<std::size_t>(width) * height);
    const auto* src = static_cast<const uint8_t*>(data);
    for (unsigned y = 0; y < height; ++y) {
        const auto* row = reinterpret_cast<const uint32_t*>(src + y * pitch);
        for (unsigned x = 0; x < width; ++x) active->pixels[static_cast<std::size_t>(y) * width + x] = row[x] | 0xff000000u;
    }
}
void LibretroSession::audioSample(int16_t left, int16_t right) {
    const int16_t sample[2]{left, right};
    audioBatch(sample, 1);
}
std::size_t LibretroSession::audioBatch(const int16_t* data, std::size_t frames) {
    // Audio device initialization is wired in the next audio milestone.
    (void)data;
    return frames;
}
void LibretroSession::inputPoll() {}
int16_t LibretroSession::inputState(unsigned port, unsigned device, unsigned, unsigned id) {
    if (!active || port >= 2 || device != kDeviceJoypad || id > kJoyR) return 0;
    // ControllerMapper uses the same SNES 12-button bit ordering as RetroPad.
    return (active->pads[port] & (1u << id)) ? 1 : 0;
}
