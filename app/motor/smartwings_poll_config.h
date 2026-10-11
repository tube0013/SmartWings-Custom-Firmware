#ifndef SMARTWINGS_POLL_CONFIG_H
#define SMARTWINGS_POLL_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
#define SW_CONFIG_CLUSTER 0xFC00
#define SW_CONFIG_MANUFACTURER 0xFFFE
#define SW_IDLE_POLL_ATTRIBUTE 0x0000
#define SW_DEFAULT_IDLE_POLL_MS 250
#define SW_FAST_POLL_MS 100
static inline bool sw_poll_interval_valid(uint16_t ms)
{
  return ms >= 250 && ms <= 1000 && ms % 250 == 0;
}
#endif
