# Upstream review and disposition

Reviewed upstream `f53de40836e2be1b3f4793d49b136c979b61099f`, all 33 issue/PR entries returned by the upstream tracker, and textual diffs for PRs 6, 8, 15, 24, 25, 26, 29, 32, 33. Tracker state is not changed by this fork. This is a disposition record, **not a claim that every device/platform request is solved**.

| Issue/PR | Fork disposition |
|---|---|
| #3, #5, #20; PR #24/#25/#32: second port / IRQ hangs | Replaced transport buffers with two independent SPSC ring pairs. No ISR mutex and no drop-on-lock-contention path. Deliberate overflow/recovery, independent port, reconnect and simultaneous router/loopback tests passed. |
| #4, #9, #14; PR #15: missing/corrupted bytes / overlapping copies | No overlapping buffer relocation remains. FIFO enabled, timeout acknowledged; exact random/boundary/large-stream comparisons performed. Finite-buffer overflow is counted, not hidden. |
| #2: unusual baud | Arbitrary representable baud accepted, actual divisor-rounded speed reported. 518400/1036800 requests passed 128 KiB comparisons. Fixed a CPython/Linux BOTHER host configuration failure with a standard-baud + TCSETS2 adapter. |
| #23: >1 Mbaud | Isolated tests passed through 7.5 Mbaud requested (7,462,686 actual); not a universal rated maximum. The main repeatable suite includes 6/7.5 Mbaud; separate independent-router test uses 115200 alongside 4 Mbaud loopback. |
| #7/#19/#28; PR #6: flow/modem lines | Configurable hardware CTS, ring-aware software RTS, active-low DTR implemented with compile-time GPIO validation. Unlike old PR wiring, default UART0 remains GP16/17 and no extra target outputs are enabled. Optional external flow-control circuitry is compile-tested, not hardware-validated. DSR, one-wire multiplexing and automatic target boot/reset protocols are not implemented. |
| PR #24/#25: BREAK, RX pull-up, LEDs, DTR | RX pull-up, DTR-independent transport and BREAK implemented. Timed BREAK is foreground/nonblocking, not `sleep_ms()` inside USB processing. SDK 2.3.1 TinyUSB already advertises BREAK in its CDC descriptor, so the old copied descriptor macro is unnecessary. PIO RGB uses GP23 on the tested board; **PR #24's LED assignments on GP16/17 would conflict with this router and were not copied**. |
| #8 / PR #8: stable serial | Preserved upstream's flash-ID serial implementation. Did not reintroduce PR #8's older unpadded `%X` formatting. Host tools select interface and serial, not enumeration order. |
| #10: serial monitor / DTR dependence | Removed DTR requirement for transport; DTR-low loopback passed. Receive-only non-default-baud clients should assert DTR so enumeration cannot silently replace the boot-capture default. |
| #11/#31: Windows reconnect/external power | SDK upgraded with its USB enumeration support; Linux remote reset/re-enumeration tested. Specific Windows/Thunderbolt/external-power sequence remains unverified, not declared fixed. |
| #12/#16: ESP/Beken programmers | Transport reliability addressed; no arbitrary overclock. Actual ESP/Beken programming/boot wiring was not tested. A working UART stream is necessary, not sufficient, for those protocols. |
| #13: fingerprint sensor bitstream | Random/boundary binary stream tested; that sensor and Windows software not available. No device-specific compatibility claim. |
| #17/#22: open-collector / RS232 | Electrical incompatibilities require an appropriate external circuit/transceiver or a separate tested implementation. Direct ±RS232/5 V connections remain forbidden. |
| #18 / PR #33: board support | Configurable pins/LED/RGB with collision rejection. Pico and Pico 2 builds compile; actual hardware tests are this RP2040 clone only. PR #33's automatic board profiles and CYW43 LEDs were reviewed but not merged wholesale or falsely presented as hardware-tested. Use explicit board-specific pins and disable unverified LEDs. |
| #21: buffer size | Explicit 32 KiB per direction/port, 128 KiB total ring storage; RP2040 ELF BSS remains within SRAM. Native full/empty/wrap/concurrency tests cover ring ownership and bounds. |
| PR #26: Nix | Reviewed dev shell/lock file. Not imported: no Nix test environment, and copying an old nixpkgs lock is not evidence of a current reproducible build. CMake/pinned-submodule build documented and exercised. |
| PR #29: Pico 2 / #30: modes | Preserved upstream PICO_BOARD override and CI board matrix; Pico 2 compiled locally. Supported/rejected line formats documented and exercised on RP2040. |
| #1: Arduino IDE / #27: Bluetooth | This fork deliberately uses the C/C++ Pico SDK and USB CDC. No Arduino IDE packaging or Bluetooth bridge is claimed. |

## Sources

- https://github.com/Noltari/pico-uart-bridge/issues
- https://github.com/Noltari/pico-uart-bridge/pulls
- https://github.com/jylho/picouart/tree/b1f4def4571fdd6fd408bd56aee83468d446280f
- https://github.com/raspberrypi/pico-sdk/releases/tag/2.3.1
- https://gist.github.com/probonopd/2a1e8ce64142fdefe20a89d0d540ab0d (clone GPIO23 RGB / GPIO24 user-button reference)

Serial transport, buffering, telemetry and PIO effects are fork changes, not assertions that upstream accepted these implementations. The small picouart FIFO/product changes were initially applied in source and then adapted to the ring transport; its Python test repository was used externally, not copied wholesale into this MIT repository.
