#include <assert.h>
#include "../app/motor/smartwings_poll_config.h"
int main(void)
{
  /* Check the complete wire-value domain, including 0/FFFF and values between presets. */
  for (unsigned i = 0; i <= UINT16_MAX; ++i)
    assert(sw_poll_interval_valid((uint16_t)i)
           == (i == 250 || i == 500 || i == 750 || i == 1000));
  assert(sw_poll_interval_valid(SW_DEFAULT_IDLE_POLL_MS));
  assert(SW_FAST_POLL_MS < SW_DEFAULT_IDLE_POLL_MS);
  return 0;
}
