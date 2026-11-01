# SDK-aware clangd setup and diagnostic troubleshooting

The firmware is embedded **C**, not host C++. Keep the CMake target language,
SDK definitions, generated headers and ARM toolchain. Do not add C++23 to `.c`
files, stub out Pico APIs or suppress missing-header errors to make an editor green.

## Build database first

CMake exports `compile_commands.json`. Configure/build normally so the database
and generated `bridge-rgb.pio.h` exist. If the build directory is external, point
your editor there or create a local ignored symlink from the repository root:

```sh
ln -s /absolute/path/to/build/compile_commands.json compile_commands.json
BUILD_DIR=/absolute/path/to/build bash scripts/check-editor.sh
```

Choose a new symlink path or inspect an existing one first; do not overwrite
someone else's database. Compilation databases contain machine-specific paths
and are deliberately not committed.

The checker validates all four firmware translation units and uses a narrowly
trusted `arm-none-eabi-gcc` query driver. An editor/LSP must use the equivalent
trusted driver allowlist, for example on Ubuntu:

```text
--query-driver=/usr/bin/arm-none-eabi-gcc,/usr/bin/arm-none-eabi-g++
```

Query-driver is a **clangd startup option**, not a compiler flag or `.clangd`
compile argument. It permits querying only the selected known compiler; it must
not be an unrestricted wildcard allowing arbitrary repository executables.

## Recognize the two configuration failures

1. Missing `bridge-rgb.pio.h`, `gpio_init`, `pio0`, `pio_sm_config`, etc.: check
   build/generated-header availability and the actual compile database first.
2. Database/SDK paths present but `assert.h`, `inttypes.h`, `memcpy`/`memcmp` or
   `snprintf` still missing: the LSP may not be discovering ARM newlib/system
   includes. Fix compiler discovery, not the valid source declarations.

Inspect the selected compiler's real include search:

```sh
arm-none-eabi-gcc -E -Wp,-v -xc /dev/null
clangd --check=bridge-led.c --query-driver=/usr/bin/arm-none-eabi-gcc --tweaks=
```

On Debian/Ubuntu, a client already running without query-driver can also use a
**machine-local**, firmware-TU-scoped include fallback after verifying that this
is the installed ARM toolchain's include directory:

```yaml
# In ~/.config/clangd/config.yaml on Linux only, after any existing fragments.
---
If:
  PathMatch: .*/pico-uart-bridge/(uart-bridge|bridge-led|bridge-diagnostics|usb-descriptors)\.c
CompileFlags:
  Add: [-isystem, /usr/lib/arm-none-eabi/include]
```

Do not apply ARM headers to native host tests or unrelated repositories. Other
platforms/toolchain layouts need their own actual paths. A user-wide config has
higher precedence than project `.clangd`, so avoid global target/standard or
warning overrides.

## Verification and cache freshness

A 2026-11-01 session initially showed **21 cached errors** for the LED source.
A fresh active probe reduced this to seven missing ARM system-header diagnostics
across four files; SDK-aware CLI checks already passed. With correct newlib
visibility and refreshed document parses, **all four files passed the actual
active LSP with zero diagnostics**, as well as the CLI checker. Firmware source
bytes were unchanged; no diagnostic suppression or reflashing was needed.

An unchanged document may still display diagnostics from an earlier parse after
editing user-level config. Restart the editor/LSP cleanly or request a real
reparse, then verify an active diagnostic result. Do not call an empty or stale
cache a clean build. The `--tweaks=` workaround in the checker disables clangd's
internal refactoring self-tests only; it does not disable parsing/diagnostics.

Repository `.clang-format` remains authoritative. Home-directory Allman/C++23
preferences must not silently reformat this existing C project or replace its
established Linux-style braces.

References: [clangd configuration](https://clangd.llvm.org/config),
[system-header/query-driver guide](https://clangd.llvm.org/guides/system-headers).
