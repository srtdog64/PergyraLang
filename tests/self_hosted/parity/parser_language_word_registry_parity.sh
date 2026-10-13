#!/usr/bin/env bash
# Focused parser gate for registry-owned language-word selectors.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/parser_tool_build_leg.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/parser_language_word_registry}"
POSITIVE_REL="src/self_hosted/parser/fixture/language_word_roles.pgy"
NEGATIVE_REL="src/self_hosted/parser/reject_fixture/systemic_slot.pgy"
SELF_BIN="$BUILD_DIR/parser_c.exe"
mkdir -p "$BUILD_DIR"

if [[ ! -x "$PGY" ]]; then
    echo "[self-host-parity:parser-language-word] missing compiler binary: $PGY" >&2
    exit 1
fi
PGY_EXEC="$(pgy_path_for_bash_tool "$PGY")"

pgy_selfhost_compile_parser_tool \
    "self-host-parity:parser-language-word" \
    "$ROOT_DIR/src/self_hosted/parser/main.pgy" c \
    "$SELF_BIN" "$BUILD_DIR/parser_c.compile.log"

(cd "$ROOT_DIR" && "$PGY_EXEC" --native-pipeline --ast \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/$POSITIVE_REL")") \
    >"$BUILD_DIR/positive.native.ast" 2>"$BUILD_DIR/positive.native.err"
(cd "$ROOT_DIR" && "$SELF_BIN" "$POSITIVE_REL") \
    >"$BUILD_DIR/positive.self.ast" 2>"$BUILD_DIR/positive.self.err"
tr -d '\r' <"$BUILD_DIR/positive.native.ast" >"$BUILD_DIR/positive.native.norm"
tr -d '\r' <"$BUILD_DIR/positive.self.ast" >"$BUILD_DIR/positive.self.norm"
if ! cmp -s "$BUILD_DIR/positive.native.norm" "$BUILD_DIR/positive.self.norm"; then
    echo "[self-host-parity:parser-language-word] action/impl/ref/own/type AST parity failed" >&2
    diff -u "$BUILD_DIR/positive.native.norm" "$BUILD_DIR/positive.self.norm" >&2 || true
    exit 1
fi

set +e
(cd "$ROOT_DIR" && "$PGY_EXEC" --native-pipeline --ast \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/$NEGATIVE_REL")") \
    >"$BUILD_DIR/systemic.native.out" 2>"$BUILD_DIR/systemic.native.err"
native_rc=$?
(cd "$ROOT_DIR" && "$SELF_BIN" "$NEGATIVE_REL") \
    >"$BUILD_DIR/systemic.self.out" 2>"$BUILD_DIR/systemic.self.err"
self_rc=$?
set -e

if [[ "$native_rc" -ne 1 || "$self_rc" -ne 1 ]]; then
    echo "[self-host-parity:parser-language-word] systemic slot rejection boundary drifted (native=$native_rc self=$self_rc)" >&2
    cat "$BUILD_DIR/systemic.native.out" "$BUILD_DIR/systemic.native.err" \
        "$BUILD_DIR/systemic.self.out" "$BUILD_DIR/systemic.self.err" >&2
    exit 1
fi
if grep -Fq "Program:" "$BUILD_DIR/systemic.native.out" \
    "$BUILD_DIR/systemic.self.out"; then
    echo "[self-host-parity:parser-language-word] systemic rejection emitted an AST" >&2
    exit 1
fi

echo "[self-host-parity:parser-language-word] typed selectors and parser boundaries ok"
