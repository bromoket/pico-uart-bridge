#!/usr/bin/env python3
"""Bounded binary serial capture with private files, exclusive access, no console dump."""

import argparse
import hashlib
import json
import os
import time

from bridge_serial import BridgeSerial


def private_output(path):
    return os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), "wb")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--seconds", type=float, default=60)
    p.add_argument("--output", required=True)
    a = p.parse_args()
    if not 0 < a.seconds <= 86400:
        p.error("--seconds must be 0..86400")
    digest = hashlib.sha256()
    count = 0
    with (
        private_output(a.output) as out,
        BridgeSerial(a.port, a.baud, timeout=0.1, exclusive=True, xonxoff=False) as s,
    ):
        end = time.monotonic() + a.seconds
        while time.monotonic() < end:
            block = s.read(4096)
            out.write(block)
            digest.update(block)
            count += len(block)
    print(
        json.dumps(
            {
                "bytes": count,
                "sha256": digest.hexdigest(),
                "output": a.output,
                "permissions": "0600",
            }
        )
    )


if __name__ == "__main__":
    main()
