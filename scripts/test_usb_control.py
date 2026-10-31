#!/usr/bin/env python3
"""Explicitly authorized USB-control test: detaches only CDC1, then resets Pico.
No target UART bytes are sent; connected target must tolerate its UART adapter
being rebooted. Run with no capture/terminal open and GP4/5 physically looped.
"""

import argparse
import json
import struct
import subprocess
import time

import usb.util
from bridge_diag import device, snapshot


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--serial", required=True)
    p.add_argument("--output", required=True)
    a = p.parse_args()
    d = device(a.serial)
    results = []
    detached = d.is_kernel_driver_active(2)
    if detached:
        d.detach_kernel_driver(2)
    try:
        before = snapshot(d)[1]
        invalid = (
            (0, 0, 0, 8),
            (115200, 1, 0, 8),
            (115200, 0, 5, 8),
            (115200, 0, 0, 9),
            (8000000, 0, 0, 8),
        )
        for coding in invalid:
            d.ctrl_transfer(0x21, 0x20, 0, 2, struct.pack("<IBBB", *coding))
            time.sleep(0.02)
        after = snapshot(d)[1]
        assert after["rejected_config"] - before["rejected_config"] == len(invalid)
        assert after["actual_baud"] == before["actual_baud"]
        results.append({"name": "invalid-coding", "pass": True, "rejected": len(invalid)})
        d.ctrl_transfer(0x21, 0x20, 0, 2, struct.pack("<IBBB", 115200, 0, 0, 8))
        d.ctrl_transfer(0x21, 0x23, 65535, 2, b"")
        time.sleep(0.05)
        assert snapshot(d)[1]["break_active"] == 1
        d.ctrl_transfer(0x21, 0x23, 0, 2, b"")
        time.sleep(0.05)
        assert snapshot(d)[1]["break_active"] == 0
        results.append({"name": "continuous-break-clear", "pass": True})
    finally:
        usb.util.release_interface(d, 2)
        if detached:
            d.attach_kernel_driver(2)
        usb.util.dispose_resources(d)
    for i in range(5):
        subprocess.run(
            ["picotool", "info", "-a", "-f", "--ser", a.serial],
            capture_output=True,
            check=True,
            timeout=20,
        )
        deadline = time.monotonic() + 10
        while True:
            try:
                d = device(a.serial)
                counters = snapshot(d)
                break
            except (usb.core.USBError, RuntimeError, ValueError):
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.1)
        assert all(
            c["actual_baud"] == 115207
            and c["rx_dropped"] == 0
            and c["overruns"] == 0
            and c["breaks"] == 0
            for c in counters
        )
        results.append(
            {"name": f"remote-BOOTSEL-roundtrip-{i}", "pass": True, "boot_baud_preserved": True}
        )
        usb.util.dispose_resources(d)
    with open(a.output, "w") as f:
        json.dump(results, f, indent=2)
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
