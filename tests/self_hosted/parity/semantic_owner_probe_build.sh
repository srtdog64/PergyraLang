#!/usr/bin/env bash
# Producer of the shared semantic owner probe build (see
# semantic_owner_probe_owner.sh). usage: semantic_owner_probe_build.sh DIR SECONDS
# A directory whose receipts still match is kept; anything else is rebuilt.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/semantic_owner_probe_owner.sh"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DIR="${1:?shared build directory}"
SECONDS_LIMIT="${2:?build limit in seconds}"
mkdir -p "$DIR"
if pgy_semantic_owner_probe_validate "$ROOT_DIR" "$PGY" "$DIR" 2>/dev/null; then
    echo "[semantic-owner-probe] receipts match; kept $DIR"
    exit 0
fi
pgy_semantic_owner_probe_build "$ROOT_DIR" "$PGY" "$DIR" "$SECONDS_LIMIT"
