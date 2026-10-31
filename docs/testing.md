# Verification record and reproducible tests

## Environment and source

Tested on Linux, RP2040 B2, 16 MiB USB-C Pico-compatible clone; GP16/17 connected to an owned stock router, GP4/5 externally jumpered. ARM GCC 14.2.1, CMake 4.2.3, Pico SDK **2.3.1** (`079c6f39023649b154152db30f1d781e884879bc`), SDK-pinned TinyUSB. RGB enabled with `-DBRIDGE_RGB_PIN=23`; GP25 status LED. No UART overclock, no SWD debugger or independent logic analyzer attached. RGB color-cycle and operation visually confirmed by the operator during this session.

The production default is 115200 8N1 on both ports. Test scripts change **only UART1** baud/framing/BREAK. Never attach a live target to the loopback test port. No target firmware writes were performed.

## Results

- Host unit/source/CLI checks: **13 passed**. These supplement, not replace, hardware tests.
- Native ring tests: empty/full, every usable slot, uint32 wrap, 2,000,000 ordered concurrent bytes. Also passed AddressSanitizer + UndefinedBehaviorSanitizer.
- Pico and Pico 2 compile: passed. Pico 2 hardware is not available/tested.
- Remote GitHub Actions [run 41199371396](https://github.com/bromoket/pico-uart-bridge/actions/runs/41199371396), source commit `c68abb3`: **all four jobs passed** (host tests, Pico build, Pico 2 build, clone RGB build). CI artifacts were built from source; the attached board was flashed from the local build, not a downloaded artifact.
- Final main hardware suite: **70/70 passed**, **2,806,392 exact-compared bytes**, including repeated 1 MiB transfers, small FIFO/USB/ring-boundary bursts, 9600 through requested 7.5 Mbaud, custom 518400/1036800, 5N1/6E1/7O2/8E1/8O1/8M1/8S1/8N2, DTR-low, 16 KiB unread-host delay, overflow/recovery, timed BREAK and 25 reopen cycles. Ordinary exact-comparison cases require unchanged drop/overrun/framing/parity counters.
- Extended isolated tests: 128 KiB matched at custom 518400/1036800 and 6/7.5 Mbaud; 32 bytes matched at 300 and 1200. Actual high rates were 6,024,096 and 7,462,686. Self-loop shares the UART clock; it is not independent frequency metrology.
- Separate independent-target test: full 524,288-byte logical boot partition matched its earlier independent backup. All 256 byte values and 1,000 deterministic response lines were verified through the stock shell.
- Simultaneous target + loopback: **52,690,944 exact loopback bytes** in 804 blocks while reading the boot partition and capturing **three target reboots**. Both interfaces had **zero new dropped-byte, overrun, framing, parity or BREAK events**. Boot captures varied in length (6485, 6485, 6447 bytes), as expected for nondeterministic logs; equality of log lengths is not used as a loss test.
- USB controls: **7/7 passed**: five invalid line configurations rejected without changing hardware baud, continuous BREAK set/clear, **five remote BOOTSEL/application round trips** preserving default boot-capture baud. The Pico can be reflashed without the operator pressing BOOTSEL while functioning firmware/USB remain available; physical BOOTSEL is still necessary for a broken USB firmware image.

### Expected error tests, not concealed failures

The final main suite deliberately overfilled finite RAM while the host did not read: **93,112 bytes were reported dropped in that test**, followed by byte-identical recovery transfers. Timed BREAK intentionally produces a BREAK/framing event in UART1's self-loop. Cumulative diagnostics therefore need not be zero after the whole suite; use per-test deltas. These tests prove bounded behavior and observable overload, **not losslessness during overload**.

Earlier 16 KiB write-before-read testing stalled the old shared-buffer design. It was retained as a regression scenario; the ring build passed the finite-stall case. Another earlier test incorrectly tried to claim the Linux-owned CDC interface for BREAK; the harness now temporarily detaches **only** loopback CDC1 and restores it in `finally`. A custom-baud open failure under Python 3.13 was fixed in `bridge_serial.py` using TCSETS2, not by changing the requested baud. The first reboot runner assumed a 25-second boot and failed; it now waits for an exact shell marker with a 100-second deadline and saves private evidence. Failed-run records were preserved locally rather than reclassified as successes.

## Run it

Install `pyserial==3.5`, `pyusb==1.3.1` in a virtual environment. Native tests need a C compiler, pthreads and clang-format; Python checks use unittest. For the separate upstream picouart timing suite, pytest is also required.

```sh
bash scripts/test-host.sh
python scripts/test_hardware.py \
  --loopback-port /dev/serial/by-id/usb-picouart_Pico_FIFO_UART_Bridge_YOUR_ID-if02 \
  --output /path/to/loopback-results.json
# Disrupts the adapter (not the router's firmware); no terminals may be open.
python scripts/test_usb_control.py --serial YOUR_ID --output /path/to/usb-results.json
python scripts/bridge_diag.py --serial YOUR_ID --led 0
python scripts/serial_capture.py \
  --port /dev/serial/by-id/usb-picouart_Pico_FIFO_UART_Bridge_YOUR_ID-if00 \
  --seconds 60 --output /private/path/new-capture.bin
```

`serial_capture.py` uses exclusive access, binary output, mode 0600 and refuses overwrite. It prints a length/hash, not the potentially secret console body. Stable USB identities are required for tests; no port auto-selection may accidentally send random bytes to the router. The hardware runner's receiver works concurrently with its bounded writer; do not call an unbounded `flush()` before starting a reader and mistake a host-side deadlock for a wire limit.

Source-level contract checks are not behavioral proof. Optical confirmation is attributed to the operator. Optional CTS/RTS/DTR physical wiring, other devices' boot protocols, Windows/macOS and externally powered USB-attach sequences remain outside the tested hardware matrix. The bridge does not turn a raw serial `cat` into a safe flash-writing protocol; future programming must verify transfer checksums and the target's actual update/recovery procedure.
