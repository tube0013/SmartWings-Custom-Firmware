#ifndef SMARTWINGS_COVER_CONFIG_H
#define SMARTWINGS_COVER_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
#define SW_SAVED_COVER_MODE_ATTRIBUTE 0x0002u
#define SW_COVER_REVERSED_MODE 0x01u
#define SW_COVER_REVERSED_STATUS 0x04u
/* Calibration, maintenance, LED and reserved modes are unsupported. */
static inline bool sw_cover_mode_valid(uint8_t mode)
{
  return (mode & ~SW_COVER_REVERSED_MODE) == 0;
}
#endif
