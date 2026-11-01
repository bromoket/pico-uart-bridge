import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class TinyUsbAdaptation(unittest.TestCase):
    def load(self):
        s = importlib.util.spec_from_file_location(
            "prepare_tinyusb", ROOT / "scripts/prepare_tinyusb.py"
        )
        m = importlib.util.module_from_spec(s)
        s.loader.exec_module(m)
        return m.adapt_source

    def test_only_out_path_is_changed(self):
        source = """TUD_EPBUF_DEF(epout, CFG_TUD_CDC_EP_BUFSIZE);
TUD_EPBUF_DEF(epin, CFG_TUD_CDC_EP_BUFSIZE);
static bool _prep_out_transaction(uint8_t itf) {
 TU_VERIFY(available >= CFG_TUD_CDC_EP_BUFSIZE);
 if (available >= CFG_TUD_CDC_EP_BUFSIZE) {
  return usbd_edpt_xfer(rhport, p_cdc->ep_out, p_epbuf->epout, CFG_TUD_CDC_EP_BUFSIZE);
 }
}
// APPLICATION API
const uint16_t count = tu_fifo_read_n(&p_cdc->tx_ff, p_epbuf->epin, CFG_TUD_CDC_EP_BUFSIZE);
"""
        out = self.load()(source)
        self.assertIn("TUD_EPBUF_DEF(epout, CFG_TUD_CDC_EP_OUT_BUFSIZE)", out)
        self.assertIn("p_epbuf->epout, CFG_TUD_CDC_EP_OUT_BUFSIZE)", out)
        self.assertIn("p_epbuf->epin, CFG_TUD_CDC_EP_BUFSIZE)", out)
        self.assertEqual(out.count("CFG_TUD_CDC_EP_OUT_BUFSIZE"), 4)

    def test_unknown_source_shape_fails_closed(self):
        with self.assertRaises(ValueError):
            self.load()("unexpected upstream source")


if __name__ == "__main__":
    unittest.main()
