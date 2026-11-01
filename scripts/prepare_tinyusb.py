#!/usr/bin/env python3
"""Generate a reviewed asymmetric CDC source copy; never patch SDK files/binaries.

TinyUSB in SDK 2.3.1 shares EP_BUFSIZE for IN and OUT. Larger IN transfers enable
RP2040 double buffering, but larger OUT transfers wait for a short packet/ZLP and
can stall an exact-64-byte command. Only OUT allocation/preparation is changed.
Original upstream license/copyright is preserved in the generated C file.
"""

import argparse
import hashlib
from pathlib import Path

EXPECTED_SHA256 = "4560ccdb2869a5d2dfdfbaebd50479518dd802beb56a9da188980c6796bb04f5"


def adapt_source(source):
    allocation = "TUD_EPBUF_DEF(epout, CFG_TUD_CDC_EP_BUFSIZE);"
    start_token = "static bool _prep_out_transaction(uint8_t itf) {"
    end_token = "// APPLICATION API"
    if (
        source.count(allocation) != 1
        or source.count(start_token) != 1
        or source.count(end_token) != 1
    ):
        raise ValueError("Unexpected TinyUSB CDC source shape; review SDK adaptation")
    start = source.index(start_token)
    end = source.index(end_token, start)
    section = source[start:end]
    if section.count("CFG_TUD_CDC_EP_BUFSIZE") != 3:
        raise ValueError("Unexpected OUT preparation; review SDK adaptation")
    section = section.replace("CFG_TUD_CDC_EP_BUFSIZE", "CFG_TUD_CDC_EP_OUT_BUFSIZE")
    result = source[:start] + section + source[end:]
    return result.replace(allocation, "TUD_EPBUF_DEF(epout, CFG_TUD_CDC_EP_OUT_BUFSIZE);")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("source")
    p.add_argument("output")
    a = p.parse_args()
    raw = Path(a.source).read_bytes()
    if hashlib.sha256(raw).hexdigest() != EXPECTED_SHA256:
        raise SystemExit("TinyUSB CDC source hash changed: explicit adaptation review required")
    Path(a.output).write_text(adapt_source(raw.decode()))


if __name__ == "__main__":
    main()
