#!/usr/bin/env bash
# Build the firmware. Reconfigures if needed, otherwise incremental.
#
# Usage:
#   ./build.sh                    # incremental build
#   ./build.sh clean              # nuke build/, full reconfigure
#   ./build.sh -DOPT=value ...    # pass cmake -D flags (reconfigures)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$REPO_ROOT/build"

PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/.pico-sdk/pico-sdk}"
PICO_TOOLCHAIN_PATH="${PICO_TOOLCHAIN_PATH:-$HOME/.pico-sdk/toolchain/arm-14.2}"

[[ -d "$PICO_SDK_PATH" ]] \
  || { echo "PICO_SDK_PATH=$PICO_SDK_PATH not found"; exit 1; }
[[ -d "$PICO_TOOLCHAIN_PATH/bin" ]] \
  || { echo "PICO_TOOLCHAIN_PATH=$PICO_TOOLCHAIN_PATH/bin not found"; exit 1; }

export PICO_SDK_PATH PICO_TOOLCHAIN_PATH
export PATH="$PICO_TOOLCHAIN_PATH/bin:$PATH"

if [[ "${1:-}" == "clean" ]]; then
  rm -rf "$BUILD_DIR"
  shift
fi

EXTRA_FLAGS=()
for arg in "$@"; do
  if [[ "$arg" == -D* ]]; then
    EXTRA_FLAGS+=("$arg")
  fi
done

if [[ ! -d "$BUILD_DIR" || ${#EXTRA_FLAGS[@]} -gt 0 ]]; then
  cmake -G Ninja -S "$REPO_ROOT" -B "$BUILD_DIR" "${EXTRA_FLAGS[@]}"
fi

cmake --build "$BUILD_DIR" -- -j"$(nproc)"

UF2="$BUILD_DIR/ds5-bridge.uf2"
[[ -f "$UF2" ]] && {
  echo
  echo "Built: $UF2 ($(du -h "$UF2" | cut -f1))"
  echo "Flash: hold BOOTSEL on the Pico, plug in USB, drag this file onto the RPI-RP2 drive."
}
