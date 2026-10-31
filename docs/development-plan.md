# Reliable dual UART bridge: execution plan

Approved scope: source-built general-purpose 3.3 V bridge, router UART0 on GP16/17, UART1 loopback on GP4/5; no router flash writes. Execute inline.

1. Preserve SDK/FIFO/reset baseline, compile and verify remote BOOTSEL (completed, 18612fa).
2. Test bounded single-producer/single-consumer rings for full, empty, partial consume, wrap and concurrent ordering. Replace shared memmove buffers and interrupt mutexes; service closed ports without deadlocking. Count discarded bytes and hardware errors; retain bounded data until host returns.
3. Test USB diagnostics and line coding; add nonblocking timed BREAK, optional hardware flow and control pins with compile-time collision checks. Default no extra target control signals. Invalid serial configurations must be rejected and counted rather than corrupting live settings.
4. Add configurable GP25 LED and opt-in GP23 WS2812 PIO activity driver; never bit-bang with interrupts disabled. Software tests check activity/error priority. Physical RGB light output remains unverified without an observing sensor/person.
5. Rebuild, remotely flash and verify, run isolated and simultaneous router/loopback tests: random, all byte values, small bursts, long streaming, independent ports, line coding, stalled readers/overflow/recovery, BREAK and reconnect. Keep router logs private. Compare exact deterministic router payloads, not variable boot log sizes, for loss evidence.
6. Document upstream issue/PR disposition, results and limits. Run final tests and diff checks, commit and push fork branch. Assess router pre-flash gate separately: UART is not NAND access, no full-NAND/OOB backup or recovery write claim without evidence.

No finite-buffer bridge can preserve indefinite unthrottled input while USB is absent. No claim of all hardware/platform issues resolved without relevant hardware. Do not add RS232/5V/open-drain claims without external level translation.
