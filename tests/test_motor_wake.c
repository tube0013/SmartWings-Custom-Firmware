#include "smartwings_motor.h"
#include <assert.h>
#include <stdio.h>
/* Exercise the captured zero lead-in and report triplets, allowing the wake
 * prefix to be lost/corrupted before the real USART frame. Not a timing proof. */
int main(void)
{
  sw_motor_t m;
  const uint8_t position[] = {7,0xF5,0x0E,0,82,0,0xAE};
  const uint8_t battery[] = {4,0xF8,100,0x98};
  const uint8_t pairing[] = {3,0xF9,0xFA, 3,0xFA,0xF9, 3,0xFB,0xF8};
  for (unsigned prefix=0;prefix<3;prefix++) {
    sw_motor_init(&m,false,0);
    for(unsigned repeat=0;repeat<3;repeat++) {
      unsigned t=100+repeat*56;
      if(prefix==1) sw_motor_rx(&m,0,t); // complete wake prefix
      if(prefix==2) sw_motor_rx(&m,0xFF,t); // partial prefix
      for(unsigned i=0;i<sizeof(position);i++) sw_motor_rx(&m,position[i],t+6+i);
      for(unsigned i=0;i<sizeof(battery);i++) sw_motor_rx(&m,battery[i],t+20+i);
    }
    assert(m.accepted_frames==6 && m.rejected_frames==0);
    assert(m.position_valid && m.position==82 && m.battery_valid && m.battery_zcl==200);
    assert(m.startup_pending); // receiving/waking never restarts boot startup policy
    for(unsigned group=0;group<3;group++) {
      for(unsigned i=0;i<3;i++) sw_motor_rx(&m,pairing[group*3+i],400+group*60+i);
      assert(m.last_opcode==pairing[group*3+1]);
    }
    const uint8_t bad_leave[]={3,0xF9,0};
    for(unsigned i=0;i<3;i++) sw_motor_rx(&m,bad_leave[i],1000+i);
    assert(m.last_opcode==0xFB && m.accepted_frames==9); // invalid XOR never dispatches leave
    sw_motor_movement_requested(&m,80);
    assert(!sw_motor_startup_tick(&m,3000));
  }
  puts("captured wake-prefix/triplet/network-opcode regressions passed");
}
