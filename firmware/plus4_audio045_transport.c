

#include "hstx.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "plus4_audio045.h"
#include "plus4_log045.h"
#include <stdatomic.h>
#include <stdio.h>

static uint16_t framebuffer[320 * 240] __attribute__((aligned(4)));
volatile bool HSTX_vblank;
static uint32_t plus4_core1_stack[2048] __attribute__((aligned(32)));
#define STACK_PATTERN 0x5a17c0deu
#define STACK_GUARD_WORDS 32u
static uint32_t packet_count, last_packets, last_under;
static _Atomic uint32_t total_packets, pump_max_us;
static int cs_frame;
extern void plus4_hdmi045_dma_snapshot(uint32_t out[4]);

uint8_t *hstx_getframebuffer(void) { return (uint8_t *)framebuffer; }
uint16_t *hstx_getlineFromFramebuffer(int line) { return framebuffer + line * 320; }
uint32_t hstx_getframecounter(void) { return video_frame_count; }
void hstx_waitForVSync(void) {
    uint32_t frame = video_frame_count;
    while (frame == video_frame_count) tight_loop_contents();
}
static void __not_in_flash_func(scanline)(uint32_t v, uint32_t active, uint32_t *out) {
    (void)v;
    if (active >= 480) return;
    const uint16_t *src = framebuffer + (active >> 1) * 320;
    for (unsigned x = 0; x < 320; ++x) {
        uint32_t p = src[x] & 0x7fffu;
        out[x] = p | (p << 16);
    }
}
static void __not_in_flash_func(audio_pump)(void) {
#if PLUS4_AUDIO046

    if (hstx_di_queue_get_level() >= 28) return;
#endif
    const uint32_t start = time_us_32();
    for (unsigned budget = 0; budget < 8 && hstx_di_queue_get_level() < 28; ++budget) {
        int16_t mono[4];
        audio_sample_t samples[4];
        plus4_audio045_pull(mono);
        for (unsigned i = 0; i < 4; ++i) samples[i].left = samples[i].right = mono[i];
        hstx_packet_t packet;
        hstx_data_island_t island;
        int next = hstx_packet_set_audio_samples_cs(&packet, samples, 4, cs_frame);
        hstx_encode_data_island(&island, &packet, false, true);
        if (hstx_di_queue_push(&island)) { cs_frame = next; ++packet_count; }
    }
    atomic_store_explicit(&total_packets, packet_count, memory_order_relaxed);
    uint32_t elapsed = time_us_32() - start;
    if (elapsed > atomic_load_explicit(&pump_max_us, memory_order_relaxed))
        atomic_store_explicit(&pump_max_us, elapsed, memory_order_relaxed);
}
void hstx_init(bool dvi_only) {
    (void)dvi_only;
    video_output_set_dvi_mode(false);
    hstx_di_queue_init();
    video_output_init(640, 480);
    pico_hdmi_set_audio_sample_rate(44100);
    for (unsigned i = 0; i < 4; ++i) audio_pump();
    atomic_store(&pump_max_us, 0);
    video_output_set_scanline_callback(scanline);
    video_output_set_background_task(audio_pump);
    for (unsigned i = 0; i < 2048; ++i) plus4_core1_stack[i] = STACK_PATTERN;
    multicore_launch_core1_with_stack(video_output_core1_run,
        plus4_core1_stack + STACK_GUARD_WORDS,
        sizeof(plus4_core1_stack) - STACK_GUARD_WORDS * sizeof(uint32_t));
    printf("PLUS4_HDMI045 ready rate=44100 channels=2 bits=16 di=32 core1_stack=8064\n");
}
void plus4_audio045_transport_report(void) {
    uint32_t guard = 1, free_words = 0;
    for (unsigned i = 0; i < STACK_GUARD_WORDS; ++i)
        if (plus4_core1_stack[i] != STACK_PATTERN) guard = 0;
    while (free_words < 2048 && plus4_core1_stack[free_words] == STACK_PATTERN) ++free_words;
    const uint32_t packets = atomic_load_explicit(&total_packets, memory_order_relaxed);
    const uint32_t under = hstx_di_queue_get_underrun_count();
    uint32_t diag[4];
    plus4_hdmi045_dma_snapshot(diag);
    plus4_log045_printf("PLUS4_LINK045 packets=%lu under=%lu di=%lu resync=%d miss=%lu pump_us=%lu guard=%lu low_free=%lu dma_empty=%lu dma_dual=%lu dma_unexpected=%lu dma_busy=%lu log_drop=%lu\n",
        (unsigned long)(packets - last_packets), (unsigned long)(under - last_under),
        (unsigned long)hstx_di_queue_get_level(), get_video_output_resync_count(),
        (unsigned long)video_output_get_line_miss_count(),
        (unsigned long)atomic_load_explicit(&pump_max_us, memory_order_relaxed),
        (unsigned long)guard, (unsigned long)(free_words * 4u),
        (unsigned long)diag[0], (unsigned long)diag[1], (unsigned long)diag[2],
        (unsigned long)diag[3], (unsigned long)plus4_log045_dropped());
    last_packets = packets; last_under = under;
}
