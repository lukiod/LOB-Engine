#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <memory>

namespace LOB {

template <typename T, size_t BlockSize = 10000>
class SlabAllocator {
public:
    SlabAllocator(size_t initialCapacity = 1000000) {
        // Pre-allocate to avoid resizing during trading day
        size_t blocksNeeded = (initialCapacity + BlockSize - 1) / BlockSize;
        for (size_t i = 0; i < blocksNeeded; ++i) {
            allocateBlock();
        }
    }

    ~SlabAllocator() {
        for (void* block : blocks_) {
            ::operator delete(block);
        }
    }

    T* allocate() {
        if (freeList_ == nullptr) {
            allocateBlock();
        }
        
        T* object = freeList_;
        freeList_ = freeList_->next; // Using 'next' pointer from the object itself (union/reinterpret_cast pattern)
        
        // Use placement new to call constructor if needed, or assume manual construction.
        // For POD/trivial types like Order, we might just call a reset method.
        // Here we just return the raw ptr, user calls placement new or init.
        return object;
    }

    void deallocate(T* object) {
        if (!object) return;
        
        // We assume T has a 'next' pointer or sufficient size to store a pointer.
        // For Order struct, it has 'next'. We reuse that field for the free list.
        object->next = freeList_;
        freeList_ = object;
    }

private:
    std::vector<void*> blocks_;
    T* freeList_ = nullptr;

    void allocateBlock() {
        // Allocate raw memory for BlockSize items
        size_t sizeBytes = sizeof(T) * BlockSize;
        // Ensure alignment
        void* rawMemory = ::operator new(sizeBytes); 
        blocks_.push_back(rawMemory);

        T* start = static_cast<T*>(rawMemory);
        
        // Link all new objects into the free list
        for (size_t i = 0; i < BlockSize - 1; ++i) {
            start[i].next = &start[i + 1];
        }
        start[BlockSize - 1].next = freeList_;
        freeList_ = &start[0];
    }
};

} // namespace LOB
