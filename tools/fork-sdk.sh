#!/bin/sh
# Make the host SDK that compiles ztoit programs: stock v2.0.0 binaries with the
# toit fork's lib/ (symlinked), so library changes on zephyr-port take effect.
# Binaries are copied, not linked: the compiler finds its lib next to its real path.
set -e
STOCK="$HOME/.local/state/ztoit/sdk/toit"
FORK="$HOME/.local/state/ztoit/sdk-fork/toit"
: "${TOIT_SRC:?source env.sh}"
rm -rf "$FORK" && mkdir -p "$FORK/lib/toit"
cp -a "$STOCK/bin" "$FORK/bin"
cp -a "$STOCK/lib/toit/bin" "$FORK/lib/toit/bin"
ln -s "$STOCK/lib/toit/vessels" "$FORK/lib/toit/vessels"
ln -s "$TOIT_SRC/lib" "$FORK/lib/toit/lib"
echo "$FORK/bin/toit"
