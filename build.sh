#!/usr/bin/env bash
# Build the firmware.
#
# First-time use (interactive):
#   ./build.sh                    Prompts for Wi-Fi networks (one or more),
#                                 saves them to senselink.local.conf
#                                 (gitignored, mode 0600), then builds.
#                                 Subsequent runs reuse the conf.
#
# Manage saved networks:
#   ./build.sh setup              Re-prompt from scratch (overwrites conf)
#   ./build.sh add                Add an additional network to the conf
#
# Other:
#   ./build.sh clean              Force clean rebuild
#   ./build.sh -DKEY=VAL ...      Pass extra cmake flags (forces reconfigure)
#
# Non-interactive (CI / scripted), no conf file written. Use parallel arrays:
#   SENSELINK_SSIDS=("Net1" "Net2") \
#   SENSELINK_PSKS=("pass1" "pass2") \
#   ./build.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$REPO_ROOT/build"
GEN_DIR="$REPO_ROOT/generated"
CONF_FILE="$REPO_ROOT/senselink.local.conf"
CREDS_HEADER="$GEN_DIR/wifi_creds_generated.h"

PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/.pico-sdk/pico-sdk}"
PICO_TOOLCHAIN_PATH="${PICO_TOOLCHAIN_PATH:-$HOME/.pico-sdk/toolchain/arm-14.2}"

# ── Colors / logging ──────────────────────────────────────────────────────
RED=$'\033[0;31m'; GREEN=$'\033[0;32m'; YELLOW=$'\033[1;33m'
CYAN=$'\033[0;36m'; BOLD=$'\033[1m'; NC=$'\033[0m'
info() { printf '%s[*]%s %s\n' "$CYAN" "$NC" "$*"; }
ok()   { printf '%s[ok]%s %s\n' "$GREEN" "$NC" "$*"; }
warn() { printf '%s[!]%s %s\n' "$YELLOW" "$NC" "$*"; }
die()  { printf '%s[err]%s %s\n' "$RED" "$NC" "$*" >&2; exit 1; }

# ── Preflight ──────────────────────────────────────────────────────────────
[[ -d "$PICO_SDK_PATH" ]] \
  || die "PICO_SDK_PATH=$PICO_SDK_PATH not found"
[[ -d "$PICO_TOOLCHAIN_PATH/bin" ]] \
  || die "PICO_TOOLCHAIN_PATH=$PICO_TOOLCHAIN_PATH/bin not found"

export PICO_SDK_PATH PICO_TOOLCHAIN_PATH
export PATH="$PICO_TOOLCHAIN_PATH/bin:$PATH"

# ── Argument parsing ───────────────────────────────────────────────────────
DO_CLEAN=0
SUBCMD=""
EXTRA_CMAKE_FLAGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    setup|add)
      [[ -n "$SUBCMD" ]] && die "specify only one subcommand"
      SUBCMD="$1" ;;
    clean)   DO_CLEAN=1 ;;
    -D*)     EXTRA_CMAKE_FLAGS+=("$1") ;;
    -h|--help)
      sed -n '2,24p' "$0" | sed 's/^# \?//'
      exit 0 ;;
    *) die "unknown argument: $1 (try --help)" ;;
  esac
  shift
done

# ── In-memory networks ────────────────────────────────────────────────────
SENSELINK_SSIDS=()
SENSELINK_PSKS=()

# Load from conf if it exists. The conf file is a sourceable shell snippet
# that sets SENSELINK_SSIDS=(...) and SENSELINK_PSKS=(...).
load_conf() {
  [[ -f "$CONF_FILE" ]] || return 1
  # shellcheck source=/dev/null
  source "$CONF_FILE"
  if (( ${#SENSELINK_SSIDS[@]} == 0 )) || \
     (( ${#SENSELINK_SSIDS[@]} != ${#SENSELINK_PSKS[@]} )); then
    warn "$CONF_FILE looks malformed; ignoring"
    SENSELINK_SSIDS=(); SENSELINK_PSKS=()
    return 1
  fi
  return 0
}

# Write the current arrays to the conf, atomically, mode 0600.
save_conf() {
  umask 077
  {
    echo "# senselink local config — do not commit."
    echo "# Re-run \`./build.sh setup\` to overwrite, or \`./build.sh add\` to append."
    echo
    printf 'SENSELINK_SSIDS=('
    local i
    for ((i = 0; i < ${#SENSELINK_SSIDS[@]}; i++)); do
      printf ' %q' "${SENSELINK_SSIDS[$i]}"
    done
    printf ' )\n'
    printf 'SENSELINK_PSKS=('
    for ((i = 0; i < ${#SENSELINK_PSKS[@]}; i++)); do
      printf ' %q' "${SENSELINK_PSKS[$i]}"
    done
    printf ' )\n'
  } > "$CONF_FILE.tmp"
  mv "$CONF_FILE.tmp" "$CONF_FILE"
  chmod 0600 "$CONF_FILE"
}

# Interactively prompt for one network, appending to the arrays.
prompt_one_network() {
  local ssid psk psk2
  while :; do
    read -rp "  SSID: " ssid
    [[ -n "$ssid" ]] && break
    warn "  SSID can't be empty"
  done
  while :; do
    read -rsp "  PSK (will not be shown): " psk; echo
    read -rsp "  PSK (again):              " psk2; echo
    if [[ -z "$psk" ]]; then
      warn "  PSK can't be empty"; continue
    fi
    if [[ "$psk" != "$psk2" ]]; then
      warn "  PSKs don't match, try again"; continue
    fi
    break
  done
  SENSELINK_SSIDS+=("$ssid")
  SENSELINK_PSKS+=("$psk")
}

# Full interactive setup: prompt for one or more networks.
do_setup() {
  echo
  echo "${BOLD}── Wi-Fi networks for the Pico ──${NC}"
  echo "Stored to $CONF_FILE (gitignored, mode 0600)."
  echo "On boot, the Pico will scan and connect to the strongest one in range."
  echo
  SENSELINK_SSIDS=(); SENSELINK_PSKS=()
  local n=1
  while :; do
    echo "Network ${n}:"
    prompt_one_network
    echo
    read -rp "Add another network? [y/N]: " more
    [[ "$more" =~ ^[Yy] ]] || break
    n=$((n+1))
    echo
  done
  save_conf
  ok "saved ${#SENSELINK_SSIDS[@]} network(s) to $CONF_FILE"
}

# Append a single network to the existing conf.
do_add() {
  echo
  echo "${BOLD}── Add a Wi-Fi network ──${NC}"
  if ! load_conf; then
    warn "no existing conf; this becomes network 1"
    SENSELINK_SSIDS=(); SENSELINK_PSKS=()
  fi
  prompt_one_network
  save_conf
  ok "saved ${#SENSELINK_SSIDS[@]} network(s) to $CONF_FILE"
}

case "$SUBCMD" in
  setup) do_setup; echo; info "Setup complete. Run ./build.sh to build."; exit 0 ;;
  add)   do_add;   echo; info "Network added. Run ./build.sh to rebuild."; exit 0 ;;
esac

# ── Resolve credentials for the build ─────────────────────────────────────
# Priority:
#   1. Explicit env vars SENSELINK_SSIDS / SENSELINK_PSKS (arrays)
#   2. Conf file
#   3. Interactive prompt (if stdin is a tty)

if (( ${#SENSELINK_SSIDS[@]} > 0 )) && \
   (( ${#SENSELINK_SSIDS[@]} == ${#SENSELINK_PSKS[@]} )); then
  info "using ${#SENSELINK_SSIDS[@]} network(s) from environment"
elif load_conf; then
  info "using ${#SENSELINK_SSIDS[@]} network(s) from $CONF_FILE"
else
  if [[ ! -t 0 ]]; then
    die "no credentials available. Either:
       1) Run \`./build.sh\` interactively to save them once, or
       2) Pass parallel SENSELINK_SSIDS / SENSELINK_PSKS arrays."
  fi
  do_setup
fi

(( ${#SENSELINK_SSIDS[@]} > 0 )) || die "no networks configured"

# ── Generate the C header consumed by the firmware ────────────────────────
# Backslashes and double-quotes need to be escaped for C string literals.
c_escape() {
  local s="$1"
  s="${s//\\/\\\\}"
  s="${s//\"/\\\"}"
  printf '%s' "$s"
}

generate_header() {
  mkdir -p "$GEN_DIR"
  {
    echo "/* AUTO-GENERATED by build.sh — do not edit, do not commit. */"
    echo "#pragma once"
    echo "#define SENSELINK_NETWORK_COUNT ${#SENSELINK_SSIDS[@]}"
    echo "static const struct senselink_network {"
    echo "    const char *ssid;"
    echo "    const char *psk;"
    echo "} senselink_networks[SENSELINK_NETWORK_COUNT] = {"
    local i
    for ((i = 0; i < ${#SENSELINK_SSIDS[@]}; i++)); do
      local s p
      s=$(c_escape "${SENSELINK_SSIDS[$i]}")
      p=$(c_escape "${SENSELINK_PSKS[$i]}")
      echo "    { \"$s\", \"$p\" },"
    done
    echo "};"
  } > "$CREDS_HEADER.tmp"
  mv "$CREDS_HEADER.tmp" "$CREDS_HEADER"
  chmod 0600 "$CREDS_HEADER"
}

generate_header
info "generated $CREDS_HEADER (${#SENSELINK_SSIDS[@]} network(s))"

# ── Build ──────────────────────────────────────────────────────────────────
if (( DO_CLEAN )); then
  info "cleaning build/"
  rm -rf "$BUILD_DIR"
fi

NEED_CONFIGURE=0
[[ ! -d "$BUILD_DIR" ]] && NEED_CONFIGURE=1
(( ${#EXTRA_CMAKE_FLAGS[@]} > 0 )) && NEED_CONFIGURE=1

if (( NEED_CONFIGURE )); then
  info "configuring (cmake)"
  cmake -G Ninja -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DSENSELINK_SKEL=ON \
    "${EXTRA_CMAKE_FLAGS[@]}" \
    >/dev/null
fi

info "building"
cmake --build "$BUILD_DIR" -- -j"$(nproc)" 2>&1 \
  | grep -E '^\[|error:|warning:|FAILED' || true

UF2="$BUILD_DIR/senselink-skel.uf2"
BL="$BUILD_DIR/ds5-bridge.uf2"
echo
[[ -f "$UF2" ]] && ok "skeleton build: $UF2 ($(du -h "$UF2" | cut -f1))"
[[ -f "$BL"  ]] && ok "baseline build: $BL ($(du -h "$BL"  | cut -f1))"

cat <<EOF

${BOLD}── Flash the Pico 2W ──${NC}
1. Hold BOOTSEL, plug into USB. It mounts as RPI-RP2.
2. Drag this file onto it:
   ${UF2}
3. After flashing, power it from any USB source (wall charger is fine).
4. On boot, it scans for one of your saved networks, connects to the
   strongest, and advertises itself over mDNS as ${BOLD}senselink.local${NC}.
5. From your Linux host: usbip list -r senselink.local

EOF
