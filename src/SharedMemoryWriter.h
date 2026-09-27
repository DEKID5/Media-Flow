#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

class SharedMemoryWriter {
public:
    SharedMemoryWriter(const std::wstring& name, size_t size);
    ~SharedMemoryWriter();

    bool Write(const void* data, size_t size);
    bool IsValid() const;
    void* GetBuffer() const;

private:
    HANDLE hMapFile;
    void* pBuf;
    size_t memSize;
};
