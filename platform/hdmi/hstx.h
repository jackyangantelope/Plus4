
#pragma once
#if PICO_RP2350
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "video_output.h"
#include "hstx_packet.h"
#include "hstx_data_island_queue.h"
extern volatile bool HSTX_vblank;

#define HSTX_BIT_FROM_GPIO(gpio) ((gpio) - 12)
#ifndef HSTX_AUDIO_DI_HIGH_WATERMARK
#define HSTX_AUDIO_DI_HIGH_WATERMARK 200
#endif
uint32_t hstx_getframecounter(void);
void hstx_waitForVSync(void);
void hstx_paceFrame(bool init);
uint8_t *hstx_getframebuffer(void);

uint16_t *hstx_get_gba_framebuffer(unsigned index);

uint16_t *hstx_get_gba_overlay_buffer(unsigned index);

bool hstx_try_present_gba_framebuffer(const uint16_t *framebuffer,
                                      const uint16_t *overlay);

bool hstx_gba_framebuffer_presented(void);
void hstx_setScanLines(int enable);
void hstx_setAspectRatio87(int enable);
void hstx_setScanLineType(int type);
uint16_t *hstx_getlineFromFramebuffer(int scanline);
void hstx_init(bool dviOnly);
void video_output_core1_run(void);
void hstx_push_audio_sample(const int left, const int right);

void hstx_restart_core1(uint32_t *new_stack, size_t new_stack_bytes);

void *hstx_default_core1_stack(size_t *out_bytes);
#ifdef __cplusplus
}
#endif
#endif