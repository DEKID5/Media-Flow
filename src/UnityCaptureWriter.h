#pragma once

#include <windows.h>
#include <cstdint>

/**
 * @brief Producer-side implementation of the UnityCapture shared-memory
 * protocol (https://github.com/schellingb/UnityCapture), matching that
 * project's Source/shared.inl exactly (named mutex/events/file mapping,
 * SharedMemHeader layout, Send() sequence).
 *
 * UnityCapture is a small MIT-licensed DirectShow virtual camera made for
 * exactly this purpose (an app feeding rendered frames into a virtual
 * webcam other apps like Zoom can select). The user must install its
 * driver once (its Install.bat, which registers the filter via regsvr32) —
 * see BUILD.md.
 *
 * The shared objects are created by the *receiving* side (UnityCapture's
 * DirectShow filter, once something like Zoom has it open as the active
 * camera) — as the sender we only ever *open* existing objects. So until a
 * consumer app is actually using the camera, isReady()/sendFrame() simply
 * report "not ready"; that is the expected idle state, not an error.
 */
class UnityCaptureWriter
{
public:
    enum EFormat { FormatUInt8 = 0, FormatFp16Gamma = 1, FormatFp16Linear = 2 };
    enum EResizeMode { ResizeDisabled = 0, ResizeLinear = 1 };
    enum EMirrorMode { MirrorDisabled = 0, MirrorHorizontal = 1 };

    explicit UnityCaptureWriter(int captureNumber = 0);
    ~UnityCaptureWriter();

    UnityCaptureWriter(const UnityCaptureWriter &) = delete;
    UnityCaptureWriter &operator=(const UnityCaptureWriter &) = delete;

    /**
     * @brief True once a consumer (e.g. Zoom, with UnityCapture selected as
     * its camera) has the shared memory open and ready to receive.
     */
    bool isReady();

    /**
     * @brief Sends one tightly-packed RGBA8 frame (width*height*4 bytes,
     * row-major top-to-bottom, no row padding) — the exact byte layout of
     * QImage::Format_RGBA8888, so callers can memcpy straight out of one.
     * Safe to call even when isReady() would be false; it will just fail
     * quietly (no consumer connected yet).
     */
    bool sendFrame(int width, int height, const uint8_t *rgba8Data);

private:
    // Mirrors UnityCapture's SharedImageMemory::Open(ForReceiving=false).
    bool open();

    // Mirrors UnityCapture's SharedImageMemory::SharedMemHeader exactly —
    // field order/types must match byte-for-byte.
    struct SharedMemHeader
    {
        DWORD maxSize;
        int width;
        int height;
        int stride;
        int format;
        int resizemode;
        int mirrormode;
        int timeout;
        uint8_t data[1];
    };

    // 4K RGBA at 16-bit per channel max, matching UnityCapture's own MAX_SHARED_IMAGE_SIZE.
    static constexpr DWORD kMaxSharedImageSize = 3840u * 2160u * 4u * sizeof(short);

    int m_capNum;
    HANDLE m_hMutex = nullptr;
    HANDLE m_hWantFrameEvent = nullptr;
    HANDLE m_hSentFrameEvent = nullptr;
    HANDLE m_hSharedFile = nullptr;
    SharedMemHeader *m_sharedBuf = nullptr;
};
