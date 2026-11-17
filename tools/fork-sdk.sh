#!/bin/sh
# Make the host SDK that compiles ztoit programs: stock v2.0.0 binaries with the
# toit fork's lib/ (symlinked), so library changes on zephyr-port take effect,
# and the fork's toit.compile, built here, which knows the `zephyr` primitives.
# Binaries are copied, not linked: the compiler finds its lib next to its real path.
set -e
STOCK="$HOME/.local/state/ztoit/sdk/toit"
FORK="${ZTOIT_SDK_FORK:-$HOME/.local/state/ztoit/sdk-fork/toit}"  # override: a second fork worktree
: "${TOIT_SRC:?source env.sh}"
rm -rf "$FORK" && mkdir -p "$FORK/lib/toit"
cp -a "$STOCK/bin" "$FORK/bin"
cp -a "$STOCK/lib/toit/bin" "$FORK/lib/toit/bin"
HOST="${ZTOIT_TOIT_HOST_BUILD:-$HOME/.local/state/ztoit/toit-host}"
cmake -S "$TOIT_SRC" -B "$HOST" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$TOIT_SRC/toolchains/host.cmake" -DTOIT_PKG_AUTO_SYNC=OFF --no-warn-unused-cli >/dev/null
ninja -C "$HOST" toit.compile >/dev/null
cp "$HOST/sdk/lib/toit/bin/toit.compile" "$FORK/lib/toit/bin/toit.compile"
ln -s "$STOCK/lib/toit/vessels" "$FORK/lib/toit/vessels"
ln -s "$TOIT_SRC/lib" "$FORK/lib/toit/lib"
echo "$FORK/bin/toit"
