"""Source-level checks for the FIFO patch; hardware tests are also required."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FifoPatchContract(unittest.TestCase):
    def test_hardware_fifo_and_timeout_irq_enabled(self):
        source = (ROOT / "uart-bridge.c").read_text()
        self.assertRegex(source, r"\buart_set_fifo_enabled\(p->inst,\s*true\)")
        self.assertIn("UART_UARTIMSC_RTIM_BITS", source)
        self.assertIn("UART_UARTICR_RTIC_BITS", source)

    def test_reset_interface_is_present(self):
        descriptors = (ROOT / "usb-descriptors.c").read_text()
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn("TUD_RPI_RESET_DESCRIPTOR", descriptors)
        self.assertIn("pico_usb_reset", cmake)
        self.assertIn("PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1", cmake)

    def test_usb_product_identifies_fifo_build(self):
        source = (ROOT / "usb-descriptors.c").read_text()
        self.assertIn("Pico FIFO UART Bridge", source)


if __name__ == "__main__":
    unittest.main()
