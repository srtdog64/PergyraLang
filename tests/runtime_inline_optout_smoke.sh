#!/usr/bin/env bash
# C-leg runtime linkage mode gate (docs/190 C3).
#
# The C backend links a separately compiled runtime object by default and
# re-inlines the whole runtime into the emitted TU when PGY_RUNTIME_INLINE=1.
# Neither half of that switch was exercised by anything: no test, Makefile
# target, or CI job ever set the opt-out, and nothing pinned extern as the
# default either. So the inline path could bit-rot unnoticed (it is the
# escape hatch you reach for precisely when you suspect an extern-mode bug),
# and a regression flipping the default back to inline would have passed every
# gate while silently giving up the compile-time win. This is the same
# config-asymmetry class as docs/189 C5-② (bc-ON never exercised).
#
# The two modes must agree observably, and each must actually BE the mode it
# claims: with a cold cache, extern mode has to produce the shared runtime
# object, and inline mode must not need it at all.
set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

LABEL="runtime-inline-optout"
FIXTURE="$ROOT_DIR/tests/cases/backend_compare/parallel_channel_sum/main.pgy"
[[ -f "$FIXTURE" ]] || { echo "[$LABEL] missing fixture: $FIXTURE" >&2; exit 1; }

OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

fail() { echo "[$LABEL] FAIL: $*" >&2; exit 1; }

# Where the driver caches the C-leg runtime object. Mirrors
# compiler_cext_object_path(): $TMP/pgy_runtime_cext_<profile>_<obs><ext>.
cache_tmp="${TMPDIR:-${TMP:-${TEMP:-/tmp}}}"
# On Windows the temp dir arrives as C:\Users\..., and a backslash is an escape
# character inside a bash glob -- the pattern would silently never match and the
# gate would report the driver had stopped producing the object at all.
cache_tmp="${cache_tmp//\\//}"
cext_glob="$cache_tmp/pgy_runtime_cext_"*

drop_cache() { rm -f $cext_glob 2>/dev/null || true; }
cache_present() { compgen -G "$cext_glob" >/dev/null 2>&1; }

build_and_run() { # $1=label $2=exe-name ; env already set by caller
    local what="$1" exe="$OUT_DIR/$2"
    local src out
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURE")"
    out="$(pgy_path_for_compiler "$PGY" "$exe")"
    (cd "$ROOT_DIR" && "$PGY" "$src" --backend=c -o "$out") \
        >"$exe.log" 2>&1 || fail "$what build failed: $(tail -3 "$exe.log")"
    "$exe" >"$exe.out" 2>&1 || fail "$what binary exited nonzero"
    tr -d '\r' <"$exe.out"
}

# --- inline mode (the opt-out): must build with no shared runtime object ---
# Export explicitly: `VAR=x some_function` leaks into the caller in bash.
drop_cache
export PGY_RUNTIME_INLINE=1
inline_out="$(build_and_run "inline" inline.exe)"
unset PGY_RUNTIME_INLINE
if cache_present; then
    fail "PGY_RUNTIME_INLINE=1 still produced the shared runtime object;\
 the opt-out no longer self-contains the emitted TU"
fi
echo "[$LABEL] PASS inline opt-out builds self-contained (no runtime object)"

# --- default mode: must be extern, i.e. it must produce the object ---
drop_cache
extern_out="$(build_and_run "extern-default" extern.exe)"
if ! cache_present; then
    fail "the default C-backend mode did not produce the shared runtime object;\
 extern is no longer the default (the compile-time win is silently gone)"
fi
echo "[$LABEL] PASS extern is the default (shared runtime object produced)"

# --- the two linkage modes must be observationally identical ---
if [[ "$inline_out" != "$extern_out" ]]; then
    printf '  inline: %s\n  extern: %s\n' "$inline_out" "$extern_out" >&2
    fail "inline and extern runtime linkage disagree on program output"
fi
echo "[$LABEL] PASS inline and extern linkage agree on output"

echo "[$LABEL] both C-leg runtime linkage modes work and agree; extern is default"
