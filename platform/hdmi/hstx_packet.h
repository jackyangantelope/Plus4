
#ifndef HSTX_PACKET_H
#define HSTX_PACKET_H

#include <stdbool.h>
#include <stdint.h>

#define W_GUARDBAND 2
#define W_PREAMBLE 8
#define W_DATA_PACKET 32
#define W_DATA_ISLAND (W_GUARDBAND + W_DATA_PACKET + W_GUARDBAND)

#define HSTX_DATA_ISLAND_WORDS W_DATA_ISLAND

typedef struct {
    uint8_t header[4];
    uint8_t subpacket[4][8];
} hstx_packet_t;

typedef struct {
    uint32_t words[HSTX_DATA_ISLAND_WORDS];
} hstx_data_island_t;

typedef struct {
    int16_t left;
    int16_t right;
} audio_sample_t;

void hstx_packet_init(hstx_packet_t *packet);
void hstx_packet_set_acr(hstx_packet_t *packet, uint32_t n, uint32_t cts);
void hstx_packet_set_audio_infoframe(hstx_packet_t *packet, uint32_t sample_rate, uint8_t channels,
                                     uint8_t bits_per_sample);
void hstx_packet_set_avi_infoframe(hstx_packet_t *packet, uint8_t vic, uint8_t pixel_repetition);
int hstx_packet_set_audio_samples(hstx_packet_t *packet, const audio_sample_t *samples, int num_samples,
                                  int frame_count);

int hstx_packet_set_audio_samples_cs(hstx_packet_t *packet, const audio_sample_t *samples, int num_samples,
                                     int frame_count);

void hstx_packet_set_cs_sample_rate(uint32_t sample_rate);
void hstx_packet_set_null(hstx_packet_t *packet);

void hstx_encode_data_island(hstx_data_island_t *out, const hstx_packet_t *packet, bool vsync, bool hsync);
const uint32_t *hstx_get_null_data_island(bool vsync, bool hsync);

#endif
