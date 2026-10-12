
#include "hstx_data_island_queue.h"

#include "video_output.h"
#include "hstx_packet.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "pico.h"
#include "hardware/sync.h"
#ifndef DI_RING_BUFFER_SIZE
#define DI_RING_BUFFER_SIZE 256
#endif

#ifdef HSTX_DI_RING_ADDRESS
static hstx_data_island_t *const di_ring_buffer = (hstx_data_island_t *)(HSTX_DI_RING_ADDRESS);
#else
static hstx_data_island_t *di_ring_buffer = NULL;
#endif
static volatile uint32_t di_ring_head = 0;
static volatile uint32_t di_ring_tail = 0;

static hstx_data_island_t silence_packet;

#define DI_LINE_RATE_HZ (25200000u / MODE_H_TOTAL_PIXELS)
static uint32_t audio_sample_accum = 0;
static uint32_t audio_samples_per_sec = 48000;
extern void * frens_f_malloc(size_t size);
void hstx_di_queue_init(void)
{
    di_ring_head = 0;
    di_ring_tail = 0;
    audio_sample_accum = 0;

#ifdef HSTX_DI_RING_ADDRESS
    printf("HSTX DI ring buffer at fixed %p (%u bytes)\n",
           (void *)di_ring_buffer,
           (unsigned)(DI_RING_BUFFER_SIZE * sizeof(hstx_data_island_t)));
#else
    if (di_ring_buffer == NULL) {
        printf("Allocating memory for HSTX DI ring buffer: %d bytes\n", DI_RING_BUFFER_SIZE * sizeof(hstx_data_island_t));
        di_ring_buffer = (hstx_data_island_t *)malloc(DI_RING_BUFFER_SIZE * sizeof(hstx_data_island_t));
    }
#endif

    hstx_packet_t packet;
    audio_sample_t samples[4] = {0};
    (void)hstx_packet_set_audio_samples(&packet, samples, 4, 4);
    hstx_encode_data_island(&silence_packet, &packet, false, true);
}

void hstx_di_queue_set_sample_rate(uint32_t sample_rate)
{
    audio_samples_per_sec = sample_rate;
    audio_sample_accum = 0;
}

bool __not_in_flash_func(hstx_di_queue_push)(const hstx_data_island_t *island)
{
    uint32_t next_head = (di_ring_head + 1) % DI_RING_BUFFER_SIZE;
    if (next_head == di_ring_tail)
        return false;

    volatile uint32_t *dst = di_ring_buffer[di_ring_head].words;
    const uint32_t *src = island->words;
    for (size_t i = 0; i < count_of(island->words); i++)
        dst[i] = src[i];

    __dmb();
    di_ring_head = next_head;
    return true;
}

uint32_t __not_in_flash_func(hstx_di_queue_get_level)(void)
{
    uint32_t head = di_ring_head;
    uint32_t tail = di_ring_tail;
    if (head >= tail)
        return head - tail;
    return DI_RING_BUFFER_SIZE + head - tail;
}

void __not_in_flash_func(hstx_di_queue_tick)(void)
{
    audio_sample_accum += audio_samples_per_sec;
}

static volatile uint32_t di_underrun_count = 0;

const uint32_t *__not_in_flash_func(hstx_di_queue_get_audio_packet)(void)
{

    if (audio_sample_accum >= 4u * DI_LINE_RATE_HZ) {
        audio_sample_accum -= 4u * DI_LINE_RATE_HZ;
        if (di_ring_tail != di_ring_head) {
            const uint32_t *words = di_ring_buffer[di_ring_tail].words;
            di_ring_tail = (di_ring_tail + 1) % DI_RING_BUFFER_SIZE;
            return words;
        }

        di_underrun_count++;
        return silence_packet.words;
    }
    return NULL;
}

uint32_t hstx_di_queue_get_underrun_count(void)
{
    return di_underrun_count;
}

uint32_t hstx_di_queue_get_underrun_delta(void)
{
    static uint32_t last = 0;
    uint32_t now = di_underrun_count;
    uint32_t delta = now - last;
    last = now;
    return delta;
}
