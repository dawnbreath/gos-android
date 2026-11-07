#!/usr/bin/env bash
# Checks the AAPCS64 trampoline under qemu (needs g++-aarch64-linux-gnu, qemu-user).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; U="$ROOT/third_party/GoSRecomp"
OUT="$(mktemp -d)"
aarch64-linux-gnu-g++ -O2 -std=c++17 -static -I"$U/recomp/runtime" "$ROOT/tests/aapcs_test.cpp" "$U/recomp/android/aapcs_call.S" -o "$OUT/aapcs_test"
qemu-aarch64 "$OUT/aapcs_test"
