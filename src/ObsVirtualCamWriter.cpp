#include "ObsVirtualCamWriter.h"

#include <cstring>
#include <cstdio>
#include <shlobj.h>

namespace {
constexpr wchar_t kVideoQueueName[] = L"OBSVirtualCamVideo";
}

ObsVirtualCamWriter::~ObsVirtualCamWriter()
{
    stop();
}

bool ObsVirtualCamWriter::start(int width, int height, double fps)
{
    if (m_header)
        return true; // already running

    if (width <= 0 || height <= 0 || fps <= 0.0)
        return false;

    const uint32_t cx = static_cast<uint32_t>(width);
    const uint32_t cy = static_cast<uint32_t>(height);
    const uint32_t frameSize = cx * cy * 3u / 2u; // NV12: Y (cx*cy) + interleaved UV (cx*cy/2)

    uint32_t offsetFrame[3];
    uint32_t size = alignUp32(sizeof(QueueHeader));
    for (int i = 0; i < 3; ++i) {
        offsetFrame[i] = size;
        size = alignUp32(size + frameSize + kFrameHeaderSize);
    }

    // Reuse the shared mapping if Zoom or OBS already has it open; that is how the producer handoff works.
    m_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, kVideoQueueName);
    if (!m_mapping)
        return false;

    void *view = MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view) {
        CloseHandle(m_mapping);
        m_mapping = nullptr;
        return false;
    }

    m_header = static_cast<QueueHeader *>(view);
    memset(m_header, 0, sizeof(QueueHeader));
    m_header->state = StateStarting;
    m_header->cx = cx;
    m_header->cy = cy;
    // Use DirectShow-style time units so OBS and Zoom agree on the frame cadence.
    m_header->interval = static_cast<uint64_t>(10000000.0 / fps);
    for (int i = 0; i < 3; ++i)
        m_header->offsets[i] = offsetFrame[i];

    auto *base = reinterpret_cast<uint8_t *>(m_header);
    for (int i = 0; i < 3; ++i) {
        m_frameTimestamp[i] = reinterpret_cast<uint64_t *>(base + offsetFrame[i]);
        m_framePtr[i] = base + offsetFrame[i] + kFrameHeaderSize;
    }

    m_frameDataSize = frameSize;
    m_width = width;
    m_height = height;

    writeResolutionHintFile(width, height, m_header->interval);
    return true;
}

void ObsVirtualCamWriter::writeResolutionHintFile(int width, int height, uint64_t interval)
{
    // Write the same hint file OBS expects so the virtual camera stays in sync.
    wchar_t path[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path)))
        return;
    wcscat_s(path, MAX_PATH, L"\\obs-virtualcam.txt");

    char content[128];
    const int len = snprintf(content, sizeof(content), "%ux%ux%llu",
                              static_cast<unsigned>(width), static_cast<unsigned>(height),
                              static_cast<unsigned long long>(interval));
    if (len <= 0)
        return;

    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(file, content, static_cast<DWORD>(len), &written, nullptr);
    CloseHandle(file);
}

bool ObsVirtualCamWriter::writeFrame(const uint8_t *nv12Data)
{
    if (!m_header || !nv12Data)
        return false;

    // Publish the next frame only after the slot is fully written.
    const uint32_t inc = m_header->writeIdx + 1;
    m_header->writeIdx = inc;
    const uint32_t idx = inc % 3;

    *m_frameTimestamp[idx] = 0; // no meaningful presentation clock to report; 0 is a valid/ignored timestamp
    memcpy(m_framePtr[idx], nv12Data, m_frameDataSize);

    m_header->readIdx = inc;
    m_header->state = StateReady;
    return true;
}

void ObsVirtualCamWriter::stop()
{
    if (!m_header)
        return;

    m_header->state = StateStopping;
    UnmapViewOfFile(m_header);
    m_header = nullptr;

    if (m_mapping) {
        CloseHandle(m_mapping);
        m_mapping = nullptr;
    }

    for (int i = 0; i < 3; ++i) {
        m_frameTimestamp[i] = nullptr;
        m_framePtr[i] = nullptr;
    }
    m_frameDataSize = 0;
}
