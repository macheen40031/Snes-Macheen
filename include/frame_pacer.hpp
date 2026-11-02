#pragma once

#include <cstddef>
#include <cstdint>

// Host presentation pacing only. It does not replace the cycle-accurate
// CPU/PPU scheduler that a complete SNES implementation will eventually need.
enum class VideoStandard : uint8_t { Ntsc, Pal };

class FramePacer {
public:
    static constexpr uint64_t NTSC_FRAME_NS = 16639267ull; // ~60.0988 Hz
    static constexpr uint64_t PAL_FRAME_NS = 19997200ull;  // ~50.0070 Hz
    static constexpr std::size_t DEFAULT_MAX_CATCH_UP = 3;

    explicit FramePacer(VideoStandard standard = VideoStandard::Ntsc,
                        std::size_t maxCatchUpFrames = DEFAULT_MAX_CATCH_UP) noexcept;

    // Adds elapsed host time and returns the number of presentation frames due.
    // Large delays are bounded; excess whole frames are counted as dropped.
    std::size_t advance(uint64_t elapsedNanoseconds) noexcept;
    uint32_t delayMilliseconds() const noexcept;
    uint64_t frameIntervalNanoseconds() const noexcept;
    uint64_t droppedFrames() const noexcept;
    uint64_t fractionalRemainderNanoseconds() const noexcept;
    void setVideoStandard(VideoStandard standard) noexcept;
    void reset() noexcept;

private:
    uint64_t frameInterval = NTSC_FRAME_NS;
    uint64_t accumulatedNanoseconds = 0;
    uint64_t dropped = 0;
    std::size_t maxCatchUp = DEFAULT_MAX_CATCH_UP;
};
