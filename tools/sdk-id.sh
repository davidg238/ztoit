#!/bin/sh
# Print the SDK identity (ztoit#24): the stock SDK's version, then the Toit fork's
# commit, with -dirty if the fork has uncommitted changes to tracked files.
# tools/fork-sdk.sh stamps it into $TOIT_HOST version; the firmware reports it as
# obs/build sdk (zephyr/ztoit_ids.cmake). They must match for an install.
set -e
: "${TOIT_SRC:?source env.sh}"
STOCK="${ZTOIT_SDK_STOCK:-$HOME/.local/state/ztoit/sdk/toit}"  # as tools/fork-sdk.sh
sha=$(git -C "$TOIT_SRC" rev-parse --short=8 HEAD)
# Untracked files don't count, nor the two third_party trees some worktrees show as typechanged.
if [ -n "$(git -C "$TOIT_SRC" status --porcelain --untracked-files=no -- . \
  ':!third_party/esp-idf' ':!third_party/luatos-soc-ec618')" ]; then
  sha="$sha-dirty"
fi
echo "$("$STOCK/bin/toit" version)+zephyr.$sha"
