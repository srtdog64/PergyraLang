#!/usr/bin/env bash
# Canonical builtin argument-retention registry and generated self-host view.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
PYTHON_BIN="${PYTHON_BIN:-python3}"
REGISTRY="$ROOT_DIR/src/semantic/builtin_argument_retention_registry.def"
PROJECTION="$ROOT_DIR/src/self_hosted/semantic/builtin_argument_retention_projection_owner.pgy"
NATIVE_OWNER="$ROOT_DIR/src/semantic/region_retention_summary.c"
TRANSFER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_owned_string_literal_transfer_owner.pgy"
FACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_builtin_argument_retention_call_fact_owner.pgy"
IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_identity_fact_owner.pgy"
RESOLUTION_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy"
FORMAL_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_formal_use_owner.pgy"
EXCLUSIVITY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_owned_string_actual_exclusivity_owner.pgy"
SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/semantic/builtin_signature_owner.pgy"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

"$PYTHON_BIN" -B "$ROOT_DIR/scripts/render_builtin_argument_retention_registry.py" \
    "$REGISTRY" "$PROJECTION" --check

"$PYTHON_BIN" - "$REGISTRY" "$SIGNATURE_OWNER" <<'PY'
import re
import sys
from pathlib import Path

registry = Path(sys.argv[1]).read_text(encoding="utf-8")
owner = Path(sys.argv[2]).read_text(encoding="utf-8")
retention_names = set(re.findall(
    r'PGY_(?:BUILTIN|STDLIB)_ARGUMENT_RETENTION\([^,]+,\s*"([^"]+)"', registry))
signature_rows = {
    name: (returns, params)
    for name, returns, params in re.findall(
        r'^\s*"([^"^]+)\^([^"^]+)\^([^"]+)",?$', owner, re.MULTILINE)
}
expected = {name: signature_rows[name] for name in retention_names if name in signature_rows}
known_block = owner.split(
    "func SemanticBuiltinRetentionSourceSignatureKnown", 1)[1].split("func ", 1)[0]
match_block = owner.split(
    "func SemanticBuiltinRetentionSourceSignatureMatches", 1)[1].split("func ", 1)[0]
known = set(re.findall(r'name == "([^"]+)"', known_block))
matches = {
    name: (returns, params)
    for name, returns, params in re.findall(
        r'if name == "([^"]+)" \{ return return_type == "([^"]+)" && param_types == "([^"]+)"; \}',
        match_block)
}
if known != set(expected) or matches != expected:
    raise SystemExit("retention signature projection drifted from canonical builtin rows")
for name, arity in re.findall(
        r'PGY_STDLIB_ARGUMENT_RETENTION\([^,]+,\s*"([^"]+)",[^\n]+,\s*([0-9]+)\)', registry):
    if name not in expected or len(expected[name][1].split("|")) != int(arity):
        raise SystemExit("stdlib retention arity drifted from its canonical signature")
PY

grep -Fq '#include "builtin_argument_retention_registry.def"' "$NATIVE_OWNER"
grep -Fq 'builtin_identity: String;' "$PROJECTION"
grep -Fq 'SemanticBuiltinArgumentRetentionIdentityForSourceName(' "$PROJECTION"
grep -Fq 'SemanticBuiltinArgumentRetentionSourceNameForIdentity(' "$PROJECTION"
grep -Fq 'SemanticBuiltinArgumentBorrowedForIdentity(' "$PROJECTION"
grep -Fq 'builtin_argument_retention_calls:' "$IDENTITY_OWNER"
grep -Fq 'SemanticAstBuiltinArgumentRetentionRecordResolvedCall(' "$RESOLUTION_OWNER"
grep -Fq 'SemanticAstCollectionOwnershipBuiltinCall(' "$FACT_OWNER"
grep -Fq 'SemanticBuiltinRetentionSourceSignatureKnown(' "$SIGNATURE_OWNER"
grep -Fq 'SemanticBuiltinRetentionSourceSignatureMatches(' "$SIGNATURE_OWNER"
grep -Fq 'SemanticBuiltinRetentionSourceSignatureMatches(' "$FACT_OWNER"
if grep -Eq 'function_tables\.(returns|params)\[table_row\] == ""' "$FACT_OWNER"; then
    echo '[builtin-argument-retention] non-empty signature fallback remains' >&2
    exit 1
fi
grep -Fq 'import "ast_owned_string_actual_exclusivity_owner.pgy";' "$TRANSFER_OWNER"
grep -Fq 'SemanticAstOwnedStringLiteralTransferBorrowUseExclusive(' "$TRANSFER_OWNER"
grep -Fq 'SemanticAstOwnedStringLiteralTransferExpressionExclusive(' "$TRANSFER_OWNER"
grep -Fq 'SemanticAstBuiltinArgumentRetentionIdentityForReadyCallContext(' "$FORMAL_OWNER"
grep -Fq 'SemanticAstBuiltinArgumentRetentionIdentityForReadyCallFacts(' "$EXCLUSIVITY_OWNER"
for consumer in "$FORMAL_OWNER" "$EXCLUSIVITY_OWNER"; do
    if grep -Fq 'SemanticBuiltinArgumentBorrowedForCall(' "$consumer"; then
        echo "[builtin-argument-retention] source-name fallback remains: $consumer" >&2
        exit 1
    fi
done
if grep -Fq 'SemanticBuiltinArgumentBorrowedForCall(' "$TRANSFER_OWNER"; then
    echo "[builtin-argument-retention] transfer bypassed its shared exclusivity owner" >&2
    exit 1
fi
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
expect_registry_rejected invalid-stdlib-arity \
    'PGY_STDLIB_ARGUMENT_RETENTION(OTHER_SCAN, "OtherScan", 0, PGY_REGION_RETENTION_BORROWED_FOR_CALL, 0)'
expect_registry_rejected invalid-stdlib-ordinal \
    'PGY_STDLIB_ARGUMENT_RETENTION(OTHER_SCAN, "OtherScan", 3, PGY_REGION_RETENTION_BORROWED_FOR_CALL, 3)'

echo 'builtin argument retention registry smoke: ok'
