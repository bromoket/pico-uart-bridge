# Pico FIFO UART Bridge

Source-built, dual USB–UART bridge forked from [Noltari](https://github.com/Noltari/pico-uart-bridge), with the FIFO/product-identification improvements from [picouart](https://github.com/jylho/picouart) integrated into C source—not patched binaries.

## What changed

- **Pico SDK 2.3.1**, pinned with its matching TinyUSB dependency.
- **32 KiB SPSC rings in each direction on each UART**: no interrupt mutex, no buffer-shifting `memmove`, no drops merely because another core holds a lock.
- Hardware FIFOs, receive-timeout handling, bounded IRQ work, RX pull-ups and clean idle/startup pin configuration.
- Independent ports; no DTR requirement for byte transport; USB OUT backpressure. Enumeration alone no longer changes the default 115200 boot-capture baud to 9600.
- Separate USB diagnostics for byte/error/overflow counters, queue depth, actual baud and GPIO mapping. No diagnostic text enters either UART stream.
- Validated line coding, 5–8 data bits, none/odd/even/mark/space parity, 1/2 stop bits; nonblocking timed/continuous BREAK.
- Optional compile-time CTS, software ring-aware RTS and active-low DTR pins. All disabled by default; conflicting/invalid GPIO assignments fail compilation.
- Optional **PIO-driven WS2812 RGB** and status LED: RX green, TX magenta, simultaneous traffic mixed, errors red, blue idle breathing. No bit-banging or interrupt masking.
- SDK USB reset interface: **remote BOOTSEL/reflash**, without touching target UART pins.
- Host tools for safe builds/flashing, private binary capture, telemetry, bounded concurrent loopback tests and Linux custom-baud compatibility.

## Wiring: GPIO numbers, not header positions

| USB interface | TX → target RX | RX ← target TX |
|---|---|---|
| `if00` / UART0 | **GP16** (standard Pico physical 21) | **GP17** (physical 22) |
| `if02` / UART1 | **GP4** (physical 6) | **GP5** (physical 7) |

Common GND is required. **3.3 V logic only**, not 5 V tolerant and not RS232/RS485 electrical signaling. Do not connect router VCC. The tested USB-C, 16 MiB Pico clone has a GP25 user LED and **GP23 RGB** (operator-confirmed working). RGB is opt-in because other boards wire it differently. The power LED is not software-controlled.

## Build and use

Install Git, CMake, Ninja, ARM GCC/newlib, Python, and picotool. Clone recursively, then:

```sh
# Default: build only; RGB disabled for unknown boards.
bash scripts/build-flash.sh
# This tested clone: compile RGB on GP23, then flash only the selected board.
BRIDGE_RGB_PIN=23 bash scripts/build-flash.sh --flash --serial YOUR_FLASH_ID
# Never guess ttyACM numbering: select by USB serial and interface.
python scripts/bridge_diag.py --serial YOUR_FLASH_ID
```

Python tools require `pyserial` and `pyusb`; see [testing](docs/testing.md). For custom builds, CMake accepts `BRIDGE_UART{0,1}_{TX,RX,CTS,RTS,DTR}`, `BRIDGE_LED_PIN` and `BRIDGE_RGB_PIN`; optional pins use `-1` to disable.

## Verified—and limits

**70 hardware cases passed**, including 1 MiB transfers, 5–8-bit formats, DTR-low, timed BREAK, 25 reopens, finite host stalls, and intentional overflow/recovery. Extended self-loop tests passed through requested **7.5 Mbaud**; actual divisor-rounded baud is reported, not assumed. During a separate simultaneous router test, **52,690,944 loopback bytes matched**, a **512 KiB router boot partition matched its independent backup**, and three reboot captures completed with **zero new drop/overrun/framing/parity/BREAK events on either UART**. RGB operation was visually confirmed by the operator.

These are bounded measurements, **not a universal losslessness or device-programming guarantee**. Infinite unthrottled input cannot fit finite RAM while a host is stalled. The overflow test deliberately drops bytes and checks that counters increase and the bridge recovers. Optional external flow/control wiring, other boards, Windows/macOS, external-power attach, and specific ESP/Beken programmers are not hardware-validated here. UART alone is not a NAND programmer.

[Reproducible tests and evidence](docs/testing.md) · [Upstream issues/PR disposition](docs/upstream-review.md) · [Protocol and compatibility](docs/protocol.md)
