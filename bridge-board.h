// SPDX-License-Identifier: MIT
#ifndef BRIDGE_BOARD_H
#define BRIDGE_BOARD_H
#ifndef BRIDGE_UART0_TX
#define BRIDGE_UART0_TX 16
#endif
#ifndef BRIDGE_UART0_RX
#define BRIDGE_UART0_RX 17
#endif
#ifndef BRIDGE_UART1_TX
#define BRIDGE_UART1_TX 4
#endif
#ifndef BRIDGE_UART1_RX
#define BRIDGE_UART1_RX 5
#endif
#ifndef BRIDGE_LED_PIN
#ifdef PICO_DEFAULT_LED_PIN
#define BRIDGE_LED_PIN PICO_DEFAULT_LED_PIN
#else
#define BRIDGE_LED_PIN 25
#endif
#endif
#ifndef BRIDGE_RGB_PIN
#define BRIDGE_RGB_PIN -1
#endif
_Static_assert(BRIDGE_UART0_TX == 0 || BRIDGE_UART0_TX == 12 || BRIDGE_UART0_TX == 16 ||
                   BRIDGE_UART0_TX == 28,
               "invalid UART0 TX");
_Static_assert(BRIDGE_UART0_RX == 1 || BRIDGE_UART0_RX == 13 || BRIDGE_UART0_RX == 17 ||
                   BRIDGE_UART0_RX == 29,
               "invalid UART0 RX");
_Static_assert(BRIDGE_UART1_TX == 4 || BRIDGE_UART1_TX == 8 || BRIDGE_UART1_TX == 20 ||
                   BRIDGE_UART1_TX == 24,
               "invalid UART1 TX");
_Static_assert(BRIDGE_UART1_RX == 5 || BRIDGE_UART1_RX == 9 || BRIDGE_UART1_RX == 21 ||
                   BRIDGE_UART1_RX == 25,
               "invalid UART1 RX");
#ifndef BRIDGE_UART0_CTS
#define BRIDGE_UART0_CTS -1
#endif
#ifndef BRIDGE_UART1_CTS
#define BRIDGE_UART1_CTS -1
#endif
#ifndef BRIDGE_UART0_RTS
#define BRIDGE_UART0_RTS -1
#endif
#ifndef BRIDGE_UART1_RTS
#define BRIDGE_UART1_RTS -1
#endif
#ifndef BRIDGE_UART0_DTR
#define BRIDGE_UART0_DTR -1
#endif
#ifndef BRIDGE_UART1_DTR
#define BRIDGE_UART1_DTR -1
#endif
_Static_assert(BRIDGE_UART0_CTS == -1 || BRIDGE_UART0_CTS == 2 || BRIDGE_UART0_CTS == 14 ||
                   BRIDGE_UART0_CTS == 18,
               "invalid UART0 CTS");
_Static_assert(BRIDGE_UART1_CTS == -1 || BRIDGE_UART1_CTS == 6 || BRIDGE_UART1_CTS == 10 ||
                   BRIDGE_UART1_CTS == 22 || BRIDGE_UART1_CTS == 26,
               "invalid UART1 CTS");
#define VALID_EXTRA(p) ((p) >= -1 && (p) <= 29)
_Static_assert(VALID_EXTRA(BRIDGE_UART0_RTS) && VALID_EXTRA(BRIDGE_UART1_RTS) &&
                   VALID_EXTRA(BRIDGE_UART0_DTR) && VALID_EXTRA(BRIDGE_UART1_DTR),
               "invalid flow/control GPIO");
#define PIN_BIT(p) ((p) < 0 ? 0ull : 1ull << ((p) & 31))
#define BITS(op)                                                                                   \
	(PIN_BIT(BRIDGE_UART0_TX) op PIN_BIT(BRIDGE_UART0_RX) op PIN_BIT(BRIDGE_UART1_TX)              \
	     op PIN_BIT(BRIDGE_UART1_RX) op PIN_BIT(BRIDGE_LED_PIN) op PIN_BIT(BRIDGE_RGB_PIN)         \
	         op PIN_BIT(BRIDGE_UART0_CTS) op PIN_BIT(BRIDGE_UART1_CTS)                             \
	             op PIN_BIT(BRIDGE_UART0_RTS) op PIN_BIT(BRIDGE_UART1_RTS)                         \
	                 op PIN_BIT(BRIDGE_UART0_DTR) op PIN_BIT(BRIDGE_UART1_DTR))
_Static_assert(BITS(+) == BITS(|), "GPIO assignments overlap");
#define NOT_UART(p)                                                                                \
	((p) != BRIDGE_UART0_TX && (p) != BRIDGE_UART0_RX && (p) != BRIDGE_UART1_TX &&                 \
	 (p) != BRIDGE_UART1_RX)
_Static_assert(BRIDGE_LED_PIN >= -1 && BRIDGE_LED_PIN <= 29 && NOT_UART(BRIDGE_LED_PIN),
               "LED conflicts with UART or invalid GPIO");
_Static_assert(BRIDGE_RGB_PIN >= -1 && BRIDGE_RGB_PIN <= 29 && NOT_UART(BRIDGE_RGB_PIN),
               "RGB conflicts with UART or invalid GPIO");
_Static_assert(BRIDGE_RGB_PIN < 0 || BRIDGE_RGB_PIN != BRIDGE_LED_PIN, "LED and RGB collide");
#endif
