#!/usr/bin/env bash
# Parity gate for the self-host compatibility-evolution corpus checker.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:compatibility-corpus] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:compatibility-corpus] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/compatibility_corpus}"
HARNESS_PATHS_FILE="$BUILD_DIR/compatibility_corpus_harness_paths.txt"
mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:compatibility-corpus" \
    "$BUILD_DIR" \
    "compatibility-corpus-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 7 ]]; then
    echo "[self-host-parity:compatibility-corpus] TestHarness manifest expected 7 compatibility corpus paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_FILE="$ROOT_DIR/${harness_paths[1]}"
NEGATIVE_EXPECTED_FILE="$ROOT_DIR/${harness_paths[2]}"
INVALID_CODEFIX_EXPECTED_FILE="$ROOT_DIR/${harness_paths[3]}"
INVALID_CHANGE_KIND_EXPECTED_FILE="$ROOT_DIR/${harness_paths[4]}"
INVALID_OBSOLETE_MIGRATION_EXPECTED_FILE="$ROOT_DIR/${harness_paths[5]}"
MISSING_SURFACE_EXPECTED_FILE="$ROOT_DIR/${harness_paths[6]}"
for path in "$TOOL_SOURCE" "$EXPECTED_FILE" "$NEGATIVE_EXPECTED_FILE" "$INVALID_CODEFIX_EXPECTED_FILE" "$INVALID_CHANGE_KIND_EXPECTED_FILE" "$INVALID_OBSOLETE_MIGRATION_EXPECTED_FILE" "$MISSING_SURFACE_EXPECTED_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:compatibility-corpus] missing input: $path" >&2
        exit 1
    fi
done

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")"
C_BIN="$BUILD_DIR/compatibility_corpus_c.exe"
C_COMPILE_LOG="$BUILD_DIR/compatibility_corpus_c.compile.log"
C_OUT="$BUILD_DIR/compatibility_corpus_c.out"
C_ERR="$BUILD_DIR/compatibility_corpus_c.err"
C_NEG_OUT="$BUILD_DIR/compatibility_corpus_c_negative.out"
C_NEG_ERR="$BUILD_DIR/compatibility_corpus_c_negative.err"
C_INVALID_CODEFIX_OUT="$BUILD_DIR/compatibility_corpus_c_invalid_codefix.out"
C_INVALID_CODEFIX_ERR="$BUILD_DIR/compatibility_corpus_c_invalid_codefix.err"
C_INVALID_CHANGE_KIND_OUT="$BUILD_DIR/compatibility_corpus_c_invalid_change_kind.out"
C_INVALID_CHANGE_KIND_ERR="$BUILD_DIR/compatibility_corpus_c_invalid_change_kind.err"
C_INVALID_OBSOLETE_MIGRATION_OUT="$BUILD_DIR/compatibility_corpus_c_invalid_obsolete_migration.out"
C_INVALID_OBSOLETE_MIGRATION_ERR="$BUILD_DIR/compatibility_corpus_c_invalid_obsolete_migration.err"
C_MISSING_SURFACE_OUT="$BUILD_DIR/compatibility_corpus_c_missing_surface.out"
C_MISSING_SURFACE_ERR="$BUILD_DIR/compatibility_corpus_c_missing_surface.err"

if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$C_BIN")" >"$C_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:compatibility-corpus] C compile failed" >&2
    cat "$C_COMPILE_LOG" >&2
    exit 1
fi

set +e
(cd "$ROOT_DIR" && "$C_BIN" 2>"$C_ERR" | pgy_selfhost_normalize_text_artifact >"$C_OUT")
C_RC=$?
set -e
if [[ "$C_RC" -ne 0 ]]; then
    echo "[self-host-parity:compatibility-corpus] C-compiled checker failed" >&2
    cat "$C_OUT" "$C_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:compatibility-corpus" \
    "$BUILD_DIR" \
    "$EXPECTED_FILE" \
    "$(cat "$C_OUT")" \
    "run_output"

set +e
(cd "$ROOT_DIR" && "$C_BIN" --self-test-malformed-row 2>"$C_NEG_ERR" | pgy_selfhost_normalize_text_artifact >"$C_NEG_OUT")
C_NEG_RC=$?
set -e
if [[ "$C_NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:compatibility-corpus] malformed-row self-test should fail closed (rc=1), got rc=$C_NEG_RC" >&2
    cat "$C_NEG_OUT" "$C_NEG_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:compatibility-corpus" \
    "$BUILD_DIR" \
    "$NEGATIVE_EXPECTED_FILE" \
    "$C_NEG_OUT" \
    "run_output"

set +e
(cd "$ROOT_DIR" && "$C_BIN" --self-test-invalid-codefix-status 2>"$C_INVALID_CODEFIX_ERR" | pgy_selfhost_normalize_text_artifact >"$C_INVALID_CODEFIX_OUT")
C_INVALID_CODEFIX_RC=$?
set -e
if [[ "$C_INVALID_CODEFIX_RC" -ne 1 ]]; then
    echo "[self-host-parity:compatibility-corpus] invalid-codefix self-test should fail closed (rc=1), got rc=$C_INVALID_CODEFIX_RC" >&2
    cat "$C_INVALID_CODEFIX_OUT" "$C_INVALID_CODEFIX_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:compatibility-corpus" \
    "$BUILD_DIR" \
    "$INVALID_CODEFIX_EXPECTED_FILE" \
    "$C_INVALID_CODEFIX_OUT" \
    "run_output"

set +e
(cd "$ROOT_DIR" && "$C_BIN" --self-test-invalid-change-kind 2>"$C_INVALID_CHANGE_KIND_ERR" | pgy_selfhost_normalize_text_artifact >"$C_INVALID_CHANGE_KIND_OUT")
C_INVALID_CHANGE_KIND_RC=$?
set -e
if [[ "$C_INVALID_CHANGE_KIND_RC" -ne 1 ]]; then
    echo "[self-host-parity:compatibility-corpus] invalid-change-kind self-test should fail closed (rc=1), got rc=$C_INVALID_CHANGE_KIND_RC" >&2
    cat "$C_INVALID_CHANGE_KIND_OUT" "$C_INVALID_CHANGE_KIND_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:compatibility-corpus" \
    "$BUILD_DIR" \
    "$INVALID_CHANGE_KIND_EXPECTED_FILE" \
    "$C_INVALID_CHANGE_KIND_OUT" \
    "run_output"

set +e
(cd "$ROOT_DIR" && "$C_BIN" --self-test-invalid-obsolete-migration 2>"$C_INVALID_OBSOLETE_MIGRATION_ERR" | pgy_selfhost_normalize_text_artifact >"$C_INVALID_OBSOLETE_MIGRATION_OUT")
C_INVALID_OBSOLETE_MIGRATION_RC=$?
set -e
if [[ "$C_INVALID_OBSOLETE_MIGRATION_RC" -ne 1 ]]; then
    echo "[self-host-parity:compatibility-corpus] invalid-obsolete-migration self-test should fail closed (rc=1), got rc=$C_INVALID_OBSOLETE_MIGRATION_RC" >&2
    cat "$C_INVALID_OBSOLETE_MIGRATION_OUT" "$C_INVALID_OBSOLETE_MIGRATION_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:compatibility-corpus" \
    "$BUILD_DIR" \
    "$INVALID_OBSOLETE_MIGRATION_EXPECTED_FILE" \
    "$C_INVALID_OBSOLETE_MIGRATION_OUT" \
    "run_output"

set +e
(cd "$ROOT_DIR" && "$C_BIN" --self-test-missing-surface 2>"$C_MISSING_SURFACE_ERR" | pgy_selfhost_normalize_text_artifact >"$C_MISSING_SURFACE_OUT")
C_MISSING_SURFACE_RC=$?
set -e
if [[ "$C_MISSING_SURFACE_RC" -ne 1 ]]; then
    echo "[self-host-parity:compatibility-corpus] missing-surface self-test should fail closed (rc=1), got rc=$C_MISSING_SURFACE_RC" >&2
    cat "$C_MISSING_SURFACE_OUT" "$C_MISSING_SURFACE_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:compatibility-corpus" \
    "$BUILD_DIR" \
    "$MISSING_SURFACE_EXPECTED_FILE" \
    "$C_MISSING_SURFACE_OUT" \
    "run_output"

assert_llvm_leg "self-host-parity:compatibility-corpus" "$TOOL_ARG" "$BUILD_DIR"

LLVM_NEG_BIN="$BUILD_DIR/compatibility_corpus_llvm_negative.exe"
LLVM_NEG_COMPILE_LOG="$BUILD_DIR/compatibility_corpus_llvm_negative.compile.log"
LLVM_NEG_OUT="$BUILD_DIR/compatibility_corpus_llvm_negative.out"
LLVM_NEG_ERR="$BUILD_DIR/compatibility_corpus_llvm_negative.err"
LLVM_INVALID_CODEFIX_OUT="$BUILD_DIR/compatibility_corpus_llvm_invalid_codefix.out"
LLVM_INVALID_CODEFIX_ERR="$BUILD_DIR/compatibility_corpus_llvm_invalid_codefix.err"
LLVM_INVALID_CHANGE_KIND_OUT="$BUILD_DIR/compatibility_corpus_llvm_invalid_change_kind.out"
LLVM_INVALID_CHANGE_KIND_ERR="$BUILD_DIR/compatibility_corpus_llvm_invalid_change_kind.err"
LLVM_INVALID_OBSOLETE_MIGRATION_OUT="$BUILD_DIR/compatibility_corpus_llvm_invalid_obsolete_migration.out"
LLVM_INVALID_OBSOLETE_MIGRATION_ERR="$BUILD_DIR/compatibility_corpus_llvm_invalid_obsolete_migration.err"
LLVM_MISSING_SURFACE_OUT="$BUILD_DIR/compatibility_corpus_llvm_missing_surface.out"
LLVM_MISSING_SURFACE_ERR="$BUILD_DIR/compatibility_corpus_llvm_missing_surface.err"
if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=llvm \
    -o "$(pgy_path_for_compiler "$PGY" "$LLVM_NEG_BIN")" >"$LLVM_NEG_COMPILE_LOG" 2>&1); then
    if pgy_selfhost_log_reports_no_llvm "$LLVM_NEG_COMPILE_LOG"; then
        echo "[self-host-parity:compatibility-corpus] malformed-row llvm-leg skipped (compiler built without LLVM backend support)"
    else
        echo "[self-host-parity:compatibility-corpus] malformed-row LLVM compile failed" >&2
        cat "$LLVM_NEG_COMPILE_LOG" >&2
        exit 1
    fi
else
    set +e
    (cd "$ROOT_DIR" && "$LLVM_NEG_BIN" --self-test-malformed-row 2>"$LLVM_NEG_ERR" | pgy_selfhost_normalize_text_artifact >"$LLVM_NEG_OUT")
    LLVM_NEG_RC=$?
    set -e
    if [[ "$LLVM_NEG_RC" -ne 1 ]]; then
        echo "[self-host-parity:compatibility-corpus] malformed-row LLVM self-test should fail closed (rc=1), got rc=$LLVM_NEG_RC" >&2
        cat "$LLVM_NEG_OUT" "$LLVM_NEG_ERR" >&2
        exit 1
    fi
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "self-host-parity:compatibility-corpus" \
        "$BUILD_DIR" \
        "$NEGATIVE_EXPECTED_FILE" \
        "$LLVM_NEG_OUT" \
        "run_output"

    set +e
    (cd "$ROOT_DIR" && "$LLVM_NEG_BIN" --self-test-invalid-codefix-status 2>"$LLVM_INVALID_CODEFIX_ERR" | pgy_selfhost_normalize_text_artifact >"$LLVM_INVALID_CODEFIX_OUT")
    LLVM_INVALID_CODEFIX_RC=$?
    set -e
    if [[ "$LLVM_INVALID_CODEFIX_RC" -ne 1 ]]; then
        echo "[self-host-parity:compatibility-corpus] invalid-codefix LLVM self-test should fail closed (rc=1), got rc=$LLVM_INVALID_CODEFIX_RC" >&2
        cat "$LLVM_INVALID_CODEFIX_OUT" "$LLVM_INVALID_CODEFIX_ERR" >&2
        exit 1
    fi
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "self-host-parity:compatibility-corpus" \
        "$BUILD_DIR" \
        "$INVALID_CODEFIX_EXPECTED_FILE" \
        "$LLVM_INVALID_CODEFIX_OUT" \
        "run_output"

    set +e
    (cd "$ROOT_DIR" && "$LLVM_NEG_BIN" --self-test-invalid-change-kind 2>"$LLVM_INVALID_CHANGE_KIND_ERR" | pgy_selfhost_normalize_text_artifact >"$LLVM_INVALID_CHANGE_KIND_OUT")
    LLVM_INVALID_CHANGE_KIND_RC=$?
    set -e
    if [[ "$LLVM_INVALID_CHANGE_KIND_RC" -ne 1 ]]; then
        echo "[self-host-parity:compatibility-corpus] invalid-change-kind LLVM self-test should fail closed (rc=1), got rc=$LLVM_INVALID_CHANGE_KIND_RC" >&2
        cat "$LLVM_INVALID_CHANGE_KIND_OUT" "$LLVM_INVALID_CHANGE_KIND_ERR" >&2
        exit 1
    fi
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "self-host-parity:compatibility-corpus" \
        "$BUILD_DIR" \
        "$INVALID_CHANGE_KIND_EXPECTED_FILE" \
        "$LLVM_INVALID_CHANGE_KIND_OUT" \
        "run_output"

    set +e
    (cd "$ROOT_DIR" && "$LLVM_NEG_BIN" --self-test-invalid-obsolete-migration 2>"$LLVM_INVALID_OBSOLETE_MIGRATION_ERR" | pgy_selfhost_normalize_text_artifact >"$LLVM_INVALID_OBSOLETE_MIGRATION_OUT")
    LLVM_INVALID_OBSOLETE_MIGRATION_RC=$?
    set -e
    if [[ "$LLVM_INVALID_OBSOLETE_MIGRATION_RC" -ne 1 ]]; then
        echo "[self-host-parity:compatibility-corpus] invalid-obsolete-migration LLVM self-test should fail closed (rc=1), got rc=$LLVM_INVALID_OBSOLETE_MIGRATION_RC" >&2
        cat "$LLVM_INVALID_OBSOLETE_MIGRATION_OUT" "$LLVM_INVALID_OBSOLETE_MIGRATION_ERR" >&2
        exit 1
    fi
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "self-host-parity:compatibility-corpus" \
        "$BUILD_DIR" \
        "$INVALID_OBSOLETE_MIGRATION_EXPECTED_FILE" \
        "$LLVM_INVALID_OBSOLETE_MIGRATION_OUT" \
        "run_output"

    set +e
    (cd "$ROOT_DIR" && "$LLVM_NEG_BIN" --self-test-missing-surface 2>"$LLVM_MISSING_SURFACE_ERR" | pgy_selfhost_normalize_text_artifact >"$LLVM_MISSING_SURFACE_OUT")
    LLVM_MISSING_SURFACE_RC=$?
    set -e
    if [[ "$LLVM_MISSING_SURFACE_RC" -ne 1 ]]; then
        echo "[self-host-parity:compatibility-corpus] missing-surface LLVM self-test should fail closed (rc=1), got rc=$LLVM_MISSING_SURFACE_RC" >&2
        cat "$LLVM_MISSING_SURFACE_OUT" "$LLVM_MISSING_SURFACE_ERR" >&2
        exit 1
    fi
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "self-host-parity:compatibility-corpus" \
        "$BUILD_DIR" \
        "$MISSING_SURFACE_EXPECTED_FILE" \
        "$LLVM_MISSING_SURFACE_OUT" \
        "run_output"
fi

echo "[self-host-parity:compatibility-corpus] parity ok"
