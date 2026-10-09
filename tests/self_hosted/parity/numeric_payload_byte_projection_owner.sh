#!/usr/bin/env bash
# Execute the existing numeric admission owner, not a reconstructed graph policy.
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_process_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
LABEL=numeric-payload-byte-projection
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
B="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/numeric-payload-bytes.XXXXXX")"
fail() { echo "[$LABEL] $*; evidence=$B" >&2; exit 1; }
cd "$ROOT_DIR"
INPUT=tests/self_hosted/fixtures/numeric_payload_byte_projection_probe.pgy
OWNER=src/self_hosted/hir/ast_expression_graph_owner.pgy
# This source ratchet only forbids reintroducing the measured allocating path.
awk '/^func AstExpressionSignedDecimalWithin\(/ { active=1 }
     /^func AstExpressionNodeIsUnaryOperator\(/ { active=0 }
     active' "$OWNER" >"$B/predicate.source"
test -s "$B/predicate.source" || fail 'numeric predicate absent'
if grep -Eq 'CodegenCharAt\(|StringIndexOf\(' "$B/predicate.source"; then
    fail 'numeric admission restored temporary-character allocation'
fi
sha256sum "$PGY" "$INPUT" "$OWNER" >"$B/input.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$B/import.sha256"
printf 'numeric payload comparisons=78322\nNUMERIC PAYLOAD BYTE PROJECTION PASS\n' >"$B/expected"
for backend in c llvm; do
    pgy_run_with_timeout 120 "$B/$backend-build.stdout" "$B/$backend-build.stderr" \
        "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$INPUT" \
        -o "$B/$backend.exe" || fail "$backend probe did not build"
    pgy_run_with_timeout 30 "$B/$backend.raw" "$B/$backend.stderr" \
        "$B/$backend.exe" || fail "$backend numeric oracle failed"
    test ! -s "$B/$backend.stderr" || fail "$backend probe wrote stderr"
    tr -d '\r' <"$B/$backend.raw" >"$B/$backend.run"
    cmp "$B/expected" "$B/$backend.run" || fail "$backend exact numeric oracle drift"
done
sha256sum --quiet -c "$B/input.sha256"
sha256sum --quiet -c "$B/import.sha256"
sha256sum "$B"/*.exe >"$B/binaries.sha256"
echo "[$LABEL] C/LLVM: 78322 valid-extent comparisons, Int/Long range and malformed controls, arena topology and invalid-extent refusals PASS; full DRV-2/installed pair remain separate; evidence=$B"
