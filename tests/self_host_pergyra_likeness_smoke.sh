#!/usr/bin/env bash
#
# self_host_pergyra_likeness_smoke.sh
#
# Ratchet gate: is the self-hosted compiler itself written in idiomatic Pergyra,
# or in C-shaped string-munging that merely passes through the Pergyra parser?
#
# The hard-self-host LOC percentage answers "how much is substituted". It does
# NOT answer "is the substitute Pergyra-like". This gate answers the second
# question and forces it to improve monotonically:
#
#   - string_munge_sig: `(...: String) -> String` signatures. Each one is a
#     text-in/text-out function — the C-compiler shape, not a typed transform.
#     This is the dominant un-Pergyra signal and the linchpin metric. RATCHET DOWN.
#   - ast_string_surface: `ast: String` parameters. The AST carried as serialized
#     text instead of a typed node tree — the root that forces everything else
#     into strings. RATCHET DOWN (toward a typed AST).
#   - sentinel: `return -1` / `== -1` / `!= -1`. Out-of-band error/not-found
#     signalling — a hidden-control-flow path (CLAUDE.md §1.1) that Pergyra's own
#     Result/Option (already lowered by the self-host rungs) is meant to replace.
#     RATCHET DOWN.
#   - result_use: Result/Option/Ok/Err/Some/None occurrences. Errors-as-data.
#     RATCHET UP.
#
# Baselines are embedded below. When you improve a metric, TIGHTEN the baseline
# in the same commit (lower a max, raise the min) so the ratchet can only get
# stricter — exactly the AIR-erasure / monotonic-decrease discipline used
# elsewhere in this repo.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SH_DIR="$ROOT_DIR/src/self_hosted"

# ---- ratchet baselines (tighten on improvement, never loosen) ----
STRING_MUNGE_SIG_MAX=168
AST_STRING_SURFACE_MAX=5
SENTINEL_MAX=42
RESULT_USE_MIN=71

fail() {
    echo "[self-host-likeness] FAIL" >&2
    echo "  - $*" >&2
    exit 1
}

count() {
    # count(pattern) -> number of matches across self-host .pgy sources
    grep -rhoE "$1" "$SH_DIR" --include='*.pgy' 2>/dev/null | wc -l | tr -d ' '
}

string_munge_sig=$(count ': String\) -> String')
ast_string_surface=$(count '\bast: String\b')
sentinel=$(count 'return -1|== -1|!= -1')
result_use=$(count '\bResult<|\bOption<|\bOk\(|\bErr\(|\bSome\(|\bNone\b')

echo "[self-host-likeness] metrics (current vs baseline):"
echo "  string_munge_sig   : $string_munge_sig  (max $STRING_MUNGE_SIG_MAX)   <- text->text functions; linchpin"
echo "  ast_string_surface : $ast_string_surface  (max $AST_STRING_SURFACE_MAX)     <- AST carried as text"
echo "  sentinel           : $sentinel  (max $SENTINEL_MAX)    <- out-of-band error/not-found"
echo "  result_use         : $result_use  (min $RESULT_USE_MIN)    <- errors-as-data"

# ---- bad metrics: current must not exceed baseline ----
if [ "$string_munge_sig" -gt "$STRING_MUNGE_SIG_MAX" ]; then
    fail "string_munge_sig rose to $string_munge_sig (> $STRING_MUNGE_SIG_MAX). New '(...: String) -> String' text-munging functions move the self-host compiler AWAY from Pergyra. Carry a typed AST/IR node + Result instead."
fi
if [ "$ast_string_surface" -gt "$AST_STRING_SURFACE_MAX" ]; then
    fail "ast_string_surface rose to $ast_string_surface (> $AST_STRING_SURFACE_MAX). The AST must move toward a typed node tree, not more 'ast: String' text surfaces."
fi
if [ "$sentinel" -gt "$SENTINEL_MAX" ]; then
    fail "sentinel rose to $sentinel (> $SENTINEL_MAX). '-1' out-of-band signalling is hidden control flow (CLAUDE.md §1.1). Use Option/Result — already lowered by the self-host rungs."
fi

# ---- good metric: current must not fall below baseline ----
if [ "$result_use" -lt "$RESULT_USE_MIN" ]; then
    fail "result_use fell to $result_use (< $RESULT_USE_MIN). The self-host compiler must not shed errors-as-data idioms."
fi

# ---- improvement nudges (non-fatal): tell the author to tighten the ratchet ----
if [ "$string_munge_sig" -lt "$STRING_MUNGE_SIG_MAX" ] \
    || [ "$ast_string_surface" -lt "$AST_STRING_SURFACE_MAX" ] \
    || [ "$sentinel" -lt "$SENTINEL_MAX" ] \
    || [ "$result_use" -gt "$RESULT_USE_MIN" ]; then
    echo "[self-host-likeness] NOTE: a metric improved past its baseline — tighten the baselines in $0 in this commit so the ratchet stays strict."
fi

echo "[self-host-likeness] PASS"
