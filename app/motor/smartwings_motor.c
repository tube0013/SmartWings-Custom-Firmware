#include "smartwings_motor.h"
#include <string.h>
static void discard(sw_motor_t *m, uint8_t n)
{
  m->used -= n;
  memmove(m->bytes, m->bytes + n, m->used);
}
static uint8_t expected_length(uint8_t opcode)
{
  switch (opcode) {
    case 0xF5: return 7;
    case 0xF8: return 4;
    case 0xF6: case 0xF9: case 0xFA: case 0xFB: return 3;
    default: return 0;
  }
}
void sw_motor_init(sw_motor_t *m, bool reversed, uint32_t now)
{
  memset(m, 0, sizeof(*m));
  m->raw_reversed = reversed;
  m->position = m->battery_zcl = 0xFF;
  m->startup_pending = true;
  m->startup_deadline_ms = now + 1000;
}
void sw_motor_set_reversed(sw_motor_t *m, bool reversed)
{
  if (m->raw_reversed == reversed) return;
  m->raw_reversed = reversed;
  if (m->position_valid)
    m->position = reversed ? 100 - m->raw_position : m->raw_position;
  /* An already-sent physical destination does not change on a mode write. */
  if (m->requested_valid) m->requested_position = 100 - m->requested_position;
}
void sw_motor_rx(sw_motor_t *m, uint8_t byte, uint32_t now)
{
  /* At 9600 baud each byte takes ~1.04 ms. A truncated frame expires;
   * the captured ~5 ms standalone-zero gap remains valid. */
  if (m->used && (uint32_t)(now - m->last_byte_ms) > 20) {
    ++m->rejected_frames; m->used = 0;
  }
  m->last_byte_ms = now;
  m->bytes[m->used++] = byte;
  while (m->used) {
    uint8_t n = m->bytes[0];
    if (n < 3 || n > 9) { discard(m, 1); continue; }
    if (m->used < 2) break;
    if (expected_length(m->bytes[1]) != n) {
      ++m->rejected_frames; discard(m, 1); continue;
    }
    if (m->used < n) break;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < n; ++i) sum ^= m->bytes[i];
    if (sum) { ++m->rejected_frames; discard(m, 1); continue; }
    ++m->accepted_frames;
    m->last_opcode = m->bytes[1];
    if (m->bytes[1] == 0xF5 && m->bytes[4] <= 100) {
      m->raw_position = m->bytes[4];
      m->position = m->raw_reversed ? 100 - m->raw_position : m->raw_position;
      m->status[0] = m->bytes[2]; m->status[1] = m->bytes[3]; m->status[2] = m->bytes[5];
      m->position_valid = true; m->last_position_ms = now;
    } else if (m->bytes[1] == 0xF8) {
      m->battery_valid = m->bytes[2] <= 100;
      m->battery_zcl = m->battery_valid ? 2 * m->bytes[2] : 0xFF;
      m->last_battery_ms = now;
    }
    /* Network frames are accepted for dispatch by the transport layer;
     * no leave/join or guessed F6 semantics are performed in this parser. */
    discard(m, n);
  }
}
bool sw_motor_startup_tick(sw_motor_t *m, uint32_t now)
{
  if ((int32_t)(now - m->startup_deadline_ms) < 0) return false;
  if (m->startup_pending) {
    m->startup_pending = false; m->startup_observing = true;
    m->startup_deadline_ms = now + 2000;
    return true;
  }
  m->startup_observing = false;
  return false;
}
void sw_motor_movement_requested(sw_motor_t *m, uint8_t canonical)
{
  m->startup_pending = m->startup_observing = false;
  if (canonical <= 100) { m->requested_valid = true; m->requested_position = canonical; }
}
size_t sw_motor_target(sw_motor_t *m, uint8_t canonical, uint8_t out[13])
{
  if (canonical > 100) return 0;
  const uint8_t prefix[13] = {0x0D,0xF1,0,0,0,0,0,0,4,0x40,0,0,0};
  memcpy(out, prefix, sizeof(prefix));
  out[10] = m->raw_reversed ? 100 - canonical : canonical;
  out[12] = 0xB8 ^ out[10];
  sw_motor_movement_requested(m, canonical);
  return 13;
}
size_t sw_motor_short(uint8_t opcode, uint8_t out[3])
{
  if (opcode < 1 || opcode > 3) return 0;
  out[0] = 3; out[1] = opcode; out[2] = 3 ^ opcode;
  return 3;
}
