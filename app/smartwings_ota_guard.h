#ifndef SMARTWINGS_OTA_GUARD_H
#define SMARTWINGS_OTA_GUARD_H
#include <stdbool.h>
#include <stdint.h>
/* Application policy only; not cryptographic or OTP rollback protection. */
static inline bool sw_ota_id_allowed(uint16_t manufacturer, uint16_t image_type,
                                    uint32_t candidate, uint32_t running)
{
  return manufacturer == 0xFFFE && image_type == 0x6AAD && candidate > running;
}
#endif
