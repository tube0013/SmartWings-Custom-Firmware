#ifndef SMARTWINGS_ZCL_H
#define SMARTWINGS_ZCL_H
#include "smartwings_motor.h"
void sw_zcl_init(void);
bool sw_zcl_motor_reversed(void);
void sw_zcl_feedback(const sw_motor_t *motor);
void sw_zcl_motion_started(void);
bool sw_zcl_set_poll_interval_ms(uint16_t interval);
#endif
