#include "UnityCaptureWriter.h"

#include <cstring>

UnityCaptureWriter::UnityCaptureWriter(int captureNumber)
    : m_capNum(captureNumber)
{
}

UnityCaptureWriter::~UnityCaptureWriter()
{
    if (m_sharedBuf) UnmapViewOfFile(m_sharedBuf);
    if (m_hSharedFile) CloseHandle(m_hSharedFile);
    if (m_hSentFrameEvent) CloseHandle(m_hSentFrameEvent);
    if (m_hWantFrameEvent) CloseHandle(m_hWantFrameEvent);
    if (m_hMutex) CloseHandle(m_hMutex);
}

bool UnityCaptureWriter::open()
{
    if (m_sharedBuf)
        return true; // already open

    // UnityCapture supports capture numbers 0-25 ('z'-'0'); CapNum 0 uses a
    // NUL terminator in place of the suffix digit for compatibility with the
    // original single-camera filter, which is exactly what shared.inl does.
    constexpr int kMaxCapNum = 'z' - '0';
    int capNum = m_capNum;
    if (capNum > kMaxCapNum) capNum = kMaxCapNum;
    const char suffix = capNum ? char('0' + capNum) : '\0';

    char nameMutex[]      = "UnityCapture_Mutx0";
    char nameEventWant[]  = "UnityCapture_Want0";
    char nameEventSent[]  = "UnityCapture_Sent0";
    char nameSharedData[] = "UnityCapture_Data0";
    nameMutex[sizeof(nameMutex) - 2]           = suffix;
    nameEventWant[sizeof(nameEventWant) - 2]   = suffix;
    nameEventSent[sizeof(nameEventSent) - 2]   = suffix;
    nameSharedData[sizeof(nameSharedData) - 2] = suffix;

    // We are always the sender: open existing objects, never create them —
    // the receiving DirectShow filter creates them once a consumer has it open.
    if (!m_hMutex) {
        m_hMutex = OpenMutexA(SYNCHRONIZE, FALSE, nameMutex);
        if (!m_hMutex) return false;
    }

    WaitForSingleObject(m_hMutex, INFINITE);
    struct UnlockAtReturn { HANDLE h; ~UnlockAtReturn() { ReleaseMutex(h); } } unlock{m_hMutex};

    if (!m_hWantFrameEvent) {
        m_hWantFrameEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, nameEventWant);
        if (!m_hWantFrameEvent) return false;
    }
    if (!m_hSentFrameEvent) {
        m_hSentFrameEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, nameEventSent);
        if (!m_hSentFrameEvent) return false;
    }
    if (!m_hSharedFile) {
        m_hSharedFile = OpenFileMappingA(FILE_MAP_WRITE, FALSE, nameSharedData);
        if (!m_hSharedFile) return false;
    }

    m_sharedBuf = static_cast<SharedMemHeader *>(MapViewOfFile(m_hSharedFile, FILE_MAP_WRITE, 0, 0, 0));
    return m_sharedBuf != nullptr;
}

bool UnityCaptureWriter::isReady()
{
    return open();
}

bool UnityCaptureWriter::sendFrame(int width, int height, const uint8_t *rgba8Data)
{
    if (!rgba8Data || width <= 0 || height <= 0)
        return false;
    if (!open())
        return false;

    const DWORD stride = static_cast<DWORD>(width); // pixel stride (tightly packed, no row padding)
    const DWORD dataSize = static_cast<DWORD>(width) * static_cast<DWORD>(height) * 4u;
    if (m_sharedBuf->maxSize < dataSize)
        return false; // frame too large for the shared buffer (shouldn't happen at our sizes)

    WaitForSingleObject(m_hMutex, INFINITE);
    m_sharedBuf->width = width;
    m_sharedBuf->height = height;
    m_sharedBuf->stride = static_cast<int>(stride);
    m_sharedBuf->format = FormatUInt8;
    m_sharedBuf->resizemode = ResizeLinear;
    m_sharedBuf->mirrormode = MirrorDisabled;
    m_sharedBuf->timeout = 0;
    memcpy(m_sharedBuf->data, rgba8Data, dataSize);
    ReleaseMutex(m_hMutex);

    SetEvent(m_hSentFrameEvent);
    return true;
}
