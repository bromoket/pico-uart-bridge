#!/usr/bin/env bash
# Build from pinned source; flashing always requires explicit target identity.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${BUILD_DIR:-$root/build}"
board="${PICO_BOARD:-pico}"
flash=false
serial=""
while (($#)); do
 case "$1" in
  --flash) flash=true; shift ;;
  --serial) serial="${2:?missing serial}"; shift 2 ;;
  *) printf 'Unknown argument: %s\n' "$1" >&2; exit 2 ;;
 esac
done
if "$flash" && [[ -z "$serial" ]]; then
 printf 'Refusing flash without --serial FLASH_ID\n' >&2; exit 2
fi
# Update only pinned submodules; never silently pull a new SDK or patch a UF2.
git -C "$root" submodule update --init --recursive
cmake -S "$root" -B "$build" -G Ninja -DPICO_BOARD="$board" \
 -DCMAKE_BUILD_TYPE=Release -DBRIDGE_RGB_PIN="${BRIDGE_RGB_PIN:--1}"
cmake --build "$build"
sha256sum "$build/uart_bridge.uf2"
if "$flash"; then
 picotool load -v -x "$build/uart_bridge.uf2" -f --ser "$serial"
fi
