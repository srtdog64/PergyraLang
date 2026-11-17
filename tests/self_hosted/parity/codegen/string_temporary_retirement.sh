#!/usr/bin/env bash
# The Pergyra-built AST emitter is the subject; no direct-MIR substitution.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CODEGEN="$(pgy_select_optional_exe_binary "${PGY_CODEGEN_BIN:-$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap/gen2.exe}")"
PYTHON="${PYTHON_BIN:-python3}"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/string-retirement.XXXXXX")"
[[ "$(uname -s)" == Linux ]] || {
    echo "[string-retirement] this ASan/LSan gate requires Linux" >&2; exit 1;
}
cd "$ROOT_DIR"
"$PYTHON" tests/self_hosted/parity/codegen/string_temporary_retirement_probe.py \
    "$ROOT_DIR" "$WORK" "$PGY" "$CODEGEN" "${PGY_SELFHOST_CC:-gcc}"
