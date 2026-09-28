#pragma once

#include <windows.h>
#include <cstdint>

/**
 * @brief Producer-side implementation of OBS Studio's own virtual-camera
 * shared-memory protocol, matching plugins/win-dshow/shared-memory-queue.c
 * in the real obs-studio source exactly (struct layout, sizing/alignment,
 * write sequence). This lets MediaFlow drive the "OBS Virtual Camera"
 * DirectShow device directly -- the user only needs OBS Studio installed
 * once (which registers the driver), never running.
 *
 * Unlike UnityCapture's protocol, there is no mutex/event synchronization
 * here at all: it's a lock-free single-writer triple-buffer ring (safe on
 * x86/x64's strong store ordering, which is what the real OBS source also
 * relies on). Critically, the *producer* creates the shared memory (unlike
 * UnityCapture, where the sender only opened objects the receiver had
 * already created) -- so this can start immediately without waiting for
 * anything else to run first, and fails cleanly if another producer
 * (e.g. OBS Studio's own "Start Virtual Camera") already holds it.
 */
class ObsVirtualCamWriter
{
public:
    ObsVirtualCamWriter() = default;
    ~ObsVirtualCamWriter();

    ObsVirtualCamWriter(const ObsVirtualCamWriter &) = delete;
    ObsVirtualCamWriter &operator=(const ObsVirtualCamWriter &) = delete;

    /**
     * @brief Creates the shared memory queue at the given resolution/frame
     * rate. Returns false if it's already in use by another producer.
     */
    bool start(int width, int height, double fps);

    /**
     * @brief Sends one frame, already in NV12 (Y plane followed by an
     * interleaved U/V plane, both tightly packed -- no row padding).
     */
    bool writeFrame(const uint8_t *nv12Data);

    void stop();
    bool isActive() const { return m_header != nullptr; }

private:
    // Mirrors virtualcam.c's own startup behavior exactly: the filter's
    // constructor (which runs when Zoom opens "OBS Virtual Camera") only
    // trusts our shared memory's own cx/cy/interval once state == READY,
    // which only happens after our *first* successful frame write -- there's
    // a startup gap before that. Its fallback for that gap is reading this
    // file for a cached resolution; without it, the filter falls through to
    // a built-in default that likely doesn't match what we later create,
    // which is what caused Zoom's outright "Failed to start the video
    // camera" (as opposed to the graceful "not sending data yet" placeholder
    // it shows once dimensions actually match).
    void writeResolutionHintFile(int width, int height, uint64_t interval);

    // Field-for-field identical to obs-studio's queue_header (same types,
    // same order) so MSVC produces the same layout/padding as OBS's own
    // compiler without needing explicit packing pragmas.
    struct QueueHeader
    {
        volatile uint32_t writeIdx;
        volatile uint32_t readIdx;
        volatile uint32_t state;
        uint32_t offsets[3];
        uint32_t type;
        uint32_t cx;
        uint32_t cy;
        uint64_t interval;
        uint32_t reserved[8];
    };

    enum QueueState { StateInvalid = 0, StateStarting = 1, StateReady = 2, StateStopping = 3 };

    static constexpr uint32_t kFrameHeaderSize = 32;
    static uint32_t alignUp32(uint32_t size) { return (size + 31u) & ~31u; }

    HANDLE m_mapping = nullptr;
    QueueHeader *m_header = nullptr;
    uint64_t *m_frameTimestamp[3] = {nullptr, nullptr, nullptr};
    uint8_t *m_framePtr[3] = {nullptr, nullptr, nullptr};
    uint32_t m_frameDataSize = 0; // cx*cy*3/2, per slot
    int m_width = 0;
    int m_height = 0;
};
