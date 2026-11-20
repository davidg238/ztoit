#!/bin/sh
# Host tests for C/C++ code in the Toit fork that needs no board (ztoit #41).
set -e
RETE=${RETE:-$HOME/workspace/rete}
FORK=${FORK:-$HOME/workspaceToit/toit-v2.0.0}
OUT=${TMPDIR:-/tmp}/ztoit-test-host
mkdir -p "$OUT"
g++ -std=c++17 -Wall -Wextra -Werror -I"$FORK/src/resources" \
  "$(dirname "$0")/../tests/host/test_ble_codecs.cc" -o "$OUT/test_ble_codecs"
"$OUT/test_ble_codecs" "$RETE" "$FORK/src/resources"
