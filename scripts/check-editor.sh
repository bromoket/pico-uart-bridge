#!/usr/bin/env bash
# Cross-target editor check using the actual CMake SDK/generated-header paths.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${BUILD_DIR:-$root/build}"
driver="$(command -v arm-none-eabi-gcc)"
if [[ ! -f "$build/compile_commands.json" || ! -f "$build/bridge-rgb.pio.h" ]]; then
 printf 'Build first so compile_commands.json and PIO headers exist: %s\n' "$build" >&2
 exit 2
fi
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
for file in uart-bridge.c bridge-led.c bridge-diagnostics.c usb-descriptors.c; do
 # Check parsing/diagnostics, not clangd's internal refactoring self-tests.
 # SwapBinaryOperands in clangd 21 reports overlapping edits inside the
 # TinyUSB descriptor macro despite a valid AST and a clean ARM GCC build.
 if ! clangd --check="$root/$file" --compile-commands-dir="$build" \
   --query-driver="$driver" --tweaks= > "$work/$file.log" 2>&1; then
  grep -E '^E\[|All checks completed' "$work/$file.log" >&2
  exit 1
 fi
 grep 'All checks completed' "$work/$file.log"
done
