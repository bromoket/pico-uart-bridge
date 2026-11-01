# Protocol, controls and compatibility

## USB identity and serial streams

VID:PID remains the upstream Pico SDK example identity `2e8a:000a`; it is not a newly allocated commercial VID/PID. Product: `Pico FIFO UART Bridge`; flash-ID-derived serial remains unique per board. Select both serial and interface, never the lowest COM/tty number.

- Interfaces 0/1: UART0 CDC control/data; 2/3: UART1.
- Interface 4: SDK reset interface, compatible with `picotool ... -f --ser ID`.
- Interface 5: endpoint-free diagnostics class interface (subclass `0x42`).

Transport is binary-transparent. Firmware inserts no banners, terminal translations, XON/XOFF or diagnostics into UART streams. RX pull-ups keep disconnected pins from floating; they do not make pins 5 V tolerant. Start captures before reboot. Host kernel/TTY buffers and application output still affect end-to-end reliability independently of firmware counters.

## Diagnostics v1 (little endian)

Read via interface-recipient **class** control request `bmRequestType=0xa1`, `bRequest=0x40`, `wValue=0`, `wIndex=5`, length 132. Word zero is protocol version 1, then 16 uint32 words per UART:

```
rx_bytes tx_bytes rx_dropped overruns
framing_errors parity_errors breaks high_water
rx_queued tx_queued actual_baud rejected_config
tx_pin rx_pin connected break_active
```

Counters are monotonic modulo 2^32 since reset. `rx_bytes` counts hardware data-register reads, including bytes later dropped; `tx_bytes` counts bytes placed into the TX FIFO, not proof a remote device consumed them. Overrun events do not reveal the exact number of physically lost characters. `connected` means CDC DTR asserted, not a responding target. Queue counts and snapshots are observational, not an atomic transaction across two cores; compare deltas after traffic settles. No secret target bytes are returned.

Optional USB timing extension: class read request `0xa1`, request `0x42`, index 5, length 32; four uint32 words per UART: `usb_service_gap_us`, `usb_blocked_max_us`, `usb_rx_written`, `usb_write_calls`. Maxima/counters are cumulative since reset, including deliberate stall tests; they are not per-test deltas or proof of peer receipt. No target data is exposed.

Write LED override via class request `0x21`, request `0x41`, index 5, zero-length body, value: 0 auto, 1 red, 2 green, 3 blue, 4 white, 5 off. It changes RGB only; the separate status LED still reflects connection/activity.

## Line coding and BREAK

5–8 data bits; parity none/odd/even/mark/space; one or two stop bits. USB's 1.5-stop-bit mode is deliberately rejected, not silently treated as one. Baud must fit the PL011 divisor range at the current peripheral clock. Invalid configurations increment `rejected_config` and do not change hardware; new USB OUT bytes backpressure until a valid configuration is restored. TinyUSB's GET_LINE_CODING reflects the host request, so consult diagnostics for actual baud. A baud change is queued once per configuration event, not polled under a mutex on every data-loop iteration. The foreground core waits for queued TX to drain before applying it; new-format USB OUT bytes are held until application is acknowledged. Change settings only at a protocol boundary, never amid inbound traffic. USB unmount clears stale connection indication and releases held BREAK/optional modem outputs; reconfiguration is queued again on mount.

Linux may set 9600 during enumeration without opening the port. The firmware preserves initial 115200 until DTR or OUT data indicates a client; DTR-less clients activate settings by sending data. To receive only at a different baud, assert DTR. The host helper works around CPython termios implementations that reject BOTHER by configuring a standard interim baud then applying TCSETS2. This was exercised under Python 3.13 on Linux.

CDC SEND_BREAK: 0 clears, 65535 holds until cleared, other uint16 durations are timed in milliseconds by foreground code without sleeping in a USB callback. BREAK intentionally produces a BREAK/framing event in a physical self-loop. In the hardware test, Linux's kernel driver is detached only from the loopback interface before a finite-duration control request, then restored in `finally`.

## Optional modem/flow pins

All extra target outputs are off by default. CMake validates UART alternate functions and rejects every enabled pin collision, including LED/RGB collisions.

- `BRIDGE_UARTn_CTS`: active-low hardware CTS, fixed-on when configured. CDC line coding does not encode enabling/disabling RTS/CTS.
- `BRIDGE_UARTn_RTS`: active-low **software** readiness, accounts for the 32 KiB receive ring. Deasserted at 28 KiB, resumed below 16 KiB; host RTS must also be asserted. Peer response latency must fit the remaining margin.
- `BRIDGE_UARTn_DTR`: active-low GPIO following host DTR; may reset a connected target. Never configure it on an unknown line.

Example optional UART0 pins: CTS GP18, RTS GP19, DTR GP20, with GP16/17 data. This example is compile-tested, not externally wired/hardware-validated. The router configuration uses none of these pins. Firmware cannot apply flow control if the peer does not expose/honor it.

## Boundaries

This is not native RS232, RS422/485, 5 V UART, 1.8 V UART, open-drain, inverted signaling, LIN, CAN, Bluetooth, or automatic ESP boot/reset circuitry. Use appropriate transceivers/level shifters and protocol-specific wiring. Specific programmer compatibility needs that target and its actual boot protocol. A self-loop uses one shared UART clock and does not independently prove baud accuracy; the separate timing tests and independent router response provide additional evidence. No SWD probe is attached, so claims here are USB/hardware functional tests, not SWD debugging.
