#!/usr/bin/env bash
# Producer of the shared MIR lowering tool build (see
# mir_lower_tool_build_owner.sh). usage: mir_lower_tool_build.sh DIR
# The launcher is PGY_BIN, else bin/pgy, as in the gates that run the tool.
# A directory whose receipts still match is kept; anything else is rebuilt.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/mir_lower_tool_build_owner.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DIR="${1:?shared build directory}"
pgy_require_runnable_binary_here "mir-lower-tool" "$PGY" || exit 1
mkdir -p "$DIR"
if pgy_mir_lower_tool_validate "$ROOT_DIR" "$PGY" "$(cd "$DIR" && pwd -P)" 2>/dev/null; then
    echo "[mir-lower-tool] receipts match; kept $DIR"
    exit 0
fi
pgy_mir_lower_tool_build "$ROOT_DIR" "$PGY" "$DIR"
