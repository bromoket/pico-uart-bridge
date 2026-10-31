// SPDX-License-Identifier: MIT
#include "bridge-led.h"
#include "bridge-board.h"
#include "bridge-led-policy.h"
#include "bridge-rgb.pio.h"
#include "bridge-state.h"
#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <pico/stdlib.h>
#include <tusb.h>
uint32_t bridge_led_override;
static int sm = -1;
void bridge_led_init(void)
{
	if (BRIDGE_LED_PIN >= 0) {
		gpio_init(BRIDGE_LED_PIN);
		gpio_set_dir(BRIDGE_LED_PIN, GPIO_OUT);
	}
	if (BRIDGE_RGB_PIN >= 0) {
		sm = pio_claim_unused_sm(pio0, false);
		if (sm < 0)
			return;
		uint offset = pio_add_program(pio0, &bridge_rgb_program);
		pio_sm_config c = bridge_rgb_program_get_default_config(offset);
		sm_config_set_sideset_pins(&c, BRIDGE_RGB_PIN);
		sm_config_set_out_shift(&c, false, true, 24);
		sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
		sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (800000 * 10));
		pio_gpio_init(pio0, BRIDGE_RGB_PIN);
		pio_sm_set_consecutive_pindirs(pio0, sm, BRIDGE_RGB_PIN, 1, true);
		pio_sm_init(pio0, sm, offset, &c);
		pio_sm_set_enabled(pio0, sm, true);
	}
}
void bridge_led_update(void)
{
	static uint32_t next, last_rx, last_tx, last_errors, rx_at, tx_at, error_at;
	static bool rx_seen, tx_seen, error_seen;
	uint32_t now = time_us_32();
	if ((int32_t)(now - next) < 0)
		return;
	next = now + 20000;
	uint32_t rx = bridge_stats[0].rx_bytes + bridge_stats[1].rx_bytes;
	uint32_t tx = bridge_stats[0].tx_bytes + bridge_stats[1].tx_bytes;
	uint32_t errors = 0;
	for (uint i = 0; i < 2; i++)
		errors += bridge_stats[i].rx_dropped + bridge_stats[i].overruns +
		          bridge_stats[i].framing_errors + bridge_stats[i].parity_errors;
	if (rx != last_rx) {
		rx_at = now;
		rx_seen = true;
	}
	if (tx != last_tx) {
		tx_at = now;
		tx_seen = true;
	}
	if (errors != last_errors) {
		error_at = now;
		error_seen = true;
	}
	last_rx = rx;
	last_tx = tx;
	last_errors = errors;
	bool receiving = rx_seen && (uint32_t)(now - rx_at) < 80000;
	bool sending = tx_seen && (uint32_t)(now - tx_at) < 80000;
	if (!receiving)
		rx_seen = false;
	if (!sending)
		tx_seen = false;
	bool fault = error_seen && (uint32_t)(now - error_at) < 500000;
	if (!fault)
		error_seen = false;
	bool connected = bridge_stats[0].connected || bridge_stats[1].connected;
	uint32_t color = bridge_color(connected, receiving, sending, fault, now / 1000);
	if (bridge_led_override) {
		const uint32_t colors[] = {0, 0x200000, 0x002000, 0x000020, 0x202020, 0};
		color = colors[bridge_led_override <= 5 ? bridge_led_override : 5];
	}
	if (BRIDGE_LED_PIN >= 0)
		gpio_put(BRIDGE_LED_PIN, receiving || sending || connected);
	/* One word every 20 ms, no blocking or interrupt masking. PIO supplies timing. */
	if (sm >= 0 && !pio_sm_is_tx_fifo_full(pio0, sm)) {
		uint32_t grb = ((color & 0xff00) << 8) | ((color & 0xff0000) >> 8) | (color & 0xff);
		pio_sm_put(pio0, sm, grb << 8);
	}
}
