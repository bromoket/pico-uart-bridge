"""Compile-time pin safety: rejects conflicts before any UF2 can be built."""

import subprocess, tempfile, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class BoardTests(unittest.TestCase):
    def compile(self, *defines):
        with tempfile.TemporaryDirectory() as d:
            return subprocess.run(
                [
                    "cc",
                    "-x",
                    "c",
                    "-fsyntax-only",
                    "-I",
                    str(ROOT),
                    *[f"-D{x}" for x in defines],
                    "-",
                ],
                input='#include "bridge-board.h"\n',
                text=True,
                capture_output=True,
            ).returncode

    def test_default(self):
        self.assertEqual(self.compile(), 0)

    def test_rgb_uart_conflict(self):
        self.assertNotEqual(self.compile("BRIDGE_RGB_PIN=16"), 0)

    def test_rgb_led_conflict(self):
        self.assertNotEqual(self.compile("BRIDGE_RGB_PIN=25"), 0)

    def test_cts_wrong_uart(self):
        self.assertNotEqual(self.compile("BRIDGE_UART0_CTS=6"), 0)

    def test_flow_collision(self):
        self.assertNotEqual(self.compile("BRIDGE_UART0_RTS=23", "BRIDGE_RGB_PIN=23"), 0)

    def test_flow_valid(self):
        self.assertEqual(
            self.compile(
                "BRIDGE_UART0_CTS=18", "BRIDGE_UART0_RTS=19", "BRIDGE_UART0_DTR=20"
            ),
            0,
        )


if __name__ == "__main__":
    unittest.main()
