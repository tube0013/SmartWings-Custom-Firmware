#ifndef SMARTWINGS_MOTOR_H
#define SMARTWINGS_MOTOR_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Canonical position: 0 = physically open, 100 = physically closed.
 * Physical opcode/axis mapping must be verified on the motor before enabling TX. */
typedef struct {
  uint8_t bytes[9], used, last_opcode;
  uint32_t last_byte_ms, last_position_ms, last_battery_ms;
  uint32_t accepted_frames, rejected_frames;
  uint8_t position, battery_zcl, raw_position, status[3];
  uint8_t requested_position;
  bool position_valid, battery_valid, requested_valid, raw_reversed;
  bool startup_pending, startup_observing;
  uint32_t startup_deadline_ms;
} sw_motor_t;
void sw_motor_init(sw_motor_t *m, bool reversed, uint32_t now_ms);
/* Reinterpret the coordinate without sending a motor command or inventing feedback. */
void sw_motor_set_reversed(sw_motor_t *m, bool reversed);
/* Accept reports independently; repeated values still refresh timestamps. */
void sw_motor_rx(sw_motor_t *m, uint8_t byte, uint32_t now_ms);
/* Once-per-boot startup policy: call only after UART RX is initialized.
 * Returns true once at 1 s to request a Stop; later movement cancels it. */
bool sw_motor_startup_tick(sw_motor_t *m, uint32_t now_ms);
void sw_motor_movement_requested(sw_motor_t *m, uint8_t canonical);
size_t sw_motor_target(sw_motor_t *m, uint8_t canonical, uint8_t out[13]);
size_t sw_motor_short(uint8_t opcode, uint8_t out[3]);
#endif
