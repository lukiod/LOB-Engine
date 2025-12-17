#pragma once

#include <string>
#include <stdexcept>
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace LOB {

class MemoryMappedFile {
public:
    MemoryMappedFile(const std::string& path) {
#ifdef _WIN32
        fileHandle_ = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fileHandle_ == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open file: " + path);
        }

        LARGE_INTEGER size;
        if (!GetFileSizeEx(fileHandle_, &size)) {
            CloseHandle(fileHandle_);
            throw std::runtime_error("Failed to get file size");
        }
        size_ = static_cast<size_t>(size.QuadPart);

        mappingHandle_ = CreateFileMappingA(fileHandle_, NULL, PAGE_READONLY, 0, 0, NULL);
        if (mappingHandle_ == NULL) {
            CloseHandle(fileHandle_);
            throw std::runtime_error("Failed to create file mapping");
        }

        data_ = static_cast<char*>(MapViewOfFile(mappingHandle_, FILE_MAP_READ, 0, 0, 0));
        if (data_ == NULL) {
            CloseHandle(mappingHandle_);
            CloseHandle(fileHandle_);
            throw std::runtime_error("Failed to map view of file");
        }
#else
        fd_ = open(path.c_str(), O_RDONLY);
        if (fd_ == -1) {
            throw std::runtime_error("Failed to open file: " + path);
        }

        struct stat sb;
        if (fstat(fd_, &sb) == -1) {
            close(fd_);
            throw std::runtime_error("Failed to get file size");
        }
        size_ = static_cast<size_t>(sb.st_size);

        data_ = static_cast<char*>(mmap(NULL, size_, PROT_READ, MAP_PRIVATE, fd_, 0));
        if (data_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("Failed to mmap file");
        }
#endif
    }

    ~MemoryMappedFile() {
#ifdef _WIN32
        if (data_) UnmapViewOfFile(data_);
        if (mappingHandle_) CloseHandle(mappingHandle_);
        if (fileHandle_ != INVALID_HANDLE_VALUE) CloseHandle(fileHandle_);
#else
        if (data_ != MAP_FAILED) munmap(data_, size_);
        if (fd_ != -1) close(fd_);
#endif
    }

    const char* data() const { return data_; }
    size_t size() const { return size_; }

private:
#ifdef _WIN32
    HANDLE fileHandle_;
    HANDLE mappingHandle_;
    char* data_;
#else
    int fd_;
    char* data_;
#endif
    size_t size_;
};

}
