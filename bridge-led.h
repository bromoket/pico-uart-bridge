// SPDX-License-Identifier: MIT
#ifndef BRIDGE_LED_H
#define BRIDGE_LED_H
#include <stdint.h>
void bridge_led_init(void);
void bridge_led_update(void);
extern uint32_t bridge_led_override;
#endif
