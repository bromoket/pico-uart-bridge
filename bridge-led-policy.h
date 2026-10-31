// SPDX-License-Identifier: MIT
#ifndef BRIDGE_LED_POLICY_H
#define BRIDGE_LED_POLICY_H
#include <stdbool.h>
#include <stdint.h>
/* RRGGBB, dim on purpose. Errors override activity; idle has a soft blue breath. */
static inline uint32_t bridge_color(bool connected, bool rx, bool tx, bool error, uint32_t ms)
{
	if (error)
		return 0x200000;
	if (rx && tx)
		return 0x202010;
	if (rx)
		return 0x002000;
	if (tx)
		return 0x200010;
	if (!connected)
		return 0x000002;
	uint32_t phase = (ms / 32) % 64;
	return 0x000004 + (phase < 32 ? phase : 63 - phase) / 2;
}
#endif
