
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace plus4_audio {

class Decimate5 {
    int32_t area_ = 0, dc_ = 0;
    uint32_t count_ = 0;
public:
    bool push(int16_t sample, int16_t &out) {
        area_ += sample;
        if (++count_ != 5) return false;
        const int32_t average = area_ / 5;
        area_ = 0; count_ = 0;
        dc_ += (average * 256 - dc_) / 256;
        int32_t ac = average - dc_ / 256;
        if (ac > 32767) ac = 32767;
        if (ac < -32768) ac = -32768;
        out = static_cast<int16_t>(ac);
        return true;
    }
};

template<uint32_t Capacity = 2048> class MonoRing {
    // Cursor subtraction relies on a capacity below 2^31.
    static_assert(Capacity && !(Capacity & (Capacity - 1)), "power of two");
    static_assert(Capacity < 0x80000000u, "cursor distance");
    static_assert(std::atomic<uint32_t>::is_always_lock_free, "lock-free cursors");
    int16_t data_[Capacity]{};
    std::atomic<uint32_t> head_{0}, tail_{0};
public:
    uint32_t push_repeat(int16_t sample, uint32_t count) {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t room = Capacity -
            (head - tail_.load(std::memory_order_acquire));
        const uint32_t keep = count < room ? count : room;
        for (uint32_t i = 0; i < keep; ++i)
            data_[(head + i) & (Capacity - 1)] = sample;
        head_.store(head + keep, std::memory_order_release);
        return keep;
    }
    uint32_t pop(int16_t *out, uint32_t count) {
        const uint32_t tail = tail_.load(std::memory_order_relaxed);
        const uint32_t available = head_.load(std::memory_order_acquire) - tail;
        const uint32_t take = count < available ? count : available;
        for (uint32_t i = 0; i < take; ++i)
            out[i] = data_[(tail + i) & (Capacity - 1)];
        tail_.store(tail + take, std::memory_order_release);
        return take;
    }

    uint32_t producer_level() const {
        return head_.load(std::memory_order_relaxed) -
               tail_.load(std::memory_order_acquire);
    }
    uint32_t consumer_level() const {
        return head_.load(std::memory_order_acquire) -
               tail_.load(std::memory_order_relaxed);
    }
};

class RationalBox {
    uint32_t filled_ = 0;
    int32_t area_ = 0;
public:
    bool push(int16_t sample, int16_t &out) {
        const uint32_t room = 500u - filled_;
        if (room > 441u) {
            area_ += int32_t(sample) * 441;
            filled_ += 441;
            return false;
        }
        area_ += int32_t(sample) * int32_t(room);
        out = int16_t(area_ / 500);
        filled_ = 441u - room;
        area_ = int32_t(sample) * int32_t(filled_);
        return true;
    }
};
}
