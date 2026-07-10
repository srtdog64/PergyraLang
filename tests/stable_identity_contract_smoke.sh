#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[stable-identity] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_stable_identity.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

AST_IDENTITY="$ROOT_DIR/src/parser/ast_identity.c"
AST_API="$ROOT_DIR/src/parser/ast_api.h"
PARSER="$ROOT_DIR/src/parser/parser.c"
IMPORT_RESOLVER="$ROOT_DIR/src/compiler/import_resolver.c"
FIXTURE="$ROOT_DIR/tests/cases/stable_identity_import_merge/main.pgy"
EMITTED_C="$WORK_DIR/stable_identity.c"

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq "$text" "$file"; then
        echo "[stable-identity] missing contract text in $file: $text" >&2
        exit 1
    fi
}

require_text "$AST_API" "bool ast_assign_stable_ids(ASTNode* root);"
require_text "$AST_IDENTITY" "uint64_t next_id;"
require_text "$AST_IDENTITY" "next_id->next_id > UINT32_MAX"
require_text "$AST_IDENTITY" "return !next_id.exhausted;"
require_text "$PARSER" "if (!ast_assign_stable_ids(program))"
require_text "$IMPORT_RESOLVER" "if (program != NULL && !ast_assign_stable_ids(program))"
require_text "$IMPORT_RESOLVER" "syntax node identity space exhausted after import merge"

if grep -Fq "(void)ast_assign_stable_ids" "$PARSER" "$IMPORT_RESOLVER"; then
    echo "[stable-identity] stable identity failure is ignored" >&2
    exit 1
fi

fixture_arg="$(pgy_path_for_compiler "$PGY" "$FIXTURE")"
emitted_arg="$(pgy_path_for_compiler "$PGY" "$EMITTED_C")"
"$PGY" "$fixture_arg" --backend=c --emit-c -o "$emitted_arg" \
    >"$WORK_DIR/emit.out" 2>"$WORK_DIR/emit.err" || {
        cat "$WORK_DIR/emit.err" >&2
        exit 1
    }

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi
if [[ -z "$PYTHON_BIN" ]]; then
    echo "[stable-identity] Python is required for emitted-ID validation" >&2
    exit 1
fi

"$PYTHON_BIN" - "$EMITTED_C" <<'PY'
import pathlib
import re
import sys

text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
ids = re.findall(r"typedef struct pgy_lambda_env_(\d+) \{", text)
if len(ids) != 2:
    raise SystemExit(f"expected two captured-lambda identity rows, got {ids}")
if len(set(ids)) != len(ids):
    raise SystemExit(f"merged modules reused a SyntaxNodeId: {ids}")

def accepts(values):
    return bool(values) and len(values) == len(set(values)) and all(v != "0" for v in values)

if accepts(["13", "13"]):
    raise SystemExit("negative duplicate-ID self-test was accepted")
if accepts(["0", "14"]):
    raise SystemExit("negative zero-ID self-test was accepted")
if not accepts(ids):
    raise SystemExit(f"emitted IDs are invalid: {ids}")
PY

backends="${PGY_STABLE_IDENTITY_BACKENDS:-c}"
for backend in $backends; do
    output="$WORK_DIR/run.$backend.out"
    normalized="$WORK_DIR/run.$backend.normalized.out"
    run_target="$WORK_DIR/stable_identity_$backend"
    run_target_arg="$(pgy_path_for_compiler "$PGY" "$run_target")"
    if ! "$PGY" "$fixture_arg" --backend="$backend" --run -o "$run_target_arg" \
        >"$output" 2>&1; then
        cat "$output" >&2
        exit 1
    fi
    tr -d '\r' <"$output" >"$normalized"
    if ! grep -Fxq '11' "$normalized" || ! grep -Fxq '12' "$normalized"; then
        echo "[stable-identity] backend=$backend output mismatch" >&2
        cat "$output" >&2
        exit 1
    fi
    echo "[stable-identity] backend=$backend import-merge identities unique"
done

echo "[stable-identity] overflow fail-close and duplicate-ID regression locked"
