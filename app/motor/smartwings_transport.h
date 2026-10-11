#ifndef SMARTWINGS_TRANSPORT_H
#define SMARTWINGS_TRANSPORT_H
#include "smartwings_motor.h"
/* Motor UART sleeps in EM2 and wakes from PA6 falling edge. */
void sw_transport_init(void);
void sw_transport_process_action(void);
bool sw_transport_network_notify(uint8_t opcode);
const sw_motor_t *sw_transport_state(void);
bool sw_transport_target(uint8_t canonical);
bool sw_transport_stop(void);
void sw_transport_set_reversed(bool reversed);
#endif
