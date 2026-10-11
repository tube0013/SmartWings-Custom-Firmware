#include "smartwings_motor.h"
#include "smartwings_cover_config.h"
#include <assert.h>
#include <stdio.h>
static void feed(sw_motor_t *m, const uint8_t *p, size_t n, uint32_t time)
{ for (size_t i=0;i<n;i++) sw_motor_rx(m,p[i],time+(uint32_t)i); }
int main(void)
{
  sw_motor_t m; uint8_t out[13];
  const uint8_t zero[]={0,7,0xF5,0x0E,0,0,0,0xFC};
  const uint8_t pos[]={0,7,0xF5,0x0E,0,0x46,0xB4,0x0E};
  const uint8_t bat[]={0,4,0xF8,0x64,0x98};
  sw_motor_init(&m,false,0);
  assert(!m.position_valid && !m.battery_valid);
  feed(&m,zero,sizeof(zero),100);assert(m.position_valid && m.position==0);
  feed(&m,pos,sizeof(pos),200);assert(m.position==70 && m.status[2]==0xB4);
  feed(&m,pos,sizeof(pos),300);assert(m.last_position_ms==307 && m.accepted_frames==3);
  feed(&m,bat,sizeof(bat),400);assert(m.battery_valid && m.battery_zcl==200);
  assert(!sw_motor_startup_tick(&m,999));assert(sw_motor_startup_tick(&m,1000));
  assert(!sw_motor_startup_tick(&m,1001));assert(!sw_motor_startup_tick(&m,3000));
  assert(!m.startup_observing);
  assert(sw_motor_target(&m,25,out)==13 && out[10]==0x19 && out[12]==0xA1);
  assert(m.position==70 && m.requested_position==25); /* never predict feedback */
  sw_motor_init(&m,true,0);feed(&m,pos,sizeof(pos),100);assert(m.position==30);
  assert(sw_motor_target(&m,25,out)==13 && out[10]==75 && out[12]==0xF3);
  assert(!sw_motor_startup_tick(&m,1000));
  assert(!sw_motor_target(&m,101,out));assert(!sw_motor_short(0x28,out));
  assert(sw_motor_short(3,out)==3 && out[2]==0);
  sw_motor_init(&m,false,0);
  const uint8_t garbage[]={0x66,9,0x01,7,0xF5,0,0,70,0,0};
  feed(&m,garbage,sizeof(garbage),0);feed(&m,pos,sizeof(pos),100);
  assert(m.position_valid && m.position==70 && m.rejected_frames>0);
  sw_motor_init(&m,false,0);sw_motor_rx(&m,7,0);sw_motor_rx(&m,0xF5,1);
  feed(&m,bat,sizeof(bat),100);assert(m.battery_valid && !m.position_valid);
  const uint8_t unknown[]={4,0xF8,0xFF,3};feed(&m,unknown,4,200);assert(!m.battery_valid && m.battery_zcl==0xFF);
  sw_motor_init(&m,false,0xFFFFFF00u);assert(!sw_motor_startup_tick(&m,743));assert(sw_motor_startup_tick(&m,744));
  /* Late reports remain acceptable after startup timeout, independent of battery. */
  feed(&m,zero,sizeof(zero),4000);assert(m.position_valid && !m.battery_valid);
  /* Runtime mode switching preserves physical targets, complements cached
   * feedback immediately, and leaves unknown/battery/parser/startup state intact. */
  for (unsigned mode = 0; mode <= UINT8_MAX; ++mode)
    assert(sw_cover_mode_valid((uint8_t)mode) == (mode <= 1));
  for (unsigned raw = 0; raw <= 100; ++raw) {
    uint8_t report[] = {7,0xF5,0,0,(uint8_t)raw,0,0};
    for (unsigned j = 0; j < 6; ++j) report[6] ^= report[j];
    sw_motor_init(&m,false,0); feed(&m,report,sizeof(report),100);
    feed(&m,bat,sizeof(bat),200);
    assert(sw_motor_target(&m,(uint8_t)raw,out)==13);
    uint32_t stamp=m.last_position_ms, count=m.accepted_frames;
    sw_motor_set_reversed(&m,true);
    assert(m.position==100-raw && m.raw_position==raw);
    assert(m.requested_position==100-raw && m.requested_valid);
    assert(m.last_position_ms==stamp && m.accepted_frames==count);
    assert(m.battery_valid && m.battery_zcl==200);
    sw_motor_set_reversed(&m,true); assert(m.position==100-raw);
    assert(sw_motor_target(&m,(uint8_t)(100-raw),out)==13);
    assert(out[10]==raw && out[12]==(0xB8 ^ raw));
    feed(&m,report,sizeof(report),300); assert(m.position==100-raw);
    sw_motor_set_reversed(&m,false); assert(m.position==raw);
  }
  sw_motor_init(&m,false,0); sw_motor_set_reversed(&m,true);
  assert(!m.position_valid && m.position==0xFF && !m.requested_valid);
  assert(!m.battery_valid && m.battery_zcl==0xFF && m.startup_pending);
  assert(sw_motor_target(&m,0,out)==13 && out[10]==100);
  assert(sw_motor_target(&m,100,out)==13 && out[10]==0);
  assert(sw_motor_short(3,out)==3 && out[0]==3 && out[1]==3 && out[2]==0);
  puts("motor core regression checks passed");return 0;
}
