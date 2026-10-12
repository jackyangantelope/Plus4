
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void plus4_audio045_begin(void);
void plus4_audio045_sample(int16_t sample);
void plus4_audio045_frame(void);
void plus4_audio045_report(void);
void plus4_audio045_pull(int16_t samples[4]);
void plus4_audio045_transport_report(void);
#ifdef __cplusplus
}
#endif
