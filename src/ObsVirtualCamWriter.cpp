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

    // CreateFileMappingW attaches to the existing mapping if one is already
    // open under this name (e.g. Zoom's/OBS's own DirectShow filter reading
    // it as a *consumer*) rather than failing -- consumers are expected to
    // already have it open while idle/showing the placeholder, and this is
    // the same physical shared memory they're reading, so re-initializing
    // the header here is exactly how a new producer takes over and starts
    // feeding them real frames. (A prior pre-check here treated any open
    // handle -- including a mere reader -- as "another producer running"
    // and refused to start, which is why frames were never sent even though
    // Zoom successfully selected the device: Zoom's own filter opening the
    // mapping to read was mistaken for a competing producer.)
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
    // 100ns units, matching DirectShow REFERENCE_TIME (obs-studio computes
    // this the same way: fps_den * 10000000 / fps_num).
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
    // Matches the filter's own fallback path exactly (virtualcam-filter.cpp):
    // SHGetFolderPathW(CSIDL_APPDATA) + "\obs-virtualcam.txt", content
    // "%u x %u x %llu" (cx, cy, interval) parsed with sscanf on their side.
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

    // Lock-free: advance write_idx first (also the slot selector), fill
    // that slot's timestamp + frame data, then publish by setting
    // read_idx = write_idx last -- a reader never observes a slot's new
    // read_idx before the data in it has been fully written.
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
