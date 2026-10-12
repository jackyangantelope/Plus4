
#ifndef PLUS4_LOG045_H
#define PLUS4_LOG045_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void plus4_log045_start(void);
void plus4_log045_pump(void);
uint32_t plus4_log045_dropped(void);
int plus4_log045_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#ifdef __cplusplus
}
#endif
#endif
