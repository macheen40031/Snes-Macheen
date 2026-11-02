#include "frame_pacer.hpp"

#include <algorithm>
#include <limits>

FramePacer::FramePacer(VideoStandard standard, std::size_t maxCatchUpFrames) noexcept
    : maxCatchUp(std::max<std::size_t>(1u, maxCatchUpFrames)) {
    setVideoStandard(standard);
}

std::size_t FramePacer::advance(uint64_t elapsedNanoseconds) noexcept {
    // Bound one update to a modest catch-up window. This prevents an app
    // returning from the background from trying to render hundreds of frames.
    const uint64_t windowFrames = static_cast<uint64_t>(maxCatchUp) * 4u + 1u;
    const uint64_t maxElapsed = frameInterval >
        std::numeric_limits<uint64_t>::max() / windowFrames
        ? std::numeric_limits<uint64_t>::max()
        : frameInterval * windowFrames;

    if (elapsedNanoseconds > maxElapsed) {
        const uint64_t excess = elapsedNanoseconds - maxElapsed;
        const uint64_t excessFrames = excess / frameInterval;
        dropped += excessFrames;
        elapsedNanoseconds = maxElapsed + (excess % frameInterval);
    }

    uint64_t total = accumulatedNanoseconds;
    if (elapsedNanoseconds > std::numeric_limits<uint64_t>::max() - total)
        total = std::numeric_limits<uint64_t>::max();
    else
        total += elapsedNanoseconds;

    const uint64_t due = total / frameInterval;
    if (due > static_cast<uint64_t>(maxCatchUp)) {
        dropped += due - static_cast<uint64_t>(maxCatchUp);
        accumulatedNanoseconds = total % frameInterval;
        return maxCatchUp;
    }

    accumulatedNanoseconds = total - due * frameInterval;
    return static_cast<std::size_t>(due);
}

uint32_t FramePacer::delayMilliseconds() const noexcept {
    if (accumulatedNanoseconds >= frameInterval) return 0u;
    const uint64_t remaining = frameInterval - accumulatedNanoseconds;
    const uint64_t roundedUpMs = (remaining + 999999u) / 1000000u;
    return static_cast<uint32_t>(std::max<uint64_t>(1u, roundedUpMs));
}

uint64_t FramePacer::frameIntervalNanoseconds() const noexcept {
    return frameInterval;
}

uint64_t FramePacer::droppedFrames() const noexcept { return dropped; }

uint64_t FramePacer::fractionalRemainderNanoseconds() const noexcept {
    return accumulatedNanoseconds;
}

void FramePacer::setVideoStandard(VideoStandard standard) noexcept {
    frameInterval = standard == VideoStandard::Pal ? PAL_FRAME_NS : NTSC_FRAME_NS;
    accumulatedNanoseconds = 0;
}

void FramePacer::reset() noexcept {
    accumulatedNanoseconds = 0;
    dropped = 0;
}
