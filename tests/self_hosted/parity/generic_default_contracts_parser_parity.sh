#!/usr/bin/env bash
# Focused parser parity for declaration-site generic defaults.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_text_mutation_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/parser_tool_build_leg.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/generic_default_contracts}"
SOURCE="tests/cases/backend_compare/generic_default_contracts/main.pgy"
SELF_BIN="$BUILD_DIR/parser_c.exe"
SELF_LOG="$BUILD_DIR/parser_c.compile.log"
SELF_OUT="$BUILD_DIR/self.ast.txt"
NATIVE_OUT="$BUILD_DIR/native.ast.txt"
MUTATED="$BUILD_DIR/invalid_default.pgy"
mkdir -p "$BUILD_DIR"

pgy_selfhost_compile_parser_tool \
    "self-host-parity:generic-defaults" \
    "$ROOT_DIR/src/self_hosted/parser/main.pgy" c "$SELF_BIN" "$SELF_LOG"

(cd "$ROOT_DIR" && "$SELF_BIN" "$SOURCE") >"$SELF_OUT"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline --ast "$SOURCE") >"$NATIVE_OUT"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:generic-defaults" "$BUILD_DIR" \
    "$NATIVE_OUT" "$SELF_OUT" "ast_text"

pgy_replace_first_literal "$ROOT_DIR/$SOURCE" "$MUTATED" \
    'Box<T = Int>' 'Box<T = >'
if (cd "$ROOT_DIR" && "$SELF_BIN" \
    "$(pgy_selfhost_path_relative_to_root "$MUTATED")" \
    >"$MUTATED.self.out" 2>"$MUTATED.self.err"); then
    echo "[self-host-parity:generic-defaults] self-host accepted invalid default" >&2
    exit 1
fi
if (cd "$ROOT_DIR" && "$PGY" --native-pipeline --ast \
    "$(pgy_path_for_compiler "$PGY" "$MUTATED")" \
    >"$MUTATED.native.out" 2>"$MUTATED.native.err"); then
    echo "[self-host-parity:generic-defaults] native accepted invalid default" >&2
    exit 1
fi

echo "[self-host-parity:generic-defaults] declaration generic default parity ok"
