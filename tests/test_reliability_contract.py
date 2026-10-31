from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReliabilityContract(unittest.TestCase):
    def test_isr_is_nonblocking_and_counted(self):
        s = (ROOT / "uart-bridge.c").read_text()
        self.assertIn("ring_push", s)
        self.assertIn("rx_dropped", s)
        self.assertNotIn("mutex_enter_blocking(&ud->uart_mtx)", s)
        self.assertNotIn("memmove(", s)

    def test_diagnostics_and_break_are_separate_from_uart(self):
        s = (
            (ROOT / "bridge-diagnostics.c").read_text()
            if (ROOT / "bridge-diagnostics.c").exists()
            else ""
        )
        self.assertIn("0x40", s)
        self.assertIn("tud_control_xfer", s)
        self.assertIn("tud_cdc_send_break_cb", (ROOT / "uart-bridge.c").read_text())


if __name__ == "__main__":
    unittest.main()
