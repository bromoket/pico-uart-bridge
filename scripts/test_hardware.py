#!/usr/bin/env python3
"""Destructive-to-stream loopback tests. NEVER point this at a live console.
Requires explicit if02 loopback, pyserial/pyusb; results contain counts, not data.
"""

import argparse, json, os, threading, time
from bridge_serial import BridgeSerial
from bridge_diag import device, snapshot


def transfer(s, payload, timeout):
    got = bytearray()
    errors = []
    stop = threading.Event()

    def read():
        try:
            while not stop.is_set() and len(got) < len(payload):
                got.extend(s.read(min(4096, len(payload) - len(got))))
        except Exception as e:
            errors.append(str(e))

    thread = threading.Thread(target=read, daemon=True)
    thread.start()
    start = time.monotonic()
    try:
        written = s.write(payload)
        thread.join(timeout)
        if thread.is_alive():
            raise AssertionError(f"read deadline; received {len(got)}/{len(payload)}")
        assert not errors, errors
        assert written == len(payload) and got == payload, (
            f"mismatch: {written=} received={len(got)}"
        )
        return time.monotonic() - start
    finally:
        stop.set()
        thread.join(1)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--loopback-port", required=True)
    p.add_argument("--output", required=True)
    a = p.parse_args()
    if not a.loopback_port.endswith("-if02"):
        p.error(
            "Use the stable /dev/serial/by-id/...-if02 path, physically looped GP4/5"
        )
    d = device()
    results = []

    def record(name, fn):
        start = time.monotonic()
        try:
            details = fn()
            row = {
                "name": name,
                "pass": True,
                "seconds": round(time.monotonic() - start, 3),
                "details": details,
            }
        except Exception as e:
            row = {"name": name, "pass": False, "error": str(e)}
        results.append(row)
        print(json.dumps(row), flush=True)
        with open(a.output, "w") as f:
            json.dump({"tests": results, "final_counters": snapshot(d)}, f, indent=2)
        if not row["pass"]:
            raise SystemExit(1)

    def stream(baud, size, trials=1, dtr=True, parity="N", bits=8, stopbits=1):
        with BridgeSerial(
            a.loopback_port,
            baud,
            timeout=0.05,
            write_timeout=30,
            parity=parity,
            bytesize=bits,
            stopbits=stopbits,
            exclusive=True,
        ) as s:
            s.dtr = dtr
            time.sleep(0.15)
            s.reset_input_buffer()
            before = snapshot(d)[1]
            total = 0
            for _ in range(trials):
                payload = os.urandom(size)
                if bits < 8:
                    payload = bytes(x & ((1 << bits) - 1) for x in payload)
                transfer(s, payload, max(5, size * 12 / baud * 3))
                total += size
            after = snapshot(d)[1]
            assert (
                after["rx_dropped"] == before["rx_dropped"]
                and after["overruns"] == before["overruns"]
            ), (before, after)
            assert (
                after["framing_errors"] == before["framing_errors"]
                and after["parity_errors"] == before["parity_errors"]
            ), (before, after)
            return {
                "bytes": total,
                "baud_requested": baud,
                "baud_actual": after["actual_baud"],
                "data_equal": True,
            }

    for baud in (
        9600,
        57600,
        115200,
        230400,
        460800,
        518400,
        921600,
        1000000,
        1036800,
        2000000,
        3000000,
        4000000,
        6000000,
        7500000,
    ):
        record(f"stream-{baud}", lambda b=baud: stream(b, 8192, 3))
    for size in (
        1,
        2,
        3,
        7,
        16,
        31,
        32,
        33,
        63,
        64,
        65,
        255,
        256,
        1024,
        16384,
        65536,
        1048576,
    ):
        record(f"boundary-{size}", lambda n=size: stream(1000000, n, 2))
    record("DTR-low", lambda: stream(115200, 16384, dtr=False))
    for bits, parity, stopbits in (
        (5, "N", 1),
        (6, "E", 1),
        (7, "O", 2),
        (8, "E", 1),
        (8, "O", 1),
        (8, "M", 1),
        (8, "S", 1),
        (8, "N", 2),
    ):
        record(
            f"coding-{bits}{parity}{stopbits}",
            lambda b=bits, p=parity, st=stopbits: stream(
                115200, 4096, bits=b, parity=p, stopbits=st
            ),
        )

    def stalled():
        with BridgeSerial(
            a.loopback_port, 1000000, timeout=0.1, write_timeout=5, exclusive=True
        ) as s:
            time.sleep(0.15)
            s.reset_input_buffer()
            before = snapshot(d)[1]
            payload = os.urandom(16384)
            assert s.write(payload) == len(payload)
            time.sleep(0.5)
            got = s.read(len(payload))
            after = snapshot(d)[1]
            assert got == payload and after["rx_dropped"] == before["rx_dropped"]
            return {"bytes": len(payload), "stall_seconds": 0.5, "data_equal": True}

    record("finite-host-stall", stalled)

    def overflow():
        with BridgeSerial(
            a.loopback_port, 1000000, timeout=0.1, write_timeout=10, exclusive=True
        ) as s:
            time.sleep(0.15)
            s.reset_input_buffer()
            before = snapshot(d)[1]
            s.write(os.urandom(131072))
            time.sleep(2)
            after = snapshot(d)[1]
            assert after["rx_dropped"] > before["rx_dropped"], (before, after)
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                if not s.read(4096) and not snapshot(d)[1]["rx_queued"]:
                    break
            return {
                "intentional_drops": after["rx_dropped"] - before["rx_dropped"],
                "deadlock": False,
            }

    record("intentional-overflow", overflow)
    record("post-overflow-recovery", lambda: stream(115200, 32768, 3))

    def break_test():
        # Linux owns CDC interface 2. Close it and temporarily detach only the
        # loopback driver, never the router CDC interface 0.
        detached = d.is_kernel_driver_active(2)
        if detached:
            d.detach_kernel_driver(2)
        try:
            d.ctrl_transfer(0x21, 0x23, 200, 2, b"")
            time.sleep(0.02)
            assert snapshot(d)[1]["break_active"] == 1
            time.sleep(0.25)
            assert snapshot(d)[1]["break_active"] == 0
        finally:
            import usb.util

            usb.util.release_interface(d, 2)
            if detached:
                d.attach_kernel_driver(2)
        deadline = time.monotonic() + 5
        while not os.path.exists(a.loopback_port) and time.monotonic() < deadline:
            time.sleep(0.1)
        return {"timed_break_ms": 200, "released": True}

    record("timed-break", break_test)
    record("post-break-recovery", lambda: stream(115200, 8192))
    for i in range(25):
        record(f"reopen-{i}", lambda: stream(115200, 1024))
    print("ALL TESTS PASSED")


if __name__ == "__main__":
    main()
