
#ifndef HSTX_DATA_ISLAND_QUEUE_H
#define HSTX_DATA_ISLAND_QUEUE_H

#include "hstx_packet.h"

#include <stdbool.h>
#include <stdint.h>

void hstx_di_queue_init(void);

void hstx_di_queue_set_sample_rate(uint32_t sample_rate);

bool hstx_di_queue_push(const hstx_data_island_t *island);

uint32_t hstx_di_queue_get_level(void);

void hstx_di_queue_tick(void);

const uint32_t *hstx_di_queue_get_audio_packet(void);

uint32_t hstx_di_queue_get_underrun_count(void);

uint32_t hstx_di_queue_get_underrun_delta(void);

#endif
