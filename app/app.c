/***************************************************************************//**
 * @file app.c
 * @brief Callbacks implementation and application specific code.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "app/framework/include/af.h"
static void sw_network_steering_completed(void);

/** @brief Complete network steering.
 *
 * This callback is fired when the Network Steering plugin is complete.
 *
 * @param status On success this will be set to SL_STATUS_OK to indicate a
 * network was joined successfully. On failure this will be the status code of
 * the last join or scan attempt. Ver.: always
 *
 * @param totalBeacons The total number of 802.15.4 beacons that were heard,
 * including beacons from different devices with the same PAN ID. Ver.: always
 * @param joinAttempts The number of join attempts that were made to get onto
 * an open Zigbee network. Ver.: always
 *
 * @param finalState The finishing state of the network steering process. From
 * this, one is able to tell on which channel mask and with which key the
 * process was complete. Ver.: always
 */
void sl_zigbee_af_network_steering_complete_cb(sl_status_t status,
                                               uint8_t totalBeacons,
                                               uint8_t joinAttempts,
                                               uint8_t finalState)
{
  UNUSED_VAR(totalBeacons);
  UNUSED_VAR(joinAttempts);
  UNUSED_VAR(finalState);
  sl_zigbee_app_debug_println("%s network %s: 0x%02X", "Join", "complete", status);
  sw_network_steering_completed();
}

/** @brief
 *
 * Application framework equivalent of ::sl_zigbee_radio_needs_calibrating_handler
 */
void sl_zigbee_af_radio_needs_calibrating_cb(void)
{
  sl_mac_calibrate_current_channel();
}

// Altered for the SmartWings bench: scheduled events wake the power manager.
#include "sl_iostream.h"
#include "app/framework/plugin/network-steering/network-steering.h"
#include "ota-client-policy-config.h"
#include "sl_sleeptimer.h"
#include "motor/smartwings_transport.h"
#include "motor/smartwings_zcl.h"
static sl_zigbee_af_event_t join_event;
static bool join_window, steering_active, had_network;
static uint32_t join_deadline_ms;
static void sw_network_steering_completed(void) { steering_active = false; }
static uint32_t app_now(void)
{
  return sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
}
static void join_event_handler(sl_zigbee_af_event_t *event)
{
  sl_zigbee_af_event_set_inactive(event);
  if (sl_zigbee_network_state() == SL_ZIGBEE_JOINED_NETWORK) {
    join_window = false; return;
  }
  if (!join_window) return;
  if ((int32_t)(app_now() - join_deadline_ms) >= 0) {
    join_window = false;
    if (steering_active) sl_zigbee_af_network_steering_stop();
    steering_active = false;
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "MOTOR network join window ended\r\n");
    return;
  }
  if (sl_zigbee_network_state() == SL_ZIGBEE_NO_NETWORK && !steering_active) {
    sl_status_t status = sl_zigbee_af_network_steering_start();
    steering_active = status == SL_STATUS_OK;
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "MOTOR network steering status0x%lx\r\n", (unsigned long)status);
  }
  sl_zigbee_af_event_set_delay_ms(event, 1000);
}
static void request_join(void)
{
  if (sl_zigbee_network_state() == SL_ZIGBEE_JOINED_NETWORK
      || sl_zigbee_network_state() == SL_ZIGBEE_JOINED_NETWORK_NO_PARENT) return;
  /* Repeated motor FA reports share one bounded window, not endless scans. */
  if (!join_window) { join_window = true; join_deadline_ms = app_now() + 60000; }
  sl_zigbee_af_event_set_active(&join_event);
}
void sw_network_motor_command(uint8_t opcode)
{
  sl_zigbee_network_status_t state = sl_zigbee_network_state();
  sl_iostream_printf(SL_IOSTREAM_STDOUT, "MOTOR network opcode%02X state%u\r\n", opcode, state);
  if (opcode == 0xFA) { request_join(); return; }
  join_window = false;
  sl_zigbee_af_event_set_inactive(&join_event);
  if (steering_active) { sl_zigbee_af_network_steering_stop(); steering_active = false; }
  if (opcode == 0xFB || state == SL_ZIGBEE_NO_NETWORK)
    sw_transport_network_notify(0xFB);
  if (state != SL_ZIGBEE_NO_NETWORK && state != SL_ZIGBEE_LEAVING_NETWORK) {
    sl_status_t status = sl_zigbee_leave_network(SL_ZIGBEE_LEAVE_NWK_WITH_NO_OPTION);
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "MOTOR network leave status0x%lx\r\n", (unsigned long)status);
  }
}
void sl_zigbee_af_stack_status_cb(sl_status_t status)
{
  sl_zigbee_network_status_t state = sl_zigbee_network_state();
  sl_iostream_printf(SL_IOSTREAM_STDOUT, "MOTOR network stack status0x%lx state%u\r\n", (unsigned long)status, state);
  if (status == SL_STATUS_NETWORK_UP) {
    join_window = false;
    had_network = true;
    sw_transport_network_notify(0xFA);
  } else if (state == SL_ZIGBEE_NO_NETWORK) {
    /* Notify an actual membership loss once, not every failed association.
     * The motor replies to FB with FA, so failed-scan notices cause retry loops. */
    if (had_network) { had_network = false; sw_transport_network_notify(0xFB); }
    if (join_window) sl_zigbee_af_event_set_active(&join_event);
  }
}
void app_init(void)
{
  sl_zigbee_af_event_init(&join_event, join_event_handler);
  sl_iostream_printf(SL_IOSTREAM_STDOUT, "SW-MG21 OTA bench version %lu; motor UART sleep diagnostic\r\n",
                    (unsigned long)SL_ZIGBEE_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION);
  sw_zcl_init();
  sw_transport_init();
  /* An unjoined shade sleeps until its motor emits a pairing request. */
}
void app_process_action(void) { sw_transport_process_action(); }
