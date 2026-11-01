// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2021 Álvaro Fernández Rojas <noltari@gmail.com>
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2020 Damien P. George
 */

#if !defined(_TUSB_CONFIG_H_)
#define _TUSB_CONFIG_H_

#include <tusb_option.h>

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE

#define CFG_TUD_CDC 2
#define CFG_TUD_CDC_RX_BUFSIZE 1024
#define CFG_TUD_CDC_TX_BUFSIZE 1024
/* Software transfer buffer, not USB wMaxPacketSize (which stays 64 at FS).
 * Multi-packet IN transfers let RP2040 use its double-buffered bulk path. */
#define CFG_TUD_CDC_EP_BUFSIZE 512
#define CFG_TUD_CDC_EP_OUT_BUFSIZE 64

void usbd_serial_init(void);

#endif /* _TUSB_CONFIG_H_ */
