#!/usr/bin/env bash

set -euo pipefail

RED="\e[1;31m"
GREEN="\e[1;32m"
BLUE="\e[1;34m"
YELLOW="\e[1;33m"
NC="\e[0m"

# -s so the demo_f4 symlink builds demo_f4
DEMO_ROOT="$(dirname "$(dirname "$(realpath -s "$0")")")"
SCRIPT_NAME=$(basename "$0")

usage() {
    cat <<EOF
Usage: $SCRIPT_NAME <a|b> [st-link serial]

  a     build and flash board A, the HaLow access point (blinks its LED on each press)
  b     build and flash board B, the station (its blue button signals board A)

With both boards plugged in, pass the serial of the one to flash.
List serials with: STM32_Programmer_CLI --list
EOF
    exit 1
}

run_step() {
    local desc="$1"
    shift
    printf "%b\n" "${BLUE}step: ${desc}${NC}"
    if "$@"; then
        printf "%b\n" "${GREEN}✓ success: ${desc}${NC}"
    else
        printf "%b\n" "${RED}✗ failed: ${desc}${NC}"
        exit 1
    fi
}

check_deps() {
    for cmd in make STM32_Programmer_CLI; do
        if ! command -v "$cmd" &> /dev/null; then
            printf "%b\n" "${RED}✗ error: command '$cmd' not found in PATH (install STM32CubeCLT)${NC}"
            exit 1
        fi
    done
}

check_submodule() {
    if [ -n "$(git -C "$DEMO_ROOT/.." submodule status | grep '^-')" ]; then
        printf "%b\n" "${RED}✗ error: submodules missing, run: git submodule update --init${NC}"
        exit 1
    fi
}

[[ $# -lt 1 || $# -gt 2 ]] && usage

case "$1" in
    a|A) APP="ap_mode"; BOARD="A (access point)" ;;
    b|B) APP="station"; BOARD="B (station)" ;;
    -h|--help) usage ;;
    *) printf "%b\n" "${RED}✗ unknown board: $1${NC}"; usage ;;
esac
SN="${2:-}"

check_deps
check_submodule

printf "%b\n" "${BLUE}====== board: ${YELLOW}${BOARD}${BLUE} | app: ${YELLOW}${APP}${BLUE} ======${NC}"

run_step "build $APP" make -C "$DEMO_ROOT" APP="$APP" -j"$(nproc)"
run_step "flash board $1" make -C "$DEMO_ROOT" APP="$APP" SN="$SN" flash

printf "%b\n" "${GREEN}====== success ======${NC}"
