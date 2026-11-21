#!/usr/bin/env bash
# builds the ap link test and flashes it to the first ST-LINK, extra options go to harness.py flash, e.g. --index 1 --bw 1
set -euo pipefail
exec "$(dirname "$(realpath "$0")")/harness.py" flash ap "$@"
