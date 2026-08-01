#!/usr/bin/env bash
# Live self-host progress inventory. LOC is implementation volume, never a
# replacement percentage; replacement requires an executable production path.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROGRESS="$ROOT_DIR/src/self_hosted/PROGRESS.md"
README="$ROOT_DIR/docs/self_hosted/README.md"

fail() { echo "[self-host-progress] FAIL: $*" >&2; exit 1; }

count_lines() {
    find "$@" -type f -name '*.pgy' -print0 | xargs -0 cat | wc -l | tr -d ' '
}

count_c_lines() {
    find "$@" -type f \( -name '*.c' -o -name '*.h' \) -print0 |
        xargs -0 cat | wc -l | tr -d ' '
}

pgy_frontend_backend_loc="$(count_lines \
    "$ROOT_DIR/src/self_hosted/lexer" \
    "$ROOT_DIR/src/self_hosted/parser" \
    "$ROOT_DIR/src/self_hosted/semantic" \
    "$ROOT_DIR/src/self_hosted/codegen")"
pgy_compiler_core_loc="$(count_lines \
    "$ROOT_DIR/src/self_hosted/lexer" \
    "$ROOT_DIR/src/self_hosted/parser" \
    "$ROOT_DIR/src/self_hosted/semantic" \
    "$ROOT_DIR/src/self_hosted/codegen" \
    "$ROOT_DIR/src/self_hosted/hir" \
    "$ROOT_DIR/src/self_hosted/mir" \
    "$ROOT_DIR/src/self_hosted/mir_lower" \
    "$ROOT_DIR/src/self_hosted/compiler")"
c_reference_loc="$(count_c_lines \
    "$ROOT_DIR/src/lexer" "$ROOT_DIR/src/parser" \
    "$ROOT_DIR/src/semantic" "$ROOT_DIR/src/codegen" \
    "$ROOT_DIR/src/runtime" "$ROOT_DIR/src/compiler" "$ROOT_DIR/src/lsp")"

[ "$pgy_frontend_backend_loc" -gt 0 ] || fail "Pergyra implementation inventory is empty"
[ "$pgy_compiler_core_loc" -ge "$pgy_frontend_backend_loc" ] ||
    fail "compiler-core inventory is smaller than its frontend/backend subset"
[ "$c_reference_loc" -gt 0 ] || fail "C reference inventory is empty"

# Prose claims are checked against a whitespace-normalised copy. These are
# sentences inside wrapped Markdown, so a plain line-oriented grep also asserts
# where the paragraph happens to wrap: re-flowing a paragraph would drop the
# claim in the gate's eyes while the document still says it. That is exactly
# what happened -- "Explicit bounded replacement: DRV-2 is live" wrapped across
# two lines and this gate had been failing on it. Structural checks below
# (headings, table rows) stay line-oriented, because there the line IS the fact.
progress_flat="$(tr '\n' ' ' < "$PROGRESS" | tr -s '[:space:]' ' ')"

require_phrase() {
    case "$progress_flat" in
        *"$1"*) : ;;
        *) fail "$2" ;;
    esac
}

require_phrase "Implementation inventory is live-measured" \
    "PROGRESS lost the live implementation-inventory rule"
require_phrase "Classification is target-specific" \
    "PROGRESS must state the current target-specific replacement truth"
require_phrase "Explicit bounded replacement: DRV-2 is" \
    "PROGRESS lost the live bounded replacement claim"
grep -Fq "### Three-axis scorecard" "$PROGRESS" ||
    fail "PROGRESS lost the implementation/bounded/released scorecard"
grep -Fq "| Bounded executable replacement |" "$PROGRESS" ||
    fail "PROGRESS lost bounded executable replacement evidence"
grep -Fq '| Released/default replacement | pure-C artifact emit: `SUBSTITUTING`; plain compile/link, run, package, LLVM: `OPEN` |' "$PROGRESS" ||
    fail "PROGRESS blurred the promoted artifact target with open default targets"
grep -Fq "self-host-progress-metric-test-smoke" "$PROGRESS" ||
    fail "PROGRESS does not name this executable metric"
grep -Fq "implementation volume is not substitution" "$README" ||
    fail "self-host README conflates implementation volume with substitution"

if grep -Eq 'Compiler-internal substitution: *~?[0-9]' "$PROGRESS"; then
    fail "PROGRESS reintroduced a hand-maintained LOC substitution percentage"
fi
if grep -Eq 'records self-host compiler-internal substitution at *~?[0-9]' "$README"; then
    fail "self-host README reintroduced a stale substitution percentage"
fi

ratio="$(awk -v p="$pgy_frontend_backend_loc" -v c="$c_reference_loc" \
    'BEGIN { printf "%.2f", (100.0 * p) / c }')"
printf '[self-host-progress] implementation_frontend_backend_loc=%s compiler_core_loc=%s c_reference_loc=%s implementation_volume_ratio=%s%% default_c_emit=substituting full_default_compile=open explicit_drv2=live\n' \
    "$pgy_frontend_backend_loc" "$pgy_compiler_core_loc" "$c_reference_loc" "$ratio"
