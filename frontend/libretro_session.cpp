#include "libretro_session.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <cmath>
#include <string>

#include <SDL.h>
#include <dlfcn.h>
#if defined(__ANDROID__)
#include <SDL_system.h>
#include <jni.h>
#endif

namespace {
#if defined(__ANDROID__)
std::string androidNativeLibraryPath(const std::string& libraryName) {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) return libraryName;

    std::string directory;
    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getApplicationInfo = activityClass
        ? env->GetMethodID(activityClass, "getApplicationInfo",
                           "()Landroid/content/pm/ApplicationInfo;")
        : nullptr;
    jobject applicationInfo = getApplicationInfo
        ? env->CallObjectMethod(activity, getApplicationInfo)
        : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        applicationInfo = nullptr;
    }
    jclass infoClass = applicationInfo ? env->GetObjectClass(applicationInfo) : nullptr;
    jfieldID nativeLibraryDir = infoClass
        ? env->GetFieldID(infoClass, "nativeLibraryDir", "Ljava/lang/String;")
        : nullptr;
    jstring path = nativeLibraryDir
        ? static_cast<jstring>(env->GetObjectField(applicationInfo, nativeLibraryDir))
        : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (path != nullptr) {
        const char* chars = env->GetStringUTFChars(path, nullptr);
        if (chars != nullptr) {
            directory = chars;
            env->ReleaseStringUTFChars(path, chars);
        }
        env->DeleteLocalRef(path);
    }
    if (infoClass) env->DeleteLocalRef(infoClass);
    if (applicationInfo) env->DeleteLocalRef(applicationInfo);
    if (activityClass) env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);

    return directory.empty() ? libraryName : directory + "/" + libraryName;
}
#endif

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
    CORE_FN(serializeSize, std::size_t(*)());
    CORE_FN(serialize, bool(*)(void*, std::size_t));
    CORE_FN(unserialize, bool(*)(const void*, std::size_t));

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
            bind(h, getMemorySize, "retro_get_memory_size") &&
            bind(h, serializeSize, "retro_serialize_size") &&
            bind(h, serialize, "retro_serialize") &&
            bind(h, unserialize, "retro_unserialize");
    }
};
#undef CORE_FN

static CoreApi api;
LibretroSession* LibretroSession::active = nullptr;

LibretroSession::LibretroSession() = default;
LibretroSession::~LibretroSession() { unload(); }

bool LibretroSession::loadRom(const std::string& corePath, const std::string& romPath) {
    unload();
#if defined(__ANDROID__)
    const std::string resolvedCorePath = corePath.find('/') == std::string::npos
        ? androidNativeLibraryPath(corePath) : corePath;
#else
    const std::string& resolvedCorePath = corePath;
#endif
    (void)dlerror();
    library = dlopen(resolvedCorePath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        const char* loaderError = dlerror();
        message = std::string("Unable to load Snes9x core at ") + resolvedCorePath +
            ": " + (loaderError ? loaderError : "unknown dynamic loader error");
        return false;
    }
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
    api.setControllerPortDevice(0, kDeviceJoypad);
    api.setControllerPortDevice(1, kDeviceJoypad);

    SystemInfo info{};
    api.getSystemInfo(&info);
    GameInfo game{romPath.c_str(), info.needFullpath ? nullptr : rom.data(),
                  info.needFullpath ? 0u : rom.size(), nullptr};
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

    const int sampleRate = std::isfinite(av.timing.sampleRate) &&
        av.timing.sampleRate >= 8000.0 && av.timing.sampleRate <= 192000.0
        ? static_cast<int>(std::lround(av.timing.sampleRate)) : 32040;
    SDL_AudioSpec desired{};
    desired.freq = sampleRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = nullptr; // Use SDL's thread-safe queued-audio interface.
    SDL_AudioSpec obtained{};
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained,
                                      SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audioDevice != 0) {
        audioFrequency = obtained.freq > 0 ? obtained.freq : sampleRate;
        SDL_PauseAudioDevice(audioDevice, 0);
    } else {
        audioFrequency = 0;
    }

    message = std::string("Snes9x ") + (info.version ? info.version : "loaded") +
        (audioDevice != 0 ? " | AUDIO ON" : " | AUDIO UNAVAILABLE");
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
    if (audioDevice) {
        SDL_ClearQueuedAudio(audioDevice);
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
    audioFrequency = 0;
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

bool LibretroSession::saveState(const std::string& path) {
    if (!gameLoaded || active != this || !api.serializeSize || !api.serialize) {
        message = "Save states are unavailable until a Snes9x game is loaded.";
        return false;
    }
    const std::size_t size = api.serializeSize();
    if (size == 0 || size > 128u * 1024u * 1024u) {
        message = "Snes9x returned an invalid save-state size.";
        return false;
    }
    std::vector<uint8_t> state(size);
    if (!api.serialize(state.data(), state.size())) {
        message = "Snes9x could not serialize the current game state.";
        return false;
    }

    const std::string temporaryPath = path + ".tmp";
    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            message = "Unable to create the temporary save-state file.";
            return false;
        }
        output.write(reinterpret_cast<const char*>(state.data()),
                     static_cast<std::streamsize>(state.size()));
        if (!output.good()) {
            output.close();
            std::remove(temporaryPath.c_str());
            message = "Writing the save-state file failed.";
            return false;
        }
    }

    if (std::rename(temporaryPath.c_str(), path.c_str()) != 0) {
        std::remove(temporaryPath.c_str());
        message = "Unable to replace the save-state file.";
        return false;
    }
    message = "Save state written successfully.";
    return true;
}

bool LibretroSession::loadState(const std::string& path) {
    if (!gameLoaded || active != this || !api.serializeSize || !api.unserialize) {
        message = "Load states are unavailable until a Snes9x game is loaded.";
        return false;
    }
    const std::size_t expectedSize = api.serializeSize();
    if (expectedSize == 0 || expectedSize > 128u * 1024u * 1024u) {
        message = "Snes9x returned an invalid save-state size.";
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        message = "No save state exists in this slot yet.";
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff fileSize = input.tellg();
    if (fileSize < 0 || static_cast<uint64_t>(fileSize) != expectedSize) {
        message = "Save-state file size does not match the active Snes9x core.";
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::vector<uint8_t> state(expectedSize);
    input.read(reinterpret_cast<char*>(state.data()),
               static_cast<std::streamsize>(state.size()));
    if (!input) {
        message = "Unable to read the complete save-state file.";
        return false;
    }
    if (!api.unserialize(state.data(), state.size())) {
        message = "Snes9x rejected this save-state file.";
        return false;
    }
    message = "Save state restored successfully.";
    return true;
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
    // Libretro audio callbacks are interleaved stereo signed-16 samples.
    // Keep at most ~250 ms queued so slow rendering cannot build unbounded
    // latency or memory use. Dropped excess frames are considered consumed.
    if (data == nullptr || frames == 0 || active == nullptr ||
        active->audioDevice == 0 || active->audioFrequency <= 0) {
        return frames;
    }

    constexpr std::size_t kBytesPerFrame = 2u * sizeof(int16_t);
    const uint64_t maxQueuedBytes64 =
        static_cast<uint64_t>(active->audioFrequency) * kBytesPerFrame / 4u;
    const Uint32 maxQueuedBytes = static_cast<Uint32>(
        std::min<uint64_t>(maxQueuedBytes64, std::numeric_limits<Uint32>::max()));
    const Uint32 queuedBytes = SDL_GetQueuedAudioSize(active->audioDevice);
    if (queuedBytes >= maxQueuedBytes) return frames;

    const Uint32 availableBytes = maxQueuedBytes - queuedBytes;
    const std::size_t framesToQueue = std::min(
        frames, static_cast<std::size_t>(availableBytes / kBytesPerFrame));
    if (framesToQueue == 0) return frames;

    const std::size_t bytesToQueue = framesToQueue * kBytesPerFrame;
    if (bytesToQueue > std::numeric_limits<Uint32>::max()) return frames;
    if (SDL_QueueAudio(active->audioDevice, data,
                       static_cast<Uint32>(bytesToQueue)) != 0) {
        active->message = "Snes9x audio queue failed: " + std::string(SDL_GetError());
    }
    return frames;
}
void LibretroSession::inputPoll() {}
int16_t LibretroSession::inputState(unsigned port, unsigned device, unsigned, unsigned id) {
    if (!active || port >= 2 || device != kDeviceJoypad || id > kJoyR) return 0;
    // ControllerMapper uses the same SNES 12-button bit ordering as RetroPad.
    return (active->pads[port] & (1u << id)) ? 1 : 0;
}
