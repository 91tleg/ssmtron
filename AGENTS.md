# Repository Guidelines

## Project Structure & Module Organization

`firmware/app/` contains the Zephyr entry point and ECU I/O loop. `firmware/lib/ecu/` defines the ECU-facing API; `firmware/lib/ssm1/` implements the SSM1 session, commands, parsers, profiles, and value decoding. `firmware/hal/` holds hardware interfaces and the nRF52 implementation. Board wiring and the devicetree overlay live in `firmware/bsp/nrf52840dk/`. Host tests are in `firmware/test/`. `docs/ref.asm` is the legacy scan-tool reference for protocol bytes, ECU addresses, and conversions; check it when changing SSM1 behavior.

## Build, Test, and Development Commands

From the repository root:

```sh
cmake -S firmware/test -B firmware/build-test
cmake --build firmware/build-test
ctest --test-dir firmware/build-test --output-on-failure

cd /opt/nordic/ncs/v3.2.4 && nrfutil sdk-manager toolchain launch --ncs-version v3.2.4 -- \
  west build -b nrf52840dk/nrf52840 /Users/maxchou/Desktop/ssmtron/firmware/app -d /Users/maxchou/Desktop/ssmtron/firmware/build --pristine

```

do not run tests or build unconditionally. only do when significant changes are made, or testing catches bugs.
Add comment to where it would help, do not use me dahs anywhere.

## Coding Style & Naming Conventions

Use C99, four-space indentation, and braces on separate lines. Follow `firmware/.clang-format`. Use `snake_case` for functions and variables, `UPPER_CASE` for macros, and module prefixes such as `ssm1_`, `ecu_`, and `hal_` for public APIs. Keep protocol parsing and decoding in `firmware/lib/ssm1/`; keep chip-specific code in `firmware/hal/nrf52/`. Host tests compile with `-Wall -Wextra -Werror -Wconversion -Wshadow -pedantic` and address/undefined-behavior sanitizers. all integer macro should be in parenthese. 

## Testing Guidelines

Tests use the local `minitest.h` harness and CTest. Name new test files `test_<module>.c` and add them to `firmware/test/CMakeLists.txt`.

## Commit & Pull Request Guidelines

conventional commit. follow 50/72 rule. use bullets for body. should only subjectively mention what changed, do not use the prompt in commit. do not add yourself as co-author. do not use em dashes. do not reference uncommitted files, instructions, or what was not done; describe only what was done.
