#!/usr/bin/env python3
"""Read counters over USB EP0 without injecting UART bytes. Requires pyusb."""

import argparse
import json
import struct

import usb.core

FIELDS = (
    "rx_bytes tx_bytes rx_dropped overruns framing_errors parity_errors breaks "
    "high_water rx_queued tx_queued actual_baud rejected_config tx_pin rx_pin connected break_active"
).split()


def device(serial=None):
    matches = list(usb.core.find(find_all=True, idVendor=0x2E8A, idProduct=0x000A))
    if serial:
        matches = [d for d in matches if d.serial_number == serial]
    if len(matches) != 1:
        raise RuntimeError("Specify --serial: exactly one matching bridge required")
    return matches[0]


def snapshot(d):
    words = struct.unpack("<33I", bytes(d.ctrl_transfer(0xA1, 0x40, 0, 5, 132, timeout=2000)))
    if words[0] != 1:
        raise RuntimeError("Unsupported diagnostics version")
    return [dict(zip(FIELDS, words[1 + i * 16 : 17 + i * 16])) for i in range(2)]


def timing(d):
    words = struct.unpack("<8I", bytes(d.ctrl_transfer(0xA1, 0x42, 0, 5, 32, timeout=2000)))
    fields = ("usb_service_gap_us", "usb_blocked_max_us", "usb_rx_written", "usb_write_calls")
    return [dict(zip(fields, words[i * 4 : (i + 1) * 4])) for i in range(2)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--serial")
    p.add_argument(
        "--timing", action="store_true", help="Read USB service/backpressure timing extension"
    )
    p.add_argument(
        "--led",
        type=int,
        choices=range(6),
        help="0 auto; 1 red; 2 green; 3 blue; 4 white; 5 off",
    )
    a = p.parse_args()
    d = device(a.serial)
    if a.led is not None:
        d.ctrl_transfer(0x21, 0x41, a.led, 5, b"", timeout=2000)
    print(json.dumps(timing(d) if a.timing else snapshot(d), indent=2))


if __name__ == "__main__":
    main()
