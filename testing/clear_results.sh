#!/usr/bin/env bash
# deletes every run under results/, asks first unless given -y
set -euo pipefail

results="$(dirname "$(realpath "$0")")/results"
shopt -s nullglob
runs=("$results"/*/)
if [[ ${#runs[@]} -eq 0 ]]; then
    echo "no runs in $results"
    exit 0
fi

if [[ "${1:-}" != "-y" ]]; then
    printf '%s\n' "${runs[@]##"$results"/}"
    read -r -p "delete these ${#runs[@]} runs? [y/N] " answer
    [[ "$answer" =~ ^[Yy]$ ]] || { echo "kept"; exit 0; }
fi
rm -rf -- "${runs[@]}"
echo "deleted ${#runs[@]} runs"
