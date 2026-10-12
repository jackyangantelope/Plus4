
#include "plus4_audio045.h"
#include "plus4_audio045_pipeline.h"
#include "pico/stdlib.h"
#include <cstdio>
#include "plus4_log045.h"
#define printf plus4_log045_printf
#if PLUS4_AUDIO046
#include "plus4_audio046_filter.h"
#if PLUS4_AUDIO047
#define PLUS4_AUDIO_LABEL "PLUS4_AUDIO047"
#else
#define PLUS4_AUDIO_LABEL "PLUS4_AUDIO046"
#endif
#else
#define PLUS4_AUDIO_LABEL "PLUS4_AUDIO045"
#endif

namespace {
plus4_audio::MonoRing<> pcm;
#if PLUS4_AUDIO046
plus4_audio::FirDecimate5 decimator;
#else
plus4_audio::Decimate5 decimator;
#endif

uint32_t generated, dropped, nonzero, high, peak, last_rate_us, last_rate_samples;
uint32_t last_report_us, last_generated, last_dropped, last_nonzero;
bool rate_initialized, rate_baseline;
#if PLUS4_AUDIO047
uint32_t period_q8, samples_q8;
#endif
std::atomic<uint32_t> source_rate{44336}, output_frames{0}, starves{0}, silence{0};
std::atomic<uint32_t> live_rate{44336}, live_level{0};
std::atomic<bool> game_started{false};
uint32_t out_count, starve_count, silence_count, fraction, gain;
#ifdef PLUS4_AUDIO045
constexpr int32_t target_level = 1024;
#else
constexpr int32_t target_level = 1280;
#endif
int32_t smooth_level = target_level * 256;
int16_t previous, next, last_output;
bool playing;

int16_t fade_to_zero(int16_t sample) {
    if (sample > 64) return sample - 64;
    if (sample < -64) return sample + 64;
    return 0;
}
}

extern "C" void __not_in_flash_func(plus4_audio045_pcm)(int16_t sample, uint32_t count) {
    generated += count;
    dropped += count - pcm.push_repeat(sample, count);
    if (sample) nonzero += count;
    uint32_t magnitude = sample < 0 ? -int32_t(sample) : sample;
    if (count && magnitude > peak) peak = magnitude;
    uint32_t level = pcm.producer_level();
    if (level > high) high = level;
}

extern "C" void __not_in_flash_func(plus4_audio045_sample)(int16_t sample) {
    int16_t output;
    if (decimator.push(sample, output)) plus4_audio045_pcm(output, 1);
}

extern "C" void plus4_audio045_begin(void) {
    last_report_us = last_rate_us = time_us_32();
    last_rate_samples = generated;
    game_started.store(true, std::memory_order_release);
    printf(PLUS4_AUDIO_LABEL " begin source_native=221681 decimation=5 source=44336 output=44100 mono_ring=2048 "
#if PLUS4_AUDIO046
           "filter=fir63 cutoff_hz=14000 gain_q8=64 "
#endif
#if PLUS4_AUDIO045_SILENT_CONTROL
           "mode=silent-control "
#else
           "mode=game "
#endif
#if PLUS4_AUDIO047
           "src=period-ema8-linear "
#else
           "src=adaptive-linear "
#endif
           "pitch_preserving=0 prefill=1024 core0_wait=0\n");
}

extern "C" void plus4_audio045_frame(void) {

    uint32_t now = time_us_32(), elapsed = now - last_rate_us;

    if (!rate_baseline) {
        last_rate_us = now;
        last_rate_samples = generated;
        rate_baseline = true;
        return;
    }
    if (elapsed < 10000u) return;
#if PLUS4_AUDIO047

    if (elapsed > 1000000u) {
        last_rate_us = now; last_rate_samples = generated;
        rate_initialized = false;
        return;
    }
    const uint32_t count = generated - last_rate_samples;
    if (rate_initialized) {
        period_q8 = uint32_t(int32_t(period_q8) +
            (int32_t(elapsed * 256u) - int32_t(period_q8)) / 8);
        samples_q8 = uint32_t(int32_t(samples_q8) +
            (int32_t(count * 256u) - int32_t(samples_q8)) / 8);
    } else {
        period_q8 = elapsed * 256u; samples_q8 = count * 256u;
    }

    uint32_t rate = uint64_t(samples_q8) * 1000000u / period_q8;
#else
    uint32_t rate = uint64_t(generated - last_rate_samples) * 1000000u / elapsed;
#endif
    if (rate < 12000) rate = 12000;
#ifdef PLUS4_AUDIO045
    if (rate > 46000) rate = 46000;
#else
    if (rate > 55000) rate = 55000;
#endif
#if PLUS4_AUDIO047
    source_rate.store(rate, std::memory_order_relaxed);
#else
    uint32_t old = source_rate.load(std::memory_order_relaxed);
    bool step_change = rate * 10 < old * 9 || rate * 10 > old * 11;
    source_rate.store(rate_initialized && !step_change ? (old * 7 + rate) / 8 : rate,
                      std::memory_order_relaxed);
#endif
    rate_initialized = true;
    last_rate_samples = generated;
    last_rate_us = now;
}

extern "C" void __not_in_flash_func(plus4_audio045_pull)(int16_t samples[4]) {
    const uint32_t level = pcm.consumer_level();

    smooth_level += (int32_t(level * 256) - smooth_level) / 256;
    int32_t rate = int32_t(source_rate.load(std::memory_order_relaxed)) +
                   (smooth_level / 256 - target_level) * 4;
#ifdef PLUS4_AUDIO045

    if (level < 512) rate = rate * int32_t(level) / 512;
    if (level > 1536) rate += int32_t(level - 1536) * 128;
#endif
    if (rate < 8000) rate = 8000;
#ifdef PLUS4_AUDIO045
    if (rate > 80000) rate = 80000;

    const uint32_t step = uint32_t(rate) * 16384u / 11025u;
#else
    if (rate > 60000) rate = 60000;
    const uint32_t step = uint32_t(rate) * 65536u / 44100u;
#endif
    if (!playing && last_output == 0 && game_started.load(std::memory_order_acquire) && level >= 1024) {
        int16_t pair[2];
        if (pcm.pop(pair, 2) == 2) {
            previous = pair[0]; next = pair[1]; fraction = gain = 0;
            smooth_level = target_level * 256;
            playing = true;
        }
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (!playing) {
            last_output = fade_to_zero(last_output);
            samples[i] = last_output;
            ++silence_count;
            continue;
        }

        int32_t value = (int32_t(previous) * int32_t(65536u - fraction) +
                         int32_t(next) * int32_t(fraction)) / 65536;
        if (gain < 256) ++gain;
        samples[i] = last_output = int16_t(value * int32_t(gain) / 256);
        fraction += step;

        while (fraction >= 65536u) {
            fraction -= 65536u;
            previous = next;
            if (pcm.pop(&next, 1) != 1) {
                playing = false;
                ++starve_count;
                break;
            }
        }
    }
    out_count += 4;
    output_frames.store(out_count, std::memory_order_relaxed);
    starves.store(starve_count, std::memory_order_relaxed);
    silence.store(silence_count, std::memory_order_relaxed);
    live_rate.store(uint32_t(rate), std::memory_order_relaxed);
    live_level.store(level, std::memory_order_relaxed);
}

extern "C" void plus4_audio045_report(void) {
    uint32_t now = time_us_32(), elapsed = now - last_report_us;
    if (elapsed < 5000000u) return;
    printf(PLUS4_AUDIO_LABEL " us=%lu src=%lu nz=%lu lost=%lu peak=%lu high=%lu "
           "level=%lu rate=%lu out_total=%lu starve_total=%lu pad_total=%lu\n",
           (unsigned long)elapsed, (unsigned long)(generated - last_generated),
           (unsigned long)(nonzero - last_nonzero), (unsigned long)(dropped - last_dropped),
           (unsigned long)peak, (unsigned long)high,
           (unsigned long)live_level.load(), (unsigned long)live_rate.load(),
           (unsigned long)output_frames.load(), (unsigned long)starves.load(),
           (unsigned long)silence.load());
    last_report_us = now; last_generated = generated; last_nonzero = nonzero; last_dropped = dropped;
    high = peak = 0;
}
