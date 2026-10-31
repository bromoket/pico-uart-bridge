#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
python3 -m unittest discover -s tests -v
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
cc -std=c11 -Wall -Wextra -Werror -O2 -pthread tests/ring_test.c -o "$work/ring"
"$work/ring"
cc -std=c11 -Wall -Wextra -Werror -O2 tests/led_test.c -o "$work/led"
"$work/led"
clang-format --dry-run --Werror uart-bridge.c bridge-*.h bridge-led.c bridge-diagnostics.c tests/*_test.c
git diff --check
