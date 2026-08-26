#!/usr/bin/env bash
set -uo pipefail

# WO-SEC-2: does the compiler leak, use freed memory, or execute UB while
# compiling? That question had no answer, and an unanswered safety question is
# not a pass -- it is an unmeasured one. The red-team review counted mallocs
# and called it "high risk"; counting proves nothing either way. This runs the
# real compiler, built with ASan+UBSan, over real sources, and reports what
# actually happens.
#
# Driven by `make test-asan`, which builds $PGY_ASAN_BIN first. The gate asks
# about the COMPILER, not the unit-test harnesses -- several of those never
# destroy the ASTs they build, and their tidiness is not a safety property of
# the compiler.
#
# First run (2026-07-14) found three real leaks, all of which are now fixed:
#   - ast_destroy had no AST_ARRAY_LITERAL case, so every array literal leaked
#     its whole element subtree (the meta-gate in ast_destroy_coverage_smoke.sh
#     keeps that class shut).
#   - Types were never freed by anyone: unbounded growth in pgy-lsp, which
#     re-analyzes on every keystroke.
#   - A scratch RIRScope released only its ops, leaking the resource facts the
#     walk recorded, and a namespace shell teardown skipped origin_path.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_ASAN_BIN:-$ROOT_DIR/bin-asan/pgy}"

if [[ ! -x "$PGY" ]]; then
    echo "[sanitizer-compile] no sanitized compiler at $PGY" >&2
    echo "  Build one with: make test-asan" >&2
    echo "  (MinGW ships no libasan; use Linux, WSL, or a clang with the" >&2
    echo "   sanitizer runtime.)" >&2
    exit 1
fi

# halt_on_error keeps a run from reporting one fault and sailing on; a gate
# that continues past a use-after-free is not reporting on the same program.
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:strict_string_checks=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

SOURCES=()
while IFS= read -r case_main; do
    SOURCES+=("$case_main")
done < <(find "$ROOT_DIR/tests/cases/backend_compare" -mindepth 2 -maxdepth 2 \
             -name main.pgy | LC_ALL=C sort | head -n "${PGY_ASAN_CASES:-40}")

# Only committed fixtures. The gate used to also compile the largest
# self-hosted sources (driver_rung2_owner.pgy et al.) for their density, but
# those are edited live by a parallel workstream: an unfinished intermediate
# once sent pgy into an infinite loop, and a gate that reads another session's
# in-progress files is flaky by construction. The backend_compare corpus is
# committed and stable, and exercises the same allocators.

if (( ${#SOURCES[@]} == 0 )); then
    echo "[sanitizer-compile] found no sources to compile" >&2
    echo "  A gate that checks nothing passes vacuously; that is a failure." >&2
    exit 1
fi

log_dir="$ROOT_DIR/.tmp/sanitizer-compile"
mkdir -p "$log_dir"
failed=()
checked=0
# Per-source ceiling: a sanitized compile of a fixture is seconds, so a run
# that blows past this is a hang (a real compiler bug on that input), and the
# gate must report it rather than wedge -- an unbounded gate that never
# returns is worse than a failing one.
timeout_s="${PGY_ASAN_TIMEOUT:-120}"
have_timeout=0
command -v timeout >/dev/null 2>&1 && have_timeout=1

for src in "${SOURCES[@]}"; do
    name="$(basename "$(dirname "$src")")_$(basename "$src" .pgy)"
    log="$log_dir/$name.log"
    # --mir stops before the backend: the question here is the compiler's own
    # memory behaviour, not the C compiler it would shell out to. A source that
    # does not typecheck standalone still exercises parse+semantic, and its
    # nonzero exit is not a sanitizer finding -- only a report is.
    if (( have_timeout )); then
        timeout "$timeout_s" "$PGY" "$src" --native-pipeline --mir > /dev/null 2> "$log"
        rc=$?
    else
        "$PGY" "$src" --native-pipeline --mir > /dev/null 2> "$log"
        rc=$?
    fi
    checked=$((checked + 1))
    if (( rc == 124 )); then
        failed+=("$src")
        echo "[sanitizer-compile] HANG in $src (exceeded ${timeout_s}s -- pgy did not terminate)" >&2
    elif grep -qE 'ERROR: (Address|Leak)Sanitizer|runtime error:|SUMMARY: (Address|Undefined)' "$log"; then
        failed+=("$src")
        echo "[sanitizer-compile] FINDING in $src" >&2
        grep -E 'ERROR:|SUMMARY:|runtime error:' "$log" | head -3 >&2
    fi
done

if (( ${#failed[@]} > 0 )); then
    echo >&2
    echo "[sanitizer-compile] ${#failed[@]}/$checked sources produced a sanitizer report." >&2
    echo "Full logs under .tmp/sanitizer-compile/." >&2
    exit 1
fi

echo "[sanitizer-compile] $checked sources compiled clean under ASan+UBSan"
