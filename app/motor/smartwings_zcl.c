#include "smartwings_zcl.h"
#include "smartwings_transport.h"
#include "smartwings_poll_config.h"
#include "smartwings_cover_config.h"
#include <string.h>
#include "app/framework/include/af.h"
#include "app/framework/plugin/scenes/scenes.h"
#include "sl_iostream.h"
#include "sl_iostream_rtt.h"

#define ENDPOINT 1
static uint8_t last_position = 0xFF, last_battery = 0xFF;
static bool pending_scene, cover_ready;
static uint8_t scene_target, scene_id;
static uint16_t scene_group;

void sw_zcl_motion_started(void)
{
  pending_scene = false;
  sl_zigbee_af_scenes_cluster_make_invalid_cb(ENDPOINT);
}

void sw_zcl_feedback(const sw_motor_t *motor)
{
  uint8_t position = motor->position_valid ? motor->position : 0xFF;
  uint8_t battery = motor->battery_valid ? motor->battery_zcl : 0xFF;
  if (position != last_position) {
    if (sl_zigbee_af_write_server_attribute(ENDPOINT, ZCL_WINDOW_COVERING_CLUSTER_ID,
        ZCL_CURRENT_LIFT_PERCENTAGE_ATTRIBUTE_ID, &position,
        ZCL_INT8U_ATTRIBUTE_TYPE) == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      last_position = position;
      sl_zigbee_af_scenes_cluster_make_invalid_cb(ENDPOINT);
      sl_iostream_printf(sl_iostream_rtt_handle, "ZCL feedback lift=%u\r\n", position);
    }
  }
  if (battery != last_battery) {
    if (sl_zigbee_af_write_server_attribute(ENDPOINT, ZCL_POWER_CONFIG_CLUSTER_ID,
        ZCL_BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID, &battery,
        ZCL_INT8U_ATTRIBUTE_TYPE) == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      last_battery = battery;
      sl_iostream_printf(sl_iostream_rtt_handle, "ZCL feedback battery=%u\r\n", battery);
    }
  }
  if (pending_scene && motor->position_valid && position == scene_target) {
    sl_zigbee_af_scenes_make_valid(ENDPOINT, scene_id, scene_group);
    pending_scene = false;
  }
}

bool sw_zcl_motor_reversed(void)
{
  uint8_t mode = 0;
  return sl_zigbee_af_read_server_attribute(ENDPOINT, ZCL_WINDOW_COVERING_CLUSTER_ID,
      ZCL_MODE_ATTRIBUTE_ID, &mode, sizeof(mode)) == SL_ZIGBEE_ZCL_STATUS_SUCCESS
      && sw_cover_mode_valid(mode) && (mode & SW_COVER_REVERSED_MODE);
}

static void apply_cover_mode(void)
{
  bool reversed = sw_zcl_motor_reversed();
  uint8_t config = 0x03;
  (void)sl_zigbee_af_read_server_attribute(ENDPOINT, ZCL_WINDOW_COVERING_CLUSTER_ID,
      ZCL_CONFIG_STATUS_ATTRIBUTE_ID, &config, sizeof(config));
  config = (config & ~SW_COVER_REVERSED_STATUS)
           | (reversed ? SW_COVER_REVERSED_STATUS : 0);
  (void)sl_zigbee_af_write_server_attribute(ENDPOINT, ZCL_WINDOW_COVERING_CLUSTER_ID,
      ZCL_CONFIG_STATUS_ATTRIBUTE_ID, &config, ZCL_BITMAP8_ATTRIBUTE_TYPE);
  sw_transport_set_reversed(reversed);
  /* Mode changes invalidate current scene, never send movement or Stop. */
  pending_scene = false;
  sl_zigbee_af_scenes_cluster_make_invalid_cb(ENDPOINT);
  sw_zcl_feedback(sw_transport_state());
}

static uint32_t scenes_measurement_filter(sl_service_opcode_t opcode,
                                         sl_service_function_context_t *context)
{
  (void)opcode;
  sl_zigbee_af_cluster_command_t *cmd = context->data;
  if (cmd->commandId == 4 && !sw_transport_state()->position_valid)
    return SL_ZIGBEE_ZCL_STATUS_FAILURE;
  /* Let the SDK Scenes service decode/store/recall all supported commands. */
  return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
}

static uint32_t lift_command_parse(sl_service_opcode_t opcode,
                                  sl_service_function_context_t *context)
{
  (void)opcode;
  sl_zigbee_af_cluster_command_t *cmd = context->data;

  sl_zigbee_af_status_t status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  uint16_t length = cmd->payloadStartIndex <= cmd->bufLen
                    ? cmd->bufLen - cmd->payloadStartIndex : 0xFFFF;
  bool ok = false;
  switch (cmd->commandId) {
    case 0: case 1: case 2:
      if (length != 0) status = SL_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND;
      else {
        ok = cmd->commandId == 2 ? sw_transport_stop()
             : sw_transport_target(cmd->commandId == 0 ? 0 : 100);
        status = ok ? SL_ZIGBEE_ZCL_STATUS_SUCCESS : SL_ZIGBEE_ZCL_STATUS_FAILURE;
      }
      break;
    case 5:
      if (length != 1) status = SL_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND;
      else if (cmd->buffer[cmd->payloadStartIndex] > 100) status = SL_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
      else {
        ok = sw_transport_target(cmd->buffer[cmd->payloadStartIndex]);
        status = ok ? SL_ZIGBEE_ZCL_STATUS_SUCCESS : SL_ZIGBEE_ZCL_STATUS_FAILURE;
      }
      break;
    default: break;
  }
  sl_iostream_printf(sl_iostream_rtt_handle, "ZCL lift cmd=%u status=%u type=%u group=%u\r\n",
                     cmd->commandId, status, cmd->type, cmd->apsFrame->groupId);
  return status;
}

bool sl_zigbee_af_scenes_server_custom_recall_scene_cb(
    const sl_zigbee_af_scene_table_entry_t * const entry, uint16_t transition_ds,
    sl_zigbee_af_status_t * const status)
{
  /* Motor velocity is not programmable: only immediate/native-speed recall. */
  bool immediate = transition_ds == 0 || (transition_ds == 0xFFFF
                    && entry->transitionTime == 0 && entry->transitionTime100ms == 0);
  if (entry->endpoint != ENDPOINT || !immediate
      || !entry->hasCurrentPositionLiftPercentageValue
      || entry->currentPositionLiftPercentageValue > 100
      || entry->hasCurrentPositionTiltPercentageValue) {
    *status = SL_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
  } else if (!sw_transport_target(entry->currentPositionLiftPercentageValue)) {
    *status = SL_ZIGBEE_ZCL_STATUS_FAILURE;
  } else {
    scene_target = entry->currentPositionLiftPercentageValue;
    scene_id = entry->sceneId; scene_group = entry->groupId; pending_scene = true;
    *status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
    sw_zcl_feedback(sw_transport_state());
  }
  sl_iostream_printf(sl_iostream_rtt_handle, "ZCL scene recall group=%u scene=%u target=%u status=%u\r\n",
    entry->groupId, entry->sceneId, entry->currentPositionLiftPercentageValue, *status);
  return true;
}

sl_zigbee_af_status_t sl_zigbee_af_pre_attribute_change_cb(uint8_t endpoint,
    sl_zigbee_af_cluster_id_t cluster, sl_zigbee_af_attribute_id_t attribute,
    uint8_t mask, uint16_t manufacturer, uint8_t type, uint8_t size, uint8_t *value)
{
  if (endpoint == ENDPOINT && cluster == SW_CONFIG_CLUSTER
      && manufacturer == SW_CONFIG_MANUFACTURER
      && (mask & CLUSTER_MASK_SERVER) && attribute == SW_IDLE_POLL_ATTRIBUTE) {
    if (type != ZCL_INT16U_ATTRIBUTE_TYPE || size != 2)
      return SL_ZIGBEE_ZCL_STATUS_INVALID_DATA_TYPE;
    uint16_t interval;
    memcpy(&interval, value, sizeof(interval));
    if (!sw_poll_interval_valid(interval)) return SL_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
  }
  if (endpoint == ENDPOINT && cluster == ZCL_WINDOW_COVERING_CLUSTER_ID
      && manufacturer == 0 && (mask & CLUSTER_MASK_SERVER)
      && attribute == ZCL_MODE_ATTRIBUTE_ID) {
    if (type != ZCL_BITMAP8_ATTRIBUTE_TYPE || size != 1)
      return SL_ZIGBEE_ZCL_STATUS_INVALID_DATA_TYPE;
    if (!sw_cover_mode_valid(value[0])) return SL_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
  }
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

void sl_zigbee_af_post_attribute_change_cb(uint8_t endpoint,
    sl_zigbee_af_cluster_id_t cluster, sl_zigbee_af_attribute_id_t attribute,
    uint8_t mask, uint16_t manufacturer, uint8_t type, uint8_t size, uint8_t *value)
{
  if (endpoint == ENDPOINT && cluster == ZCL_WINDOW_COVERING_CLUSTER_ID
      && manufacturer == 0 && (mask & CLUSTER_MASK_SERVER)
      && attribute == ZCL_MODE_ATTRIBUTE_ID && type == ZCL_BITMAP8_ATTRIBUTE_TYPE
      && size == 1 && sw_cover_mode_valid(value[0])) {
    /* Private persisted shadow keeps the existing poll token B000 stable. */
    sl_zigbee_af_status_t saved = sl_zigbee_af_write_manufacturer_specific_server_attribute(
        ENDPOINT, SW_CONFIG_CLUSTER, SW_SAVED_COVER_MODE_ATTRIBUTE,
        SW_CONFIG_MANUFACTURER, value, ZCL_INT8U_ATTRIBUTE_TYPE);
    if (cover_ready) apply_cover_mode();
    sl_iostream_printf(sl_iostream_rtt_handle, "ZCL covering mode save=%u\r\n", saved);
    sl_iostream_printf(sl_iostream_rtt_handle, "ZCL covering reversed=%u\r\n",
                       (value[0] & SW_COVER_REVERSED_MODE) != 0);
  }
  if (endpoint == ENDPOINT && cluster == SW_CONFIG_CLUSTER
      && manufacturer == SW_CONFIG_MANUFACTURER
      && (mask & CLUSTER_MASK_SERVER) && attribute == SW_IDLE_POLL_ATTRIBUTE
      && type == ZCL_INT16U_ATTRIBUTE_TYPE && size == 2) {
    uint16_t interval;
    memcpy(&interval, value, sizeof(interval));
    if (sw_poll_interval_valid(interval)) {
      sl_zigbee_af_set_long_poll_interval_ms_cb(interval);
      sl_iostream_printf(sl_iostream_rtt_handle, "ZCL idle poll=%u ms\r\n", interval);
    }
  }
}

bool sw_zcl_set_poll_interval_ms(uint16_t interval)
{
  if (!sw_poll_interval_valid(interval)) return false;
  return sl_zigbee_af_write_manufacturer_specific_server_attribute(
      ENDPOINT, SW_CONFIG_CLUSTER, SW_IDLE_POLL_ATTRIBUTE, SW_CONFIG_MANUFACTURER,
      (uint8_t *)&interval, ZCL_INT16U_ATTRIBUTE_TYPE) == SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

void sw_zcl_init(void)
{
  uint8_t mode = 0;
  if (sl_zigbee_af_read_manufacturer_specific_server_attribute(
      ENDPOINT, SW_CONFIG_CLUSTER, SW_SAVED_COVER_MODE_ATTRIBUTE,
      SW_CONFIG_MANUFACTURER, &mode, sizeof(mode)) != SL_ZIGBEE_ZCL_STATUS_SUCCESS
      || !sw_cover_mode_valid(mode)) mode = 0;
  (void)sl_zigbee_af_write_server_attribute(ENDPOINT, ZCL_WINDOW_COVERING_CLUSTER_ID,
      ZCL_MODE_ATTRIBUTE_ID, &mode, ZCL_BITMAP8_ATTRIBUTE_TYPE);
  cover_ready = true;
  apply_cover_mode();
  uint16_t interval = SW_DEFAULT_IDLE_POLL_MS;
  sl_zigbee_af_status_t poll_status = sl_zigbee_af_read_manufacturer_specific_server_attribute(
      ENDPOINT, SW_CONFIG_CLUSTER, SW_IDLE_POLL_ATTRIBUTE, SW_CONFIG_MANUFACTURER,
      (uint8_t *)&interval, sizeof(interval));
  if (poll_status != SL_ZIGBEE_ZCL_STATUS_SUCCESS || !sw_poll_interval_valid(interval))
    interval = SW_DEFAULT_IDLE_POLL_MS;
  sl_zigbee_af_set_long_poll_interval_ms_cb(interval);
  sl_zigbee_af_set_short_poll_interval_ms_cb(SW_FAST_POLL_MS);
  sl_iostream_printf(sl_iostream_rtt_handle, "ZCL idle poll=%u ms; fast poll=%u ms; read=%u\r\n",
                    interval, SW_FAST_POLL_MS, poll_status);
  /* AF dispatch applies endpoint/profile/security/group filters first and
     expands broadcast endpoint 0xFF to the matching local endpoint. */
  sl_status_t status = sl_zigbee_subscribe_to_zcl_commands(
      ZCL_WINDOW_COVERING_CLUSTER_ID, 0xFFFF, ZCL_DIRECTION_CLIENT_TO_SERVER,
      lift_command_parse);
  sl_status_t scene_status = sl_zigbee_subscribe_to_zcl_commands(
      ZCL_SCENES_CLUSTER_ID, 0xFFFF, ZCL_DIRECTION_CLIENT_TO_SERVER,
      scenes_measurement_filter);
  sl_iostream_printf(sl_iostream_rtt_handle, "ZCL scenes filter status=%lu\r\n", (unsigned long)scene_status);
  sl_iostream_printf(sl_iostream_rtt_handle, "ZCL lift subscription status=%lu\r\n", (unsigned long)status);
}
