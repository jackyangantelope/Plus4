
#include "profile050_sampler.h"
#include "profile030_common.h"
#include "plus4_log045.h"

#include <cstdint>
#include <cstring>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/exception.h"
#include "hardware/structs/systick.h"
#include "hardware/structs/scb.h"
#include "hardware/sync.h"

namespace {
constexpr uint32_t kPeriodUs = 2000u;
constexpr uint32_t kBinBytes = 512u;
constexpr unsigned kBinSlots = 1024u;
#if PLUS4_PROFILE052

constexpr unsigned kTopBins = 16u;
#else
constexpr unsigned kTopBins = 32u;
#endif
Plus4Profile030::Histogram histogram;
Plus4Profile030::Slot bins[kBinSlots];
uint32_t ticksPerUs, randomState, handlerUs, maxHandlerUs, unsupported;
#if PLUS4_PROFILE052

constexpr uint32_t kFineBinBytes = 64u;
constexpr unsigned kFineBins = 16u;
uint32_t fineStart;
extern "C" void plus4_profile052_run_symbol() asm("_ZN5Plus47TED73603runEi");
#endif
}

extern "C" void __not_in_flash_func(plus4_profile050_capture)(const uint32_t* frame, uint32_t lr) {
    const uint32_t begin = time_us_32();
    const uintptr_t sp = reinterpret_cast<uintptr_t>(frame);
    if (!Plus4Profile030::Histogram::supportedReturn(lr) ||
        (sp & 3u) || sp < 0x20000000u || sp > 0x20082000u - 32u) {
        ++unsupported;
    } else {
        histogram.record(frame[6], frame[7]);
    }
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    const uint32_t nextUs = kPeriodUs - 127u + (randomState & 255u);
    systick_hw->rvr = ticksPerUs * nextUs - 1u;
    const uint32_t used = time_us_32() - begin;
    handlerUs += used;
    if (used > maxHandlerUs) maxHandlerUs = used;
}

extern "C" __attribute__((naked)) void __not_in_flash_func(plus4_profile050_systick)() {
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b plus4_profile050_capture\n");
}

void plus4_profile050_init() {
    if (systick_hw->csr & 3u) panic("Plus4 PC profiler: SysTick already in use");
    ticksPerUs = clock_get_hz(clk_sys) / 1000000u;
    if (ticksPerUs != 480u) panic("Plus4 PC profiler expects 480 MHz");
    exception_set_exclusive_handler(SYSTICK_EXCEPTION, plus4_profile050_systick);
    exception_set_priority(SYSTICK_EXCEPTION, 0);
#if PLUS4_PROFILE052
    plus4_log045_printf("PLUS4_PROFILE050 begin period_us=2000 bin_bytes=512 top_bins=16 scope=manual-game\n");
    const auto runStart = uint32_t(reinterpret_cast<uintptr_t>(&plus4_profile052_run_symbol)) & ~1u;
    if (runStart < 0x10080000u || runStart >= 0x11000000u)
        panic("Plus4 fine profiler: TED run outside XIP");
    fineStart = runStart & ~(kBinBytes - 1u);
    plus4_log045_printf("PLUS4_PROFILE052 begin run=%08lx focus=%08lx..%08lx bin_bytes=64 bins=16 scope=manual-game\n",
        (unsigned long)runStart, (unsigned long)fineStart,
        (unsigned long)(fineStart + kFineBins * kFineBinBytes));
#else
    plus4_log045_printf("PLUS4_PROFILE050 begin period_us=2000 bin_bytes=512 top_bins=32 scope=manual-game\n");
#endif
}

void plus4_profile050_start() {
    plus4_profile050_stop();
    std::memset(&histogram, 0, sizeof(histogram));
    handlerUs = maxHandlerUs = unsupported = 0;
    randomState = 0x6d2b79f5;
    systick_hw->rvr = ticksPerUs * kPeriodUs - 1u;
    systick_hw->cvr = 0;
    systick_hw->csr = 7;
}

void plus4_profile050_stop() {
    const uint32_t state = save_and_disable_interrupts();
    systick_hw->csr = 0;
    scb_hw->icsr = 1u << 25;
    __dsb();
    __isb();
    restore_interrupts(state);
}

void plus4_profile050_report(unsigned window) {

    std::memset(bins, 0, sizeof(bins));
    unsigned occupied = 0, overflow = 0;
    for (const auto& slot : histogram.slots) {
        if (!slot.key) continue;
        const uint32_t key = (slot.key & ~(kBinBytes - 1u)) | (slot.key & 1u);
        unsigned index = ((key >> 9) * 2654435761u) & (kBinSlots - 1u);
        bool inserted = false;
        for (unsigned n = 0; n < kBinSlots; ++n) {
            auto& bin = bins[index];
            if (!bin.count || bin.key == key) {
                if (!bin.count) { bin.key = key; ++occupied; }
                bin.count += slot.count;
                inserted = true;
                break;
            }
            index = (index + 1u) & (kBinSlots - 1u);
        }
        if (!inserted) overflow += slot.count;
    }
    plus4_log045_printf("PLUS4_PC050_BEGIN window=%u period_us=2000 bin_bytes=512 total=%lu dropped=%lu invalid=%lu unsupported=%lu irq=%lu handler_body_us=%lu handler_max_us=%lu bins=%u bin_overflow=%u\n",
        window, (unsigned long)histogram.total, (unsigned long)histogram.dropped,
        (unsigned long)histogram.invalid, (unsigned long)unsupported,
        (unsigned long)histogram.inIRQ, (unsigned long)handlerUs,
        (unsigned long)maxHandlerUs, occupied, overflow);
    uint32_t shown = 0;
    for (unsigned rank = 0; rank < kTopBins; rank += 8) {
        uint32_t keys[8]{}, counts[8]{};
        unsigned entries = 0;
        for (unsigned j = 0; j < 8; ++j) {
            unsigned best = kBinSlots;
            for (unsigned i = 0; i < kBinSlots; ++i)
                if (bins[i].count && (best == kBinSlots || bins[i].count > bins[best].count)) best = i;
            if (best == kBinSlots) break;
            keys[j] = bins[best].key;
            counts[j] = bins[best].count;
            shown += counts[j];
            bins[best].count = 0;
            ++entries;
        }
        if (!entries) break;
        plus4_log045_printf("PLUS4_PC050_BINS window=%u first=%u entries=%u %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu\n",
            window, rank, entries,
            (unsigned long)keys[0], (unsigned long)counts[0],
            (unsigned long)keys[1], (unsigned long)counts[1],
            (unsigned long)keys[2], (unsigned long)counts[2],
            (unsigned long)keys[3], (unsigned long)counts[3],
            (unsigned long)keys[4], (unsigned long)counts[4],
            (unsigned long)keys[5], (unsigned long)counts[5],
            (unsigned long)keys[6], (unsigned long)counts[6],
            (unsigned long)keys[7], (unsigned long)counts[7]);
    }
    plus4_log045_printf("PLUS4_PC050_END window=%u shown=%lu other=%lu\n",
        window, (unsigned long)shown,
        (unsigned long)(histogram.total >= shown ? histogram.total - shown : 0));
#if PLUS4_PROFILE052

    uint32_t fineCounts[kFineBins]{};
    uint32_t focusSamples = 0;
    const uint32_t fineEnd = fineStart + kFineBins * kFineBinBytes;
    for (const auto& slot : histogram.slots) {
        if (!slot.key || (slot.key & 1u)) continue;
        const uint32_t pc = slot.key & ~1u;
        if (pc < fineStart || pc >= fineEnd) continue;
        fineCounts[(pc - fineStart) / kFineBinBytes] += slot.count;
        focusSamples += slot.count;
    }
    plus4_log045_printf("PLUS4_PC052_BEGIN window=%u run=%08lx focus=%08lx end=%08lx bin_bytes=64 bins=16 focus_samples=%lu total=%lu dropped=%lu invalid=%lu unsupported=%lu\n",
        window, (unsigned long)(uint32_t(reinterpret_cast<uintptr_t>(&plus4_profile052_run_symbol)) & ~1u),
        (unsigned long)fineStart, (unsigned long)fineEnd,
        (unsigned long)focusSamples, (unsigned long)histogram.total,
        (unsigned long)histogram.dropped, (unsigned long)histogram.invalid,
        (unsigned long)unsupported);
    for (unsigned first = 0; first < kFineBins; first += 8) {
        plus4_log045_printf("PLUS4_PC052_BINS window=%u first=%u entries=8 %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu %08lx:%lu\n",
            window, first,
            (unsigned long)(fineStart + (first + 0u) * kFineBinBytes), (unsigned long)fineCounts[first + 0u],
            (unsigned long)(fineStart + (first + 1u) * kFineBinBytes), (unsigned long)fineCounts[first + 1u],
            (unsigned long)(fineStart + (first + 2u) * kFineBinBytes), (unsigned long)fineCounts[first + 2u],
            (unsigned long)(fineStart + (first + 3u) * kFineBinBytes), (unsigned long)fineCounts[first + 3u],
            (unsigned long)(fineStart + (first + 4u) * kFineBinBytes), (unsigned long)fineCounts[first + 4u],
            (unsigned long)(fineStart + (first + 5u) * kFineBinBytes), (unsigned long)fineCounts[first + 5u],
            (unsigned long)(fineStart + (first + 6u) * kFineBinBytes), (unsigned long)fineCounts[first + 6u],
            (unsigned long)(fineStart + (first + 7u) * kFineBinBytes), (unsigned long)fineCounts[first + 7u]);
    }
    plus4_log045_printf("PLUS4_PC052_END window=%u shown=%lu outside=%lu\n",
        window, (unsigned long)focusSamples,
        (unsigned long)(histogram.total - focusSamples));
#endif
}
