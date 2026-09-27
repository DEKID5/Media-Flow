#include "SharedMemoryWriter.h"

SharedMemoryWriter::SharedMemoryWriter(const std::wstring& name, size_t size) 
    : memSize(size), hMapFile(NULL), pBuf(nullptr) {
    
    hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,   // Use paging file
        NULL,                   // Default security
        PAGE_READWRITE,         // Read/write access
        0,                      // Maximum object size (high-order DWORD)
        static_cast<DWORD>(size),// Maximum object size (low-order DWORD)
        name.c_str());          // Name of mapping object

    if (hMapFile == NULL) {
        return;
    }

    pBuf = MapViewOfFile(
        hMapFile,               // Handle to map object
        FILE_MAP_ALL_ACCESS,    // Read/write permission
        0,
        0,
        size);
}

SharedMemoryWriter::~SharedMemoryWriter() {
    if (pBuf) {
        UnmapViewOfFile(pBuf);
    }
    if (hMapFile) {
        CloseHandle(hMapFile);
    }
}

bool SharedMemoryWriter::Write(const void* data, size_t size) {
    if (!pBuf || size > memSize || !data) {
        return false;
    }
    memcpy(pBuf, data, size);
    return true;
}

bool SharedMemoryWriter::IsValid() const {
    return pBuf != nullptr;
}

void* SharedMemoryWriter::GetBuffer() const {
    return pBuf;
}
