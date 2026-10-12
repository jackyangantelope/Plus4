
#pragma once
#include <cstdint>

namespace Plus4Profile030 {
constexpr unsigned kWarmup = 600, kEnd = 1200;
inline uint16_t buttons(unsigned f) {
    if ((f >= 300 && f < 306) || (f >= 410 && f < 416) ||
        (f >= 470 && f < 476)) return 1;
    if (f >= 375 && f < 381) return 128;
    if (f < kWarmup) return 0;

    const unsigned step = (f - kWarmup) % 160;
    if (step < 24) return 128;
    if (step >= 40 && step < 64) return 32;
    if (step >= 80 && step < 104) return 64;
    if (step >= 120 && step < 144) return 16;
    return 0;
}
constexpr unsigned kSlots = 4096, kProbes = 32;
struct Slot { uint32_t key = 0, count = 0; };
struct Histogram {
    Slot slots[kSlots]{};
    uint32_t total = 0, dropped = 0, invalid = 0, inIRQ = 0;

    static inline bool supportedReturn(uint32_t lr) {
        return (lr & 0xffffff80u) == 0xffffff80u && (lr & 0x61u) == 0x61u;
    }
    inline void record(uint32_t pc, uint32_t xpsr) {
        ++total;
        if (!(xpsr & (1u << 24)) ||
            !((pc >= 0x10080000u && pc < 0x11000000u) ||
              (pc >= 0x20000000u && pc < 0x20082000u) ||
              (pc >= 2 && pc < 0x8000u))) { ++invalid; return; }
        const uint32_t irq = (xpsr & 0x1ffu) != 0;
        inIRQ += irq;
        const uint32_t key = (pc & ~1u) | irq;
        unsigned index = ((pc >> 1) * 2654435761u) & (kSlots - 1);
        for (unsigned n = 0; n < kProbes; ++n) {
            Slot& s = slots[index];
            if (!s.key || s.key == key) { s.key = key; ++s.count; return; }
            index = (index + 1) & (kSlots - 1);
        }
        ++dropped;
    }
};
}
