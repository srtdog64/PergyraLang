#!/usr/bin/env bash
# Native source lowering must evaluate a non-trivial match subject once before
# any case condition or payload extraction consumes it.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

# Subject of this gate:
#   native source-to-MIR match-subject single-evaluation semantics.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
BACKENDS="${PGY_NATIVE_BOUNDARY_BACKENDS:-c}"
SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/match_scrutinee_single_evaluation.pgy"
TMP_BASE="${TMPDIR:-${TEMP:-$ROOT_DIR/.tmp}}"
mkdir -p "$TMP_BASE"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy-native-match-once.XXXXXX")"
trap 'rm -rf -- "$WORK_DIR"' EXIT

fail() {
    echo "[native-match-once] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$ROOT_DIR/$1" ||
        fail "missing owner contract in $1: $2"
}

pgy_require_runnable_binary_here "native-match-once" "$PGY" || exit 1
require_text src/compiler/driver_app.c \
    'match_subject_single_evaluation_desugar_program(ast);'
require_text src/compiler/match_subject_single_evaluation_desugar.c \
    'subject = ast_match_detach_subject(match);'
require_text src/compiler/match_subject_single_evaluation_desugar.c \
    'if (!ast_match_attach_subject(match, ident))'
require_text src/parser/ast_block_match_event_accessors.c \
    'ast_match_detach_subject(ASTNode* node)'

printf 'probe-some\n7\nprobe-none\n0\n' >"$WORK_DIR/expected.out"
suffix=""
pgy_binary_expects_windows_paths "$PGY" && suffix=".exe"

for backend in $BACKENDS; do
    program="$WORK_DIR/match-$backend$suffix"
    source_arg="$(pgy_path_for_compiler "$PGY" "$SOURCE")"
    program_arg="$(pgy_path_for_compiler "$PGY" "$program")"
    (cd "$ROOT_DIR" && "$PGY" "$source_arg" --native-pipeline \
        "--backend=$backend" -o "$program_arg") \
        >"$WORK_DIR/$backend.compile.out" \
        2>"$WORK_DIR/$backend.compile.err" || {
            cat "$WORK_DIR/$backend.compile.out" \
                "$WORK_DIR/$backend.compile.err" >&2
            fail "$backend compilation failed"
        }
    [[ -x "$program" ]] || fail "$backend published no executable"
    "$program" | tr -d '\r' >"$WORK_DIR/$backend.run.out"
    cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/$backend.run.out" || {
        cat "$WORK_DIR/$backend.run.out" >&2
        fail "$backend re-evaluated the match subject"
    }
done

echo "[native-match-once] $BACKENDS evaluate each match subject once: PASS"
