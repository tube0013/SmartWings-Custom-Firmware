#include "smartwings_motor.h"
#include <stdio.h>
int main(void) {
  sw_motor_t m; unsigned time,byte;sw_motor_init(&m,false,0);
  while(scanf("%u %u",&time,&byte)==2)sw_motor_rx(&m,(uint8_t)byte,time);
  printf("%lu %u %u %u %u\n",(unsigned long)m.accepted_frames,m.position_valid,m.position,m.battery_valid,m.battery_zcl);
}
