// SPDX-License-Identifier: MIT
#include "bridge-led.h"
#include "bridge-state.h"
#include <device/usbd_pvt.h>
#include <pico/usb_reset.h>
#include <string.h>
#include <tusb.h>
static void diag_init(void) {}
static void diag_reset(uint8_t rhport)
{
	(void)rhport;
}
static uint16_t diag_open(uint8_t rhport, tusb_desc_interface_t const *itf, uint16_t length)
{
	(void)rhport;
	return itf->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC && itf->bInterfaceSubClass == 0x42 &&
	               length >= 9
	           ? 9
	           : 0;
}
static bool diag_control(uint8_t rhport, uint8_t stage, tusb_control_request_t const *req)
{
	static uint32_t snapshot[33];
	if (stage != CONTROL_STAGE_SETUP)
		return true;
	if (req->wIndex != 5)
		return false;
	if (req->bmRequestType == 0xa1 && req->bRequest == 0x40) {
		snapshot[0] = 1;
		memcpy(&snapshot[1], bridge_stats, sizeof(bridge_stats));
		return tud_control_xfer(rhport, req, snapshot, sizeof(snapshot));
	}
	if (req->bmRequestType == 0x21 && req->bRequest == 0x41 && req->wValue <= 5 &&
	    req->wLength == 0) {
		bridge_led_override = req->wValue;
		return tud_control_status(rhport, req);
	}
	return false;
}
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *count)
{
	static usbd_class_driver_t drivers[2];
	drivers[0] = usb_reset_interface_driver;
	drivers[1] = (usbd_class_driver_t){.name = "bridge diagnostics",
	                                   .init = diag_init,
	                                   .reset = diag_reset,
	                                   .open = diag_open,
	                                   .control_xfer_cb = diag_control};
	*count = 2;
	return drivers;
}
