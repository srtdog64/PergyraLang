#!/usr/bin/env bash
# Native String-copy lifetime: independent output, real release and escape control.
# Requires Linux Clang sanitizers; a missing compiler or sanitizer is a failure.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
CC="${CC:-clang}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
export PGY_NATIVE_PIPELINE=1
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export LSAN_OPTIONS=exitcode=23
export UBSAN_OPTIONS=halt_on_error=1
unset LSAN_SUPPRESSIONS
flags=(-O1 -g -fwrapv -fno-strict-aliasing -I"$ROOT_DIR/src/runtime" -I"$ROOT_DIR/src")
fixture="$ROOT_DIR/tests/cases/backend_compare/region_string_clone/main.pgy"
escape="$ROOT_DIR/tests/cases/backend_compare/region_string_clone_escape/main.pgy"
for ((i=0; i<100; i++)); do printf 'copy-'; done >"$WORK/expected"
printf '150\n' >>"$WORK/expected"
printf 'retained' >"$WORK/escape.expected"

# Do not short-circuit the second backend when the first detects a defect.
failures=0
for backend in c llvm; do
    ext=c
    sanitizer=address,undefined
    runtime=()
    if [[ "$backend" == llvm ]]; then
        ext=ll
        # LSan instruments the runtime allocations, not arbitrary emitted IR loads.
        sanitizer=leak
        runtime=(-DPGY_LLVM_ENABLED "$ROOT_DIR/src/runtime/pgy_runtime_lib.c")
    fi
    "$PGY_BIN" "$fixture" --native-pipeline "--emit-$backend" -o "$WORK/copy.$ext"
    "$CC" "${flags[@]}" -fsanitize="$sanitizer" -fno-sanitize-recover=all \
        "$WORK/copy.$ext" "${runtime[@]}" -pthread -lm -o "$WORK/copy-$backend"
    if "$WORK/copy-$backend" >"$WORK/$backend.out" 2>"$WORK/$backend.err"; then
        if cmp "$WORK/expected" "$WORK/$backend.out"; then
            echo "[string-copy-release] $backend: output and leak check PASS"
        else
            failures=$((failures + 1))
        fi
    else
        cat "$WORK/$backend.err" >&2
        failures=$((failures + 1))
    fi

    # Falsify physical reclamation, not just byte-equal output: removing the
    # emitted cleanup must make this same source program fail leak detection.
    if [[ "$backend" == c ]]; then
        sed '/^[[:space:]]*pgy_region_destroy(/d' "$WORK/copy.$ext" >"$WORK/no-release.$ext"
    else
        sed '/^[[:space:]]*call void @pgy_region_destroy_export(/d' "$WORK/copy.$ext" >"$WORK/no-release.$ext"
    fi
    if cmp -s "$WORK/copy.$ext" "$WORK/no-release.$ext"; then
        echo "[string-copy-release] $backend: no physical cleanup to falsify" >&2
        failures=$((failures + 1))
    else
        "$CC" "${flags[@]}" -fsanitize="$sanitizer" -fno-sanitize-recover=all \
            "$WORK/no-release.$ext" "${runtime[@]}" -pthread -lm -o "$WORK/no-release-$backend"
        if "$WORK/no-release-$backend" >"$WORK/mutant.out" 2>"$WORK/mutant.err"; then
            echo "[string-copy-release] $backend: deleted cleanup escaped the detector" >&2
            failures=$((failures + 1))
        elif grep -Eq '(AddressSanitizer|LeakSanitizer): .*leaked in' "$WORK/mutant.err"; then
            echo "[string-copy-release] $backend: cleanup-deletion mutation detected"
        else
            cat "$WORK/mutant.err" >&2
            failures=$((failures + 1))
        fi
    fi

    # Escaping copies must remain valid after their producer returns. They are
    # deliberately outside the leak-free claim, so run this control separately.
    "$PGY_BIN" "$escape" --native-pipeline "--emit-$backend" -o "$WORK/escape.$ext"
    "$CC" "${flags[@]}" "$WORK/escape.$ext" "${runtime[@]}" \
        -pthread -lm -o "$WORK/escape-$backend"
    "$WORK/escape-$backend" >"$WORK/escape-$backend.out"
    cmp "$WORK/escape.expected" "$WORK/escape-$backend.out"
    if [[ "$backend" == c ]]; then
        body="$(awk '/^char\* pgy_u_RetainedCopy\(/,/^}/' "$WORK/escape.c")"
    else
        body="$(awk '/^define .*@RetainedCopy\(/,/^}/' "$WORK/escape.ll")"
    fi
    if [[ -z "$body" ]] || ! grep -Fq 'StringConcat' <<<"$body" ||
        grep -Eq '(pgy_region_string_concat|pgy_region_destroy)' <<<"$body"; then
        echo "[string-copy-release] $backend: escaping result lost its heap lifetime" >&2
        failures=$((failures + 1))
    fi
done
if ((failures != 0)); then
    echo "[string-copy-release] FAIL: $failures checks" >&2
    exit 1
fi
echo '[string-copy-release] PASS: normal/early exits release copies; returned copy stays outside region'
