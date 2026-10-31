// SPDX-License-Identifier: MIT
/* Based on Noltari/pico-uart-bridge, Copyright 2021 Álvaro Fernández Rojas.
 * FIFO and product identification derived from jylho/picouart source patches.
 * SPSC transport and diagnostics: fork implementation, 2026. */
#include "bridge-board.h"
#include "bridge-led.h"
#include "bridge-ring.h"
#include "bridge-state.h"
#include <hardware/clocks.h>
#include <hardware/irq.h>
#include <hardware/uart.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <string.h>
#include <tusb.h>

#define DEF_BIT_RATE 115200
#define LOAD(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define SAVE(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)

typedef struct {
	uart_inst_t *inst;
	uint tx_pin, rx_pin;
	int cts_pin, rts_pin, dtr_pin;
	uint32_t host_rts;
	bridge_ring_t rx, tx;
	cdc_line_coding_t requested, applied;
	mutex_t config_lock; /* Only foreground code; never used from an IRQ. */
	uint32_t break_request, break_deadline;
	bool timed_break;
} bridge_port_t;
static bridge_port_t ports[2];
bridge_stats_t bridge_stats[2];

static void receive(uint index)
{
	bridge_port_t *p = &ports[index];
	bridge_stats_t *s = &bridge_stats[index];
	/* Bounded work so an endless/faulty source cannot starve the other port. */
	for (uint budget = 0; budget < 128 && uart_is_readable(p->inst); budget++) {
		uint32_t dr = uart_get_hw(p->inst)->dr;
		s->rx_bytes++;
		if (dr & UART_UARTDR_OE_BITS)
			s->overruns++;
		if (dr & UART_UARTDR_FE_BITS)
			s->framing_errors++;
		if (dr & UART_UARTDR_PE_BITS)
			s->parity_errors++;
		if (dr & UART_UARTDR_BE_BITS)
			s->breaks++;
		if (!ring_push(&p->rx, (uint8_t)dr))
			s->rx_dropped++;
		uint32_t used = ring_count(&p->rx);
		if (used > s->high_water)
			s->high_water = used;
	}
	uart_get_hw(p->inst)->rsr = 0;
	uart_get_hw(p->inst)->icr = UART_UARTICR_RTIC_BITS | UART_UARTICR_OEIC_BITS;
}
static void irq0(void)
{
	receive(0);
}
static void irq1(void)
{
	receive(1);
}

static bool valid_coding(cdc_line_coding_t const *lc)
{
	uint32_t clock = clock_get_hz(clk_peri);
	return lc->bit_rate >= clock / (16u * 65535u) + 1 && lc->bit_rate <= clock / 16u &&
	       lc->data_bits >= 5 && lc->data_bits <= 8 && lc->parity <= 4 &&
	       (lc->stop_bits == 0 || lc->stop_bits == 2);
}
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *lc)
{
	if (itf >= 2)
		return;
	if (!valid_coding(lc)) {
		bridge_stats[itf].rejected_config++;
		return;
	}
	/* Linux can SET_LINE_CODING(9600) during enumeration without an open
	 * console. Preserve the 115200 boot-capture default until a real client
	 * asserts DTR or sends OUT data. DTR-less clients are handled in usb_port. */
	if (tud_cdc_n_connected(itf)) {
		mutex_enter_blocking(&ports[itf].config_lock);
		ports[itf].requested = *lc;
		mutex_exit(&ports[itf].config_lock);
	}
}
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
	if (itf >= 2)
		return;
	bridge_port_t *p = &ports[itf];
	SAVE(&p->host_rts, rts);
	if (p->dtr_pin >= 0)
		gpio_put(p->dtr_pin, !dtr); /* Active-low modem signal. */
}
void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms)
{
	if (itf < 2)
		SAVE(&ports[itf].break_request, 0x10000u | duration_ms);
}
static void update_config(uint index)
{
	bridge_port_t *p = &ports[index];
	if (mutex_try_enter(&p->config_lock, NULL)) {
		if (memcmp(&p->requested, &p->applied, sizeof(p->applied)) &&
		    !(uart_get_hw(p->inst)->fr & UART_UARTFR_BUSY_BITS) && !ring_count(&p->tx)) {
			cdc_line_coding_t lc = p->requested;
			irq_set_enabled(index ? UART1_IRQ : UART0_IRQ, false);
			bridge_stats[index].actual_baud = uart_set_baudrate(p->inst, lc.bit_rate);
			uart_parity_t parity =
			    lc.parity == 0
			        ? UART_PARITY_NONE
			        : (lc.parity == 1 || lc.parity == 3 ? UART_PARITY_ODD : UART_PARITY_EVEN);
			uart_set_format(p->inst, lc.data_bits, lc.stop_bits == 2 ? 2 : 1, parity);
			if (lc.parity >= 3)
				hw_set_bits(&uart_get_hw(p->inst)->lcr_h, UART_UARTLCR_H_SPS_BITS);
			p->applied = lc;
			irq_set_enabled(index ? UART1_IRQ : UART0_IRQ, true);
		}
		mutex_exit(&p->config_lock);
	}
	uint32_t request =
	    LOAD(&p->break_request) ? __atomic_exchange_n(&p->break_request, 0, __ATOMIC_ACQ_REL) : 0;
	if (request) {
		uint16_t duration = (uint16_t)request;
		uart_set_break(p->inst, duration != 0);
		bridge_stats[index].break_active = duration != 0;
		p->timed_break = duration != 0 && duration != 0xffff;
		p->break_deadline = time_us_32() + duration * 1000u;
	}
	if (p->timed_break && (int32_t)(time_us_32() - p->break_deadline) >= 0) {
		uart_set_break(p->inst, false);
		p->timed_break = false;
		bridge_stats[index].break_active = 0;
	}
}
static void usb_port(uint index)
{
	bridge_port_t *p = &ports[index];
	/* USB OUT naturally backpressures once the software TX ring is full. */
	uint32_t space = BRIDGE_RING_SIZE - ring_count(&p->tx);
	uint8_t buffer[64];
	uint32_t count = tud_cdc_n_available(index);
	bool configured = true;
	if (tud_cdc_n_connected(index) || count) {
		cdc_line_coding_t lc;
		tud_cdc_n_get_line_coding(index, &lc);
		if (valid_coding(&lc)) {
			mutex_enter_blocking(&p->config_lock);
			p->requested = lc;
			configured = !memcmp(&lc, &p->applied, sizeof(lc));
			mutex_exit(&p->config_lock);
		} else {
			configured = false;
		}
	}
	/* Never enqueue new-format bytes until the foreground core has applied it. */
	if (!configured)
		count = 0;
	if (count > sizeof(buffer))
		count = sizeof(buffer);
	if (count > space)
		count = space;
	if (count) {
		count = tud_cdc_n_read(index, buffer, count);
		for (uint32_t i = 0; i < count; i++)
			ring_push(&p->tx, buffer[i]);
	}
	/* USB is serviced regardless of DTR: some valid clients don't assert it. */
	uint32_t tail = LOAD(&p->rx.tail), head = LOAD(&p->rx.head);
	count = head - tail;
	uint32_t contiguous = BRIDGE_RING_SIZE - (tail & (BRIDGE_RING_SIZE - 1));
	if (count > contiguous)
		count = contiguous;
	if (count > 512)
		count = 512;
	if (count) {
		uint32_t sent = tud_cdc_n_write(index, &p->rx.bytes[tail & (BRIDGE_RING_SIZE - 1)], count);
		SAVE(&p->rx.tail, tail + sent);
		if (sent)
			tud_cdc_n_write_flush(index);
	}
	bridge_stats[index].connected = tud_cdc_n_connected(index);
}
static void usb_core(void)
{
	tusb_init();
	bridge_led_init();
	while (true) {
		tud_task();
		if (tud_mounted())
			for (uint i = 0; i < 2; i++)
				usb_port(i);
		bridge_led_update();
	}
}
int main(void)
{
	usbd_serial_init();
	uint tx[2] = {BRIDGE_UART0_TX, BRIDGE_UART1_TX}, rx[2] = {BRIDGE_UART0_RX, BRIDGE_UART1_RX};
	int cts[2] = {BRIDGE_UART0_CTS, BRIDGE_UART1_CTS},
	    rts[2] = {BRIDGE_UART0_RTS, BRIDGE_UART1_RTS},
	    dtr[2] = {BRIDGE_UART0_DTR, BRIDGE_UART1_DTR};
	for (uint i = 0; i < 2; i++) {
		bridge_port_t *p = &ports[i];
		p->inst = i ? uart1 : uart0;
		p->tx_pin = tx[i];
		p->rx_pin = rx[i];
		p->cts_pin = cts[i];
		p->rts_pin = rts[i];
		p->dtr_pin = dtr[i];
		p->host_rts = 1;
		if (cts[i] >= 0) {
			gpio_pull_up(cts[i]);
			gpio_set_function(cts[i], GPIO_FUNC_UART);
		}
		if (rts[i] >= 0) {
			gpio_init(rts[i]);
			gpio_put(rts[i], false);
			gpio_set_dir(rts[i], GPIO_OUT);
		}
		if (dtr[i] >= 0) {
			gpio_init(dtr[i]);
			gpio_put(dtr[i], true);
			gpio_set_dir(dtr[i], GPIO_OUT);
		}
		/* Select a pulled-up RX before enabling UART, otherwise its unmapped
		 * input reads low and creates a spurious startup BREAK. Hold TX idle. */
		gpio_pull_up(rx[i]);
		gpio_set_function(rx[i], GPIO_FUNC_UART);
		gpio_init(tx[i]);
		gpio_put(tx[i], true);
		gpio_set_dir(tx[i], GPIO_OUT);
		mutex_init(&p->config_lock);
		p->requested = (cdc_line_coding_t){
		    .bit_rate = DEF_BIT_RATE, .stop_bits = 0, .parity = 0, .data_bits = 8};
		p->applied = p->requested;
		bridge_stats[i].actual_baud = uart_init(p->inst, DEF_BIT_RATE);
		uart_set_hw_flow(p->inst, cts[i] >= 0, false);
		uart_set_format(p->inst, 8, 1, UART_PARITY_NONE);
		uart_set_translate_crlf(p->inst, false);
		uart_set_fifo_enabled(p->inst, true);
		hw_write_masked(&uart_get_hw(p->inst)->ifls, 2u << UART_UARTIFLS_RXIFLSEL_LSB,
		                UART_UARTIFLS_RXIFLSEL_BITS);
		gpio_set_function(tx[i], GPIO_FUNC_UART);
		gpio_set_function(rx[i], GPIO_FUNC_UART);
		gpio_pull_up(rx[i]);
		bridge_stats[i].tx_pin = tx[i];
		bridge_stats[i].rx_pin = rx[i];
		irq_set_exclusive_handler(i ? UART1_IRQ : UART0_IRQ, i ? irq1 : irq0);
		uart_set_irq_enables(p->inst, true, false);
		hw_set_bits(&uart_get_hw(p->inst)->imsc, UART_UARTIMSC_RTIM_BITS | UART_UARTIMSC_OEIM_BITS);
		irq_set_enabled(i ? UART1_IRQ : UART0_IRQ, true);
	}
	multicore_launch_core1(usb_core);
	while (true)
		for (uint i = 0; i < 2; i++) {
			bridge_port_t *p = &ports[i];
			update_config(i);
			if (!bridge_stats[i].break_active) {
				uint8_t byte;
				for (uint budget = 0; budget < 32 && uart_is_writable(p->inst); budget++) {
					if (!ring_pop(&p->tx, &byte))
						break;
					uart_putc_raw(p->inst, byte);
					bridge_stats[i].tx_bytes++;
				}
			}
			uint32_t used = ring_count(&p->rx);
			if (used > BRIDGE_RING_SIZE)
				used = BRIDGE_RING_SIZE; /* Observer snapshot can race an IRQ. */
			if (p->rts_pin >= 0) {
				/* Early software RTS accounts for the RAM ring, not merely the 32-byte FIFO.
				 * Peer must honour active-low RTS; hysteresis leaves 4 KiB response margin. */
				if (!LOAD(&p->host_rts) || used >= BRIDGE_RING_SIZE - 4096)
					gpio_put(p->rts_pin, true);
				else if (used < BRIDGE_RING_SIZE / 2)
					gpio_put(p->rts_pin, false);
			}
			bridge_stats[i].rx_queued = used;
			bridge_stats[i].tx_queued = ring_count(&p->tx);
		}
}
