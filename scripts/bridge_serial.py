"""Pyserial adapter for Linux custom baud rates on Python <3.14.

Some CPython termios implementations reject BOTHER in tcsetattr even though the
kernel supports it through TCSETS2. Configure framing using a standard speed,
then use pyserial's Linux TCSETS2 path for the requested custom speed. No retries
or silent fallback baud. Standard rates and non-Linux platforms are unchanged.
"""

import sys
import serial

if sys.platform.startswith("linux"):
    import termios


class BridgeSerial(serial.Serial):
    def _reconfigure_port(self, force_update=False):
        requested = self._baudrate
        native = (
            not sys.platform.startswith("linux")
            or hasattr(termios, f"B{requested}")
            or requested in getattr(self, "BAUDRATE_CONSTANTS", {})
        )
        if sys.platform.startswith("linux") and not native:
            self._baudrate = 38400
            try:
                super()._reconfigure_port(force_update=force_update)
            finally:
                self._baudrate = requested
            self._set_special_baudrate(requested)
        else:
            super()._reconfigure_port(force_update=force_update)
