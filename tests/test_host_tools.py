import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from serial_capture import private_output  # noqa: E402 -- test local scripts without installation


class HostTools(unittest.TestCase):
    def test_capture_is_private_and_never_overwritten(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "capture"
            with private_output(path) as f:
                f.write(b"private")
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            with self.assertRaises(FileExistsError):
                private_output(path)
            self.assertEqual(path.read_bytes(), b"private")

    def test_flash_requires_identity_before_build(self):
        r = subprocess.run(
            ["bash", str(ROOT / "scripts/build-flash.sh"), "--flash"],
            capture_output=True,
            text=True,
        )
        self.assertEqual(r.returncode, 2)
        self.assertIn("Refusing flash without --serial", r.stderr)


if __name__ == "__main__":
    unittest.main()
