#include "smartwings_ota_guard.h"
#include <assert.h>
int main(void)
{
  assert(!sw_ota_id_allowed(0xFFFE, 0x6AAD, 3, 6)); /* stale C slot */
  assert(!sw_ota_id_allowed(0xFFFE, 0x6AAD, 6, 6));
  assert(sw_ota_id_allowed(0xFFFE, 0x6AAD, 7, 6));
  assert(!sw_ota_id_allowed(0x1141, 0x6AAD, 7, 6));
  assert(!sw_ota_id_allowed(0xFFFE, 0x0211, 7, 6));
  assert(!sw_ota_id_allowed(0xFFFE, 0x6AAD, 0, 0xFFFFFFFF));
}
