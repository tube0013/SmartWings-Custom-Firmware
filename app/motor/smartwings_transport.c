#include "smartwings_transport.h"
#include "smartwings_zcl.h"
#include "app/framework/include/af.h"
#include "em_cmu.h"
#include "em_core.h"
#include "gpiointerrupt.h"
#include "em_gpio.h"
#include "em_usart.h"
#include "sl_iostream.h"
#include "sl_iostream_rtt.h"
#include "sl_power_manager.h"
#include "sl_sleeptimer.h"
#include <stdlib.h>
#include <string.h>

#define QUEUE_SIZE 128u
#define QUEUE_MASK (QUEUE_SIZE - 1u)
typedef struct { uint32_t ms; uint8_t byte; } sample_t;
static sample_t rx[QUEUE_SIZE];
static uint8_t tx[QUEUE_SIZE];
static volatile uint32_t rx_head, rx_tail, tx_head, tx_tail, rx_errors;
static uint32_t logged_errors;
static sw_motor_t motor;
static sl_zigbee_af_event_t motor_event;
static char console_line[32];
static uint8_t console_used;
static bool console_overflow;
static volatile bool em1_held;
static volatile uint32_t uart_until_ms, gpio_edges;
static uint32_t em2_entries;
static bool transport_ready;
static uint8_t pending_network_notice;
static sl_power_manager_em_transition_event_handle_t power_handle;
/* Keep USART available for motor response triplets; permit EM2 after quiet. */
#define MOTOR_RX_HOLD_MS 5000u
static void uart_hold(uint32_t ms);
static void motor_network_command(uint8_t opcode);

static uint32_t now_ms(void)
{
  return sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
}

static void uart_hold(uint32_t ms)
{
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  uint32_t deadline = now_ms() + ms;
  if (!em1_held || (int32_t)(deadline - uart_until_ms) > 0) uart_until_ms = deadline;
  if (!em1_held) {
    em1_held = true;
    /* Supported from ISR: SDK restores HF clocks before this returns. */
    sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
  }
  CORE_EXIT_ATOMIC();
}

static void rx_edge(uint8_t pin)
{
  (void)pin;
  ++gpio_edges;
  uart_hold(MOTOR_RX_HOLD_MS);
}

static void power_transition(sl_power_manager_em_t from, sl_power_manager_em_t to)
{
  if (to == SL_POWER_MANAGER_EM2) {
    USART_Enable(USART0, usartDisable);
    ++em2_entries;
  } else if (from == SL_POWER_MANAGER_EM2) {
    /* HF clocks have been restored by the power manager. Registers retained. */
    USART_Enable(USART0, usartEnable);
  }
}
static const sl_power_manager_em_transition_event_info_t power_info = {
  .event_mask = SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM2
              | SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM2,
  .on_event = power_transition
};

void USART0_RX_IRQHandler(void)
{
  uint32_t flags = USART_IntGet(USART0);
  USART_IntClear(USART0, flags & USART_IF_RXOF);
  if (flags & USART_IF_RXOF) ++rx_errors;
  while (USART0->STATUS & USART_STATUS_RXDATAV) {
    uint16_t data = USART_RxDataXGet(USART0);
    uint32_t next = (rx_head + 1u) & QUEUE_MASK;
    if ((data & (USART_RXDATAX_FERR | USART_RXDATAX_PERR)) || next == rx_tail) {
      ++rx_errors;
    } else {
      rx[rx_head].byte = (uint8_t)data;
      rx[rx_head].ms = now_ms();
      __DMB();
      rx_head = next;
    }
  }
}

void USART0_TX_IRQHandler(void)
{
  while ((USART0->STATUS & USART_STATUS_TXBL) && tx_tail != tx_head) {
    USART_Tx(USART0, tx[tx_tail]);
    tx_tail = (tx_tail + 1u) & QUEUE_MASK;
  }
  if (tx_tail == tx_head) USART_IntDisable(USART0, USART_IEN_TXBL);
}

static bool queue_tx(const uint8_t *bytes, size_t size)
{
  uint32_t available = (tx_tail - tx_head - 1u) & QUEUE_MASK;
  if (size > available) return false;
  uart_hold(1000);
  uint32_t head = tx_head;
  for (size_t i = 0; i < size; ++i) {
    tx[head] = bytes[i]; head = (head + 1u) & QUEUE_MASK;
  }
  __DMB(); tx_head = head;
  USART_IntEnable(USART0, USART_IEN_TXBL);
  sl_iostream_printf(sl_iostream_rtt_handle, "MOTOR TX");
  for (size_t i = 0; i < size; ++i) sl_iostream_printf(sl_iostream_rtt_handle, " %02X", bytes[i]);
  sl_iostream_printf(sl_iostream_rtt_handle, "\r\n");
  return true;
}

bool sw_transport_target(uint8_t canonical)
{
  if (canonical > 100 || ((tx_tail - tx_head - 1u) & QUEUE_MASK) < 13) return false;
  uint8_t frame[13];
  bool ok = queue_tx(frame, sw_motor_target(&motor, canonical, frame));
  if (ok) sw_zcl_motion_started();
  return ok;
}

bool sw_transport_stop(void)
{
  uint8_t frame[3];
  if (!queue_tx(frame, sw_motor_short(3, frame))) return false;
  motor.startup_pending = motor.startup_observing = false;
  motor.requested_valid = false;
  sw_zcl_motion_started();
  return true;
}

void sw_transport_set_reversed(bool reversed)
{
  sw_motor_set_reversed(&motor, reversed);
}

const sw_motor_t *sw_transport_state(void) { return &motor; }

bool sw_transport_network_notify(uint8_t opcode)
{
  if (opcode != 0xFA && opcode != 0xFB) return false;
  if (!transport_ready) { pending_network_notice = opcode; return true; }
  uint8_t frame[] = {3, opcode, 3 ^ opcode};
  return queue_tx(frame, sizeof(frame));
}

extern void sw_network_motor_command(uint8_t opcode);
static void motor_network_command(uint8_t opcode)
{
  if (opcode == 0xF9 || opcode == 0xFA || opcode == 0xFB)
    sw_network_motor_command(opcode);
}

static void print_state(void)
{
  sl_iostream_printf(sl_iostream_rtt_handle,
    "MOTOR STATE accepted=%lu rejected=%lu uart_errors=%lu position=%u position_valid=%u raw=%u battery_zcl=%u battery_valid=%u requested=%u requested_valid=%u\r\n",
    (unsigned long)motor.accepted_frames, (unsigned long)motor.rejected_frames,
    (unsigned long)rx_errors, motor.position, motor.position_valid, motor.raw_position,
    motor.battery_zcl, motor.battery_valid, motor.requested_position, motor.requested_valid);
  sl_iostream_printf(sl_iostream_rtt_handle,
    "POWER em2_entries=%lu gpio_edges=%lu uart_em1=%u long_poll_ms=%lu short_poll_ms=%u network=%u\r\n",
    (unsigned long)em2_entries, (unsigned long)gpio_edges, em1_held,
    (unsigned long)sl_zigbee_af_get_long_poll_interval_ms_cb(),
    sl_zigbee_af_get_short_poll_interval_ms_cb(), sl_zigbee_network_state());
}

/* Bench control lives exclusively on SWD RTT, never on the motor UART. */
static void console_command(void)
{
  console_line[console_used] = 0;
  bool ok = true;
  if (!strcmp(console_line, "state")) print_state();
  else if (!strcmp(console_line, "stop")) ok = sw_transport_stop();
  else if (!strcmp(console_line, "open")) ok = sw_transport_target(0);
  else if (!strcmp(console_line, "close")) ok = sw_transport_target(100);
  else if (!strncmp(console_line, "poll ", 5)) {
    char *end; const char *number = console_line + 5;
    long interval = strtol(number, &end, 10);
    ok = end != number && !*end && interval >= 0 && interval <= UINT16_MAX;
    if (ok) ok = sw_zcl_set_poll_interval_ms((uint16_t)interval);
    if (ok) print_state();
  }
  else if (!strncmp(console_line, "target ", 7)) {
    char *end; const char *number = console_line + 7;
    long target = strtol(number, &end, 10);
    ok = end != number && !*end && target >= 0 && target <= 100;
    if (ok) ok = sw_transport_target((uint8_t)target);
  } else ok = false;
  sl_iostream_printf(sl_iostream_rtt_handle, "BENCH command %s\r\n", ok ? "OK" : "REJECTED");
}

void sw_transport_process_action(void)
{
  if (rx_errors != logged_errors) {
    logged_errors = rx_errors;
    motor.used = 0; ++motor.rejected_frames;
    sl_iostream_printf(sl_iostream_rtt_handle, "MOTOR UART error count=%lu\r\n", (unsigned long)rx_errors);
  }
  while (rx_tail != rx_head) {
    __DMB(); sample_t sample = rx[rx_tail];
    __DMB(); rx_tail = (rx_tail + 1u) & QUEUE_MASK;
    uint32_t previous = motor.accepted_frames;
    sw_motor_rx(&motor, sample.byte, sample.ms);
    if (motor.accepted_frames != previous) {
      motor_network_command(motor.last_opcode);
      print_state(); sw_zcl_feedback(&motor);
    }
  }
  uint8_t input[32]; size_t size = 0;
  if (sl_iostream_read(sl_iostream_rtt_handle, input, sizeof(input), &size) == SL_STATUS_OK) {
    for (size_t i = 0; i < size; ++i) {
      uint8_t c = input[i];
      if (c == '\n' || c == '\r') {
        if (console_used && !console_overflow) console_command();
        else if (console_overflow) sl_iostream_printf(sl_iostream_rtt_handle, "BENCH command REJECTED\r\n");
        console_used = 0; console_overflow = false;
      } else if (console_used + 1u < sizeof(console_line)) console_line[console_used++] = (char)c;
      else console_overflow = true;
    }
  }
  if (sw_motor_startup_tick(&motor, now_ms())) {
    uint8_t frame[3]; queue_tx(frame, sw_motor_short(3, frame));
    sl_iostream_printf(sl_iostream_rtt_handle, "MOTOR startup Stop once\r\n");
  }
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  if (em1_held && (int32_t)(now_ms() - uart_until_ms) >= 0
      && tx_head == tx_tail && rx_head == rx_tail
      && (USART0->STATUS & USART_STATUS_TXC)
      && !(USART0->STATUS & USART_STATUS_RXDATAV)
      && GPIO_PinInGet(gpioPortA, 6)) {
    em1_held = false;
    sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
  }
  CORE_EXIT_ATOMIC();
}

static void motor_event_handler(sl_zigbee_af_event_t *event)
{
  /* RTT/startup servicing only; RX is also drained from every main-loop wake. */
  sl_zigbee_af_event_set_delay_ms(event, 250);
  sw_transport_process_action();
}

void sw_transport_init(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_USART0, true);
  GPIO_PinModeSet(gpioPortA, 5, gpioModePushPull, 1);
  GPIO_PinModeSet(gpioPortA, 6, gpioModeInputPull, 1);
  GPIO->USARTROUTE[0].TXROUTE = (gpioPortA << _GPIO_USART_TXROUTE_PORT_SHIFT) | (5u << _GPIO_USART_TXROUTE_PIN_SHIFT);
  GPIO->USARTROUTE[0].RXROUTE = (gpioPortA << _GPIO_USART_RXROUTE_PORT_SHIFT) | (6u << _GPIO_USART_RXROUTE_PIN_SHIFT);
  GPIO->USARTROUTE[0].ROUTEEN = GPIO_USART_ROUTEEN_TXPEN | GPIO_USART_ROUTEEN_RXPEN;
  USART_InitAsync_TypeDef config = USART_INITASYNC_DEFAULT;
  config.baudrate = 9600;
  USART_InitAsync(USART0, &config);
  /* PA6 falling edge precedes captured motor frames by a zero-prefix gap. */
  GPIOINT_Init();
  GPIOINT_CallbackRegister(6, rx_edge);
  GPIO_ExtIntConfig(gpioPortA, 6, 6, false, true, true);
  sl_power_manager_subscribe_em_transition_event(&power_handle, &power_info);
  uart_hold(3000);
  sw_motor_init(&motor, sw_zcl_motor_reversed(), now_ms());
  USART_IntClear(USART0, _USART_IF_MASK);
  NVIC_ClearPendingIRQ(USART0_RX_IRQn); NVIC_ClearPendingIRQ(USART0_TX_IRQn);
  NVIC_EnableIRQ(USART0_RX_IRQn); NVIC_EnableIRQ(USART0_TX_IRQn);
  USART_IntEnable(USART0, USART_IEN_RXDATAV | USART_IEN_RXOF);
  transport_ready = true;
  if ((pending_network_notice == 0xFA && sl_zigbee_network_state() == SL_ZIGBEE_JOINED_NETWORK)
      || (pending_network_notice == 0xFB && sl_zigbee_network_state() == SL_ZIGBEE_NO_NETWORK))
    sw_transport_network_notify(pending_network_notice);
  pending_network_notice = 0;
  sl_zigbee_af_event_init(&motor_event, motor_event_handler);
  sl_zigbee_af_event_set_delay_ms(&motor_event, 250);
  sl_iostream_printf(sl_iostream_rtt_handle, "MOTOR UART USART0 PA5 TX / PA6 RX 9600 8N1; axis from saved Mode; EM2 GPIO wake, RX hold5s\r\n");
}
