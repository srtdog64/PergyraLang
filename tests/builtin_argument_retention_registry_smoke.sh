#!/usr/bin/env bash
# Canonical builtin argument-retention registry and generated self-host view.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
PYTHON_BIN="${PYTHON_BIN:-python3}"
REGISTRY="$ROOT_DIR/src/semantic/builtin_argument_retention_registry.def"
PROJECTION="$ROOT_DIR/src/self_hosted/semantic/builtin_argument_retention_projection_owner.pgy"
NATIVE_OWNER="$ROOT_DIR/src/semantic/region_retention_summary.c"
TRANSFER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_owned_string_literal_transfer_owner.pgy"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

"$PYTHON_BIN" -B "$ROOT_DIR/scripts/render_builtin_argument_retention_registry.py" \
    "$REGISTRY" "$PROJECTION" --check

grep -Fq '#include "builtin_argument_retention_registry.def"' "$NATIVE_OWNER"
grep -Fq 'builtin_identity: String;' "$PROJECTION"
grep -Fq 'SemanticBuiltinArgumentRetentionIdentityForSourceName(' "$PROJECTION"
grep -Fq 'SemanticBuiltinArgumentRetentionSourceNameForIdentity(' "$PROJECTION"
grep -Fq 'SemanticBuiltinArgumentBorrowedForIdentity(' "$PROJECTION"
grep -Fq 'SemanticBuiltinArgumentRetentionProjectionReady()' "$TRANSFER_OWNER"
grep -Fq 'SemanticAstCollectionOwnershipBuiltinCall(' "$TRANSFER_OWNER"
grep -Fq 'SemanticBuiltinArgumentBorrowedForCall(' "$TRANSFER_OWNER"
if grep -Eq 'case \(uint32_t\)BUILTIN_(PRINT|LOG|COMPILER_ARTIFACT_WRITE)' "$NATIVE_OWNER"; then
    echo '[builtin-argument-retention] native switch fallback returned' >&2
    exit 1
fi

expect_registry_rejected() {
    local label="$1"
    local appended_row="$2"
    local malformed_registry="$TMP_DIR/$label.def"
    cp "$REGISTRY" "$malformed_registry"
    printf '\n%s\n' "$appended_row" >> "$malformed_registry"
    if "$PYTHON_BIN" -B "$ROOT_DIR/scripts/render_builtin_argument_retention_registry.py" \
        "$malformed_registry" "$TMP_DIR/$label.pgy" >/dev/null 2>&1; then
        echo "[builtin-argument-retention] malformed registry was accepted: $label" >&2
        exit 1
    fi
}

expect_registry_rejected missing-close \
    'PGY_BUILTIN_ARGUMENT_RETENTION(WRITE, "Write", 0, PGY_REGION_RETENTION_BORROWED_FOR_CALL'
expect_registry_rejected parenthesized-ordinal \
    'PGY_BUILTIN_ARGUMENT_RETENTION(WRITE, "Write", (0), PGY_REGION_RETENTION_BORROWED_FOR_CALL)'
expect_registry_rejected duplicate-identity \
    'PGY_BUILTIN_ARGUMENT_RETENTION(PRINT, "OtherPrint", 0, PGY_REGION_RETENTION_BORROWED_FOR_CALL)'
expect_registry_rejected duplicate-source-name \
    'PGY_BUILTIN_ARGUMENT_RETENTION(OTHER_PRINT, "Print", 0, PGY_REGION_RETENTION_BORROWED_FOR_CALL)'

echo 'builtin argument retention registry smoke: ok'
