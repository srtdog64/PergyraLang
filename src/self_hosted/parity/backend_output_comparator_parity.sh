#!/usr/bin/env bash
# Rung 2 parity for the backend output comparator (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/backend_output_comparator/main.pgy).
# Shell `diff -q` is the parity backend. Asserts:
#   - clean fixture (expected == actual): rc=0, JSON byte-equal vs
#     expected/clean.json
#   - synthetic mismatch fixture: rc=1, mismatch_lines >= 1, ok:false
#   - synthetic missing-input fixture: rc=1, input_error finding
# See src/self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:backend-output-comparator] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:backend-output-comparator] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/backend_output_comparator}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/expected/clean.json"
FIXTURE_EXPECTED="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/fixture/expected.txt"
FIXTURE_ACTUAL="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/fixture/actual.txt"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$FIXTURE_EXPECTED" "$FIXTURE_ACTUAL"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:backend-output-comparator] missing input: $path" >&2
        exit 1
    fi
done

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

# Phase 1 - clean (match) fixture.
set +e
CLEAN_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL" --run 2>/dev/null)"
CLEAN_RC=$?
set -e
if [[ "$CLEAN_RC" -ne 0 ]]; then
    echo "[self-host-parity:backend-output-comparator] clean fixture expected rc=0, got rc=$CLEAN_RC" >&2
    printf '%s\n' "$CLEAN_OUT" >&2
    exit 1
fi
CLEAN_JSON="$(printf '%s\n' "$CLEAN_OUT" \
    | grep -F 'pgy.selfhost.backend-output-comparator.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$CLEAN_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:backend-output-comparator] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $CLEAN_JSON" >&2
    exit 1
fi

# Shell drift detector for clean -- diff -q must agree. MSYS2 /
# Git-for-Windows checkouts can land text fixtures with CRLF endings
# depending on autocrlf and .gitattributes ordering (we saw
# --strip-trailing-cr still disagree in one Windows run, which means
# at least one byte is escaping that flag's normalization). The
# Pergyra tool already compares line-by-line after its own
# normalization, so we mirror that more aggressively here: strip all
# CRs (not just trailing-CR-per-line) from both files via tr before
# the shell diff. That matches what the Pergyra tool's
# line-iteration accepts.
CMP_TMP="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-bocparity.XXXXXX")"
trap_cmp_cleanup() { rm -rf "$CMP_TMP"; }
trap trap_cmp_cleanup EXIT
EXPECTED_NORM="$CMP_TMP/expected.norm"
ACTUAL_NORM="$CMP_TMP/actual.norm"
tr -d '\r' < "$FIXTURE_EXPECTED" > "$EXPECTED_NORM"
tr -d '\r' < "$FIXTURE_ACTUAL"   > "$ACTUAL_NORM"
# Capture rc explicitly. `diff -q` returns 0 (same), 1 (different),
# or 2 (error -- missing file, permission, etc.); the previous
# `if ! diff ...` form treated rc=2 the same as rc=1 and reported
# "disagrees with Pergyra" when in fact diff just couldn't run.
# In a recent ci-windows run the od dump showed both fixtures
# byte-identical (size=23, same chars) yet the diff arm still
# fired -- the only explanation is diff returning a non-{0,1} rc
# that the previous `!` form silently lumped into "different".
diff_rc=0
diff -q "$EXPECTED_NORM" "$ACTUAL_NORM" >/dev/null 2>&1 || diff_rc=$?
if [[ "$diff_rc" -eq 1 ]]; then
    echo "[self-host-parity:backend-output-comparator] shell diff -q disagrees with Pergyra (clean)" >&2
    # Surface the exact bytes that diverged so a future run has a
    # concrete trace instead of just the error line.
    {
        echo "--- expected (size=$(wc -c <"$FIXTURE_EXPECTED")) ---"
        od -c "$FIXTURE_EXPECTED" | head -5
        echo "--- actual   (size=$(wc -c <"$FIXTURE_ACTUAL")) ---"
        od -c "$FIXTURE_ACTUAL" | head -5
    } >&2
    exit 1
elif [[ "$diff_rc" -ne 0 ]]; then
    # diff itself errored out (rc=2: missing file / permission /
    # tool not found). Don't claim Pergyra is in disagreement --
    # log the rc + the normalized paths so the operator can see
    # which side the runtime is failing on.
    echo "[self-host-parity:backend-output-comparator] shell diff -q errored rc=$diff_rc (not a Pergyra disagreement)" >&2
    echo "  expected_norm=$EXPECTED_NORM (size=$(wc -c <"$EXPECTED_NORM" 2>/dev/null || echo '?'))" >&2
    echo "  actual_norm=$ACTUAL_NORM   (size=$(wc -c <"$ACTUAL_NORM" 2>/dev/null || echo '?'))" >&2
    exit 1
fi

# Phase 2 - synthetic mismatch fixture (different middle line in actual.txt).
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-cmp.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/self_hosted/tools/backend_output_comparator/fixture"
mkdir -p "$NEG_ROOT/.tmp"
cp "$FIXTURE_EXPECTED" "$NEG_ROOT/src/self_hosted/tools/backend_output_comparator/fixture/expected.txt"
sed 's/gamma/GAMMA-DRIFT/' "$FIXTURE_ACTUAL" \
    > "$NEG_ROOT/src/self_hosted/tools/backend_output_comparator/fixture/actual.txt"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:backend-output-comparator] mismatch fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:backend-output-comparator] mismatch fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Eq '"mismatch_lines":[1-9]' <<<"$NEG_OUT"; then
    echo "[self-host-parity:backend-output-comparator] mismatch fixture expected mismatch_lines >= 1" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"mismatch"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:backend-output-comparator] mismatch fixture expected a mismatch finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

# Phase 3 - synthetic missing-input fixture (delete actual.txt).
MISS_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-cmp-miss.XXXXXX")"
trap "rm -rf '$NEG_ROOT' '$MISS_ROOT'" EXIT
mkdir -p "$MISS_ROOT/src/self_hosted/tools/backend_output_comparator/fixture"
mkdir -p "$MISS_ROOT/.tmp"
cp "$FIXTURE_EXPECTED" "$MISS_ROOT/src/self_hosted/tools/backend_output_comparator/fixture/expected.txt"

set +e
MISS_OUT="$(cd "$MISS_ROOT" && "$PGY" "$PERGYRA_TOOL" --run 2>&1)"
MISS_RC=$?
set -e
if [[ "$MISS_RC" -ne 1 ]]; then
    echo "[self-host-parity:backend-output-comparator] missing-input fixture expected rc=1, got rc=$MISS_RC" >&2
    printf '%s\n' "$MISS_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"input_error"' <<<"$MISS_OUT"; then
    echo "[self-host-parity:backend-output-comparator] missing-input fixture expected input_error finding" >&2
    printf '%s\n' "$MISS_OUT" >&2
    exit 1
fi

echo "[self-host-parity:backend-output-comparator] rung-2 parity ok (clean rc=0; mismatch rc=1; missing-input rc=1)"
