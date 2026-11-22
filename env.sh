# Source me: the build environment, one Zephyr tree for every board (4.5, #47).
export ZEPHYR_BASE="$HOME/zephyr-4.5/zephyr"
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk-1.0.1"
export PATH="$HOME/zephyr-4.5/.venv/bin:$PATH"
export TOIT_SRC="$HOME/workspaceToit/toit-v2.0.0"
export TOIT_HOST="$HOME/.local/state/ztoit/sdk-fork/toit/bin/toit"  # tools/fork-sdk.sh: stock binaries + fork lib/
