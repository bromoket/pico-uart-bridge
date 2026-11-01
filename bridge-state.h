// SPDX-License-Identifier: MIT
#ifndef BRIDGE_STATE_H
#define BRIDGE_STATE_H
#include <stdint.h>
typedef struct {
	/* Aligned ARM word loads/stores; each field has a single writer. Snapshot
	 * fields are individually coherent, not a transaction across both cores. */
	volatile uint32_t rx_bytes, tx_bytes, rx_dropped, overruns;
	volatile uint32_t framing_errors, parity_errors, breaks, high_water;
	volatile uint32_t rx_queued, tx_queued, actual_baud, rejected_config;
	volatile uint32_t tx_pin, rx_pin, connected, break_active;
} bridge_stats_t;
typedef struct {
	volatile uint32_t usb_service_gap_us, usb_blocked_max_us;
	volatile uint32_t usb_rx_written, usb_write_calls;
} bridge_timing_t;
extern bridge_stats_t bridge_stats[2];
extern bridge_timing_t bridge_timing[2];
#endif
