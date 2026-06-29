#!/usr/bin/env bash
#
# self_host_pergyra_likeness_smoke.sh
#
# Ratchet gate: does the tracked self-host implementation move toward idiomatic
# Pergyra, or toward C-shaped string-munging that merely passes through the
# Pergyra parser?
#
# The hard-self-host LOC percentage answers "how much is substituted". It does
# not answer "is the substitute Pergyra-like". This gate is not the architecture
# proof; compiler-world contracts own intent/zone/resource structure. This gate
# owns only monotonic smell metrics that should fall as typed facts replace text
# bridges:
#
#   - string_munge_sig: `(...: String) -> String` signatures. Each one is a
#     text-in/text-out function: the C-compiler shape, not a typed transform.
#     This is the dominant un-Pergyra signal and the linchpin metric. Ratchet
#     down.
#   - ast_string_surface: `ast: String` parameters. The AST carried as
#     serialized text instead of a typed node tree is the root that forces
#     everything else into strings. Ratchet down toward a typed AST.
#   - sentinel: `return -1` / `== -1` / `!= -1`. Out-of-band error/not-found
#     signalling is hidden control flow that Pergyra's own Result/Option
#     surface is meant to replace. Ratchet down.
#   - result_use: Result/Option/Ok/Err/Some/None occurrences. Errors-as-data.
#     Ratchet up.
#
# Baselines are embedded below. The default measured scope is tracked,
# non-fixture `src/self_hosted/**/*.pgy` implementation source with `//`
# comments stripped. Untracked design sketches and committed fixtures are not
# part of this gate. A metric may exclude a named text-domain owner only when
# that owner owns text as its resource; this keeps the gate from punishing real
# responsibility separation such as JSON string escaping.
# When a metric improves, tighten the baseline in the same commit so the
# ratchet can only get stricter, matching the AIR-erasure / monotonic-decrease
# discipline used elsewhere in this repo.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SH_DIR="$ROOT_DIR/src/self_hosted"

# ---- ratchet baselines (tighten on improvement, never loosen) ----
STRING_MUNGE_SIG_MAX=161
AST_STRING_SURFACE_MAX=4
SENTINEL_MAX=34
RESULT_USE_MIN=45

fail() {
    echo "[self-host-likeness] FAIL" >&2
    echo "  - $*" >&2
    exit 1
}

count() {
    # count(pattern) -> matches across tracked self-host implementation code.
    # Fixtures, untracked sketches, and // comments are intentionally excluded
    # so this gate measures the implementation, not examples or notes.
    local pattern="$1"
    local exclude_re="${2:-}"
    local matches
    matches="$(
        git -C "$ROOT_DIR" ls-files src/self_hosted \
            | grep '\.pgy$' \
            | grep -Ev '/fixture(s)?/' \
            | {
                if [ -n "$exclude_re" ]; then
                    grep -Ev "$exclude_re"
                else
                    cat
                fi
            } \
            | while IFS= read -r rel; do
                [ -f "$ROOT_DIR/$rel" ] || continue
                sed 's://.*$::' "$ROOT_DIR/$rel"
            done \
            | grep -oE "$pattern" || true
    )"
    if [ -z "$matches" ]; then
        echo 0
    else
        printf '%s\n' "$matches" | wc -l | tr -d ' '
    fi
}

string_munge_sig=$(count ': String\) -> String' '^src/self_hosted/lib/json\.pgy$')
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
    fail "string_munge_sig rose to $string_munge_sig (> $STRING_MUNGE_SIG_MAX). New '(...: String) -> String' text-munging functions move the self-host compiler away from Pergyra. Carry a typed AST/IR node plus Result instead."
fi
if [ "$ast_string_surface" -gt "$AST_STRING_SURFACE_MAX" ]; then
    fail "ast_string_surface rose to $ast_string_surface (> $AST_STRING_SURFACE_MAX). The AST must move toward a typed node tree, not more 'ast: String' text surfaces."
fi
if [ "$sentinel" -gt "$SENTINEL_MAX" ]; then
    fail "sentinel rose to $sentinel (> $SENTINEL_MAX). '-1' out-of-band signalling is hidden control flow. Use Option/Result, already lowered by the self-host rungs."
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
    echo "[self-host-likeness] NOTE: a metric improved past its baseline; tighten the baselines in $0 in this commit so the ratchet stays strict."
fi

echo "[self-host-likeness] PASS"
