#!/usr/bin/env bash

set -euo pipefail

RED="\e[1;31m"
GREEN="\e[1;32m"
BLUE="\e[1;34m"
YELLOW="\e[1;33m"
NC="\e[0m"

# Resolves ESW_ROOT to the 'fw/' directory
ESW_ROOT="$(dirname "$(dirname "$(realpath "$0")")")"
SRC=""
PRESET="Debug"
TARGET_NAME=""
DO_FLASH=false
DO_CLEAN=false

PORT="${PORT:-swd}"
FREQ="${FREQ:-8000}"
RESET="${RESET:-HWrst}"
MODE="${MODE:-UR}"
SCRIPT_NAME=$(basename "$0")

usage() {
    cat <<EOF
Usage: $SCRIPT_NAME --src <path> [options]

options:
  -s, --src <path>      path to source directory (required)
  -p, --preset <name>   cmake preset (default: Debug)
  -t, --target <name>   cmake target name (default: build all targets)
  -f, --flash           flash CM7 and CM4 binaries after build
  -c, --clean           clean build directory
  -h, --help            show this help message
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
    local deps=("cmake" "ninja")
    [[ "$DO_FLASH" == "true" ]] && deps+=("STM32_Programmer_CLI")
    for cmd in "${deps[@]}"; do
        if ! command -v "$cmd" &> /dev/null; then
            printf "%b\n" "${RED}✗ error: command '$cmd' not found in PATH${NC}"
            exit 1
        fi
    done
}

check_submodules() {
    local submodule_dir="$ESW_ROOT/stm32h7/STM32CubeH7"
    if [ ! -d "$submodule_dir/Drivers" ]; then
        printf "%b\n" "${RED}✗ error: STM32CubeH7 drivers not found at $submodule_dir/Drivers${NC}"
        printf "%b\n" "${RED}  run: git submodule update --init --recursive${NC}"
        exit 1
    fi
}

clean() {
    local build_dir="$1"
    rm -rf "$build_dir"
}

# parse flags
while [[ $# -gt 0 ]]; do
    case "$1" in
        -s|--src)       SRC="$(realpath "$2")"; shift 2 ;;
        -p|--preset)    PRESET="$2"; shift 2 ;;
        -t|--target)    TARGET_NAME="$2"; shift 2 ;;
        -f|--flash)     DO_FLASH=true; shift ;;
        -c|--clean)     DO_CLEAN=true; shift ;;
        -h|--help)      usage ;;
        *)              printf "%b\n" "${RED}✗ unknown option: $1${NC}"; usage ;;
    esac
done

if [[ -z "$SRC" ]]; then
    printf "%b\n" "${RED}✗ error: -s/--src required${NC}"
    usage
fi

SRC_DIR="$SRC"
if [ ! -d "$SRC_DIR" ]; then
    printf "%b\n" "${RED}✗ failed: directory $SRC_DIR does not exist${NC}"
    exit 1
fi

check_deps
check_submodules

printf "%b\n" "${BLUE}====== project: ${YELLOW}$(basename "$SRC_DIR")${BLUE} | preset: ${YELLOW}$PRESET${BLUE} ======${NC}"

pushd "$SRC_DIR" > /dev/null
BUILD_DIR="build/$PRESET"

if [[ "$DO_CLEAN" == "true" ]]; then
    run_step "cleaning build dir" clean "$BUILD_DIR"
    popd > /dev/null
    exit 0
fi

if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    mkdir -p "$BUILD_DIR"
    run_step "configure cmake" cmake --preset "$PRESET"
fi

# execute build
if [[ -n "$TARGET_NAME" ]]; then
    run_step "build target $TARGET_NAME" cmake --build --preset "$PRESET" --target "$TARGET_NAME"
else
    run_step "build all targets (CM7 & CM4)" cmake --build --preset "$PRESET"
fi

# flash if parameter set
if [[ "$DO_FLASH" == "true" ]]; then
    mapfile -t ELFS < <(find "$SRC_DIR" -type f -name "*.elf")

    if [[ ${#ELFS[@]} -eq 0 ]]; then
        printf "%b\n" "${RED}✗ flash failed: No .elf files found in $BUILD_DIR${NC}"
        exit 1
    fi

    FLASH_CMD=(STM32_Programmer_CLI --connect port="$PORT" freq="$FREQ" reset="$RESET" mode="$MODE")
    for elf in "${ELFS[@]}"; do
        printf "%b\n" "${BLUE}adding ELF: ${elf}${NC}"
        FLASH_CMD+=(--write "$elf")
    done
    FLASH_CMD+=(--verify --start)

    run_step "flash mcu" "${FLASH_CMD[@]}"
fi

popd > /dev/null

printf "%b\n" "${GREEN}====== success ======${NC}"