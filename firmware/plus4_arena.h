
#pragma once
#include <cstddef>
#include <cstdint>

class Plus4Arena {
 public:
    Plus4Arena(uint8_t* cold, std::size_t capacity,
               uint8_t* hot = nullptr, std::size_t hotCapacity = 0)
        : cold_(cold), capacity_(capacity), hot_(hot),
          hotCapacity_(hot == nullptr ? 0 : hotCapacity) {}
    void reset() { used_ = 0; }
    std::size_t used() const { return used_; }
    void* allocate(std::size_t size) {
        constexpr std::size_t alignment = alignof(std::max_align_t);
        const std::size_t padding = (alignment - (used_ & (alignment - 1))) & (alignment - 1);
        if (used_ > capacity_ || padding > capacity_ - used_)
            return nullptr;
        const std::size_t start = used_ + padding;
        if (size == 0) size = 1;
        if (start > capacity_ || size > capacity_ - start) return nullptr;
        uint8_t* backing = cold_;
        // Hot and cold buffers share one logical address range.
        if (start < hotCapacity_) {
            if (size > hotCapacity_ - start) return nullptr;
            backing = hot_;
        }
        used_ = start + size;
        return backing + start;
    }
 private:
    uint8_t* cold_;
    std::size_t capacity_;
    uint8_t* hot_;
    std::size_t hotCapacity_;
    std::size_t used_ = 0;
};
