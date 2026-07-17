#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq -- "$text" "$ROOT_DIR/$file"; then
        echo "[source-module-graph] missing $file contract: $text" >&2
        exit 1
    fi
}

require_text src/compiler/module_loader.h "PgySourceModuleGraph"
require_text src/compiler/module_loader.h "first_syntax_id"
require_text src/compiler/module_loader.h "module_loader_load_program_with_graph"
require_text src/compiler/module_loader.c "module_loader_build_graph"
require_text src/compiler/module_loader.c "import_resolver_canonicalize_path_dup"
require_text src/compiler/module_loader.c "module_loader_validate_graph"
require_text src/compiler/driver_app.c "module_loader_load_program_with_graph"
require_text src/compiler/driver_app.c "module_loader_validate_graph"
require_text src/compiler/driver_app.c "module_loader_destroy_graph"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[source-module-graph] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_module_graph.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
OUT="$WORK_DIR/basic.exe"
LOG="$WORK_DIR/compile.log"
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/examples/basic.pgy")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$OUT")" \
    >"$LOG" 2>&1)
test -s "$OUT"

echo "[source-module-graph] driver validates the anchored source/module graph"
