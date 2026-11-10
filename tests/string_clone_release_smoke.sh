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
local_cleanup="$ROOT_DIR/tests/cases/backend_compare/function_local_collection_cleanup/main.pgy"
owned_pop="$ROOT_DIR/tests/cases/backend_compare/owned_string_pop/main.pgy"
borrowed_pop="$ROOT_DIR/tests/cases/backend_compare/borrowed_string_pop/main.pgy"
mixed_owned_return="$ROOT_DIR/tests/cases/function_param_flow_summary/mixed_owned_static_string_result_valid.pgy"
loop_owned_literal="$ROOT_DIR/tests/cases/function_param_flow_summary/owned_string_array_literal_loop_recheck_valid.pgy"
for ((i=0; i<100; i++)); do printf 'copy-'; done >"$WORK/expected"
printf '150\n' >>"$WORK/expected"
printf 'retained' >"$WORK/escape.expected"
printf '1\n' >"$WORK/local-cleanup.expected"
printf '0\n' >"$WORK/owned-pop.expected"
printf '0\n' >"$WORK/borrowed-pop.expected"
printf 'owned\n\n' >"$WORK/mixed-owned-return.expected"
printf '3\n3\n' >"$WORK/loop-owned-literal.expected"

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

    # A private function-local Array<String> has the same physical contract:
    # MIR owns one cleanup on every return edge, and both backends must emit it.
    # This catches semantic admission that is not followed by actual storage
    # and owned-element retirement.
    "$PGY_BIN" "$local_cleanup" --native-pipeline "--emit-$backend" \
        -o "$WORK/local-cleanup.$ext"
    "$CC" "${flags[@]}" -fsanitize="$sanitizer" -fno-sanitize-recover=all \
        "$WORK/local-cleanup.$ext" "${runtime[@]}" -pthread -lm \
        -o "$WORK/local-cleanup-$backend"
    if "$WORK/local-cleanup-$backend" \
        >"$WORK/local-cleanup-$backend.out" \
        2>"$WORK/local-cleanup-$backend.err" &&
        cmp "$WORK/local-cleanup.expected" \
            "$WORK/local-cleanup-$backend.out"; then
        echo "[string-copy-release] $backend: function-local collection cleanup PASS"
    else
        cat "$WORK/local-cleanup-$backend.err" >&2
        failures=$((failures + 1))
    fi
    sed '/pgy_array_drop_owned_String/d' "$WORK/local-cleanup.$ext" \
        >"$WORK/local-cleanup-no-release.$ext"
    if cmp -s "$WORK/local-cleanup.$ext" \
        "$WORK/local-cleanup-no-release.$ext"; then
        echo "[string-copy-release] $backend: no local collection cleanup to falsify" >&2
        failures=$((failures + 1))
    else
        "$CC" "${flags[@]}" -fsanitize="$sanitizer" -fno-sanitize-recover=all \
            "$WORK/local-cleanup-no-release.$ext" "${runtime[@]}" \
            -pthread -lm -o "$WORK/local-cleanup-no-release-$backend"
        if "$WORK/local-cleanup-no-release-$backend" \
            >"$WORK/local-cleanup-mutant.out" \
            2>"$WORK/local-cleanup-mutant.err"; then
            echo "[string-copy-release] $backend: deleted local cleanup escaped the detector" >&2
            failures=$((failures + 1))
        elif grep -Eq '(AddressSanitizer|LeakSanitizer): .*leaked in' \
            "$WORK/local-cleanup-mutant.err"; then
            echo "[string-copy-release] $backend: local cleanup-deletion mutation detected"
        else
            cat "$WORK/local-cleanup-mutant.err" >&2
            failures=$((failures + 1))
        fi
    fi

    # Each iteration owns a nonempty literal of fresh String results. Its
    # descriptor and all payloads retire at the iteration boundary; an extra
    # copy of the final Fresh result must not abandon that original result.
    if "$PGY_BIN" "$loop_owned_literal" --native-pipeline "--emit-$backend" \
        -o "$WORK/loop-owned-literal.$ext" &&
        "$CC" "${flags[@]}" -fsanitize="$sanitizer" -fno-sanitize-recover=all \
            "$WORK/loop-owned-literal.$ext" "${runtime[@]}" -pthread -lm \
            -o "$WORK/loop-owned-literal-$backend"; then
        if "$WORK/loop-owned-literal-$backend" \
            >"$WORK/loop-owned-literal-$backend.out" \
            2>"$WORK/loop-owned-literal-$backend.err" &&
            cmp "$WORK/loop-owned-literal.expected" \
                "$WORK/loop-owned-literal-$backend.out"; then
            echo "[string-copy-release] $backend: owned literal loop cleanup PASS"
        else
            cat "$WORK/loop-owned-literal-$backend.err" >&2
            failures=$((failures + 1))
        fi
        sed '/pgy_array_drop_owned_String/d' "$WORK/loop-owned-literal.$ext" \
            >"$WORK/loop-owned-literal-no-release.$ext"
        if cmp -s "$WORK/loop-owned-literal.$ext" \
            "$WORK/loop-owned-literal-no-release.$ext"; then
            echo "[string-copy-release] $backend: no owned literal loop cleanup to falsify" >&2
            failures=$((failures + 1))
        elif "$CC" "${flags[@]}" -fsanitize="$sanitizer" -fno-sanitize-recover=all \
            "$WORK/loop-owned-literal-no-release.$ext" "${runtime[@]}" \
            -pthread -lm -o "$WORK/loop-owned-literal-no-release-$backend"; then
            if "$WORK/loop-owned-literal-no-release-$backend" \
                >"$WORK/loop-owned-literal-mutant.out" \
                2>"$WORK/loop-owned-literal-mutant.err"; then
                echo "[string-copy-release] $backend: deleted loop cleanup escaped the detector" >&2
                failures=$((failures + 1))
            elif grep -Eq '(AddressSanitizer|LeakSanitizer): .*leaked in' \
                "$WORK/loop-owned-literal-mutant.err"; then
                echo "[string-copy-release] $backend: loop cleanup-deletion mutation detected"
            else
                cat "$WORK/loop-owned-literal-mutant.err" >&2
                failures=$((failures + 1))
            fi
        else
            echo "[string-copy-release] $backend: deleted loop cleanup did not compile" >&2
            failures=$((failures + 1))
        fi
    else
        echo "[string-copy-release] $backend: owned literal loop emission/build failed" >&2
        failures=$((failures + 1))
    fi

    # A function with an already-owned return branch and a static-literal
    # branch copies only the literal at its producer. Both branches then have
    # one owned result contract, and the caller can retire either payload.
    "$PGY_BIN" "$mixed_owned_return" --native-pipeline "--emit-$backend" \
        -o "$WORK/mixed-owned-return.$ext"
    "$CC" "${flags[@]}" -fsanitize="$sanitizer" \
        -fno-sanitize-recover=all "$WORK/mixed-owned-return.$ext" \
        "${runtime[@]}" -pthread -lm -o "$WORK/mixed-owned-return-$backend"
    if "$WORK/mixed-owned-return-$backend" \
        >"$WORK/mixed-owned-return-$backend.out" \
        2>"$WORK/mixed-owned-return-$backend.err" &&
        cmp "$WORK/mixed-owned-return.expected" \
            "$WORK/mixed-owned-return-$backend.out"; then
        echo "[string-copy-release] $backend: mixed owned/static return cleanup PASS"
    else
        cat "$WORK/mixed-owned-return-$backend.err" >&2
        failures=$((failures + 1))
    fi

    # ArrayPop has two physical contracts. A borrowed String array only
    # forgets the descriptor entry; an owned String array must retire the
    # removed allocation before shortening the descriptor. The semantic
    # receipt, rather than the surface spelling, chooses the ABI.
    "$PGY_BIN" "$owned_pop" --native-pipeline "--emit-$backend" \
        -o "$WORK/owned-pop.$ext"
    "$CC" "${flags[@]}" -fsanitize="$sanitizer" \
        -fno-sanitize-recover=all "$WORK/owned-pop.$ext" \
        "${runtime[@]}" -pthread -lm -o "$WORK/owned-pop-$backend"
    if "$WORK/owned-pop-$backend" >"$WORK/owned-pop-$backend.out" \
        2>"$WORK/owned-pop-$backend.err" &&
        cmp "$WORK/owned-pop.expected" "$WORK/owned-pop-$backend.out"; then
        echo "[string-copy-release] $backend: owned ArrayPop PASS"
    else
        cat "$WORK/owned-pop-$backend.err" >&2
        failures=$((failures + 1))
    fi
    owned_pop_mutation_flags=()
    if [[ "$backend" == c ]]; then
        # The inline C runtime lets -O1 erase this deliberately unobserved
        # allocation after the shallow-pop mutation. Keep the falsifier at
        # -O0 so the deleted release remains an executable leak.
        owned_pop_mutation_flags=(-O0)
        sed '/pgy_array_pop_owned_String/s/pgy_array_pop_owned_String/pgy_array_pop_String/' \
            "$WORK/owned-pop.$ext" >"$WORK/owned-pop-shallow.$ext"
    else
        sed '/call void @pgy_array_pop_owned_String/s/@pgy_array_pop_owned_String/@pgy_array_pop_String/' \
            "$WORK/owned-pop.$ext" >"$WORK/owned-pop-shallow.$ext"
    fi
    if cmp -s "$WORK/owned-pop.$ext" "$WORK/owned-pop-shallow.$ext"; then
        echo "[string-copy-release] $backend: no owned ArrayPop ABI to falsify" >&2
        failures=$((failures + 1))
    else
        "$CC" "${flags[@]}" "${owned_pop_mutation_flags[@]}" \
            -fsanitize="$sanitizer" \
            -fno-sanitize-recover=all "$WORK/owned-pop-shallow.$ext" \
            "${runtime[@]}" -pthread -lm \
            -o "$WORK/owned-pop-shallow-$backend"
        if "$WORK/owned-pop-shallow-$backend" \
            >"$WORK/owned-pop-mutant.out" \
            2>"$WORK/owned-pop-mutant.err"; then
            echo "[string-copy-release] $backend: shallow owned pop escaped leak detection" >&2
            failures=$((failures + 1))
        elif grep -Eq '(AddressSanitizer|LeakSanitizer): .*leaked in' \
            "$WORK/owned-pop-mutant.err"; then
            echo "[string-copy-release] $backend: owned-pop mutation detected"
        else
            cat "$WORK/owned-pop-mutant.err" >&2
            failures=$((failures + 1))
        fi
    fi

    "$PGY_BIN" "$borrowed_pop" --native-pipeline "--emit-$backend" \
        -o "$WORK/borrowed-pop.$ext"
    "$CC" "${flags[@]}" -fsanitize=address,undefined \
        -fno-sanitize-recover=all "$WORK/borrowed-pop.$ext" \
        "${runtime[@]}" -pthread -lm -o "$WORK/borrowed-pop-$backend"
    if ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        "$WORK/borrowed-pop-$backend" \
        >"$WORK/borrowed-pop-$backend.out" \
        2>"$WORK/borrowed-pop-$backend.err" &&
        cmp "$WORK/borrowed-pop.expected" \
            "$WORK/borrowed-pop-$backend.out"; then
        echo "[string-copy-release] $backend: borrowed ArrayPop PASS"
    else
        cat "$WORK/borrowed-pop-$backend.err" >&2
        failures=$((failures + 1))
    fi
    if [[ "$backend" == c ]]; then
        sed '/pgy_array_pop_String/s/pgy_array_pop_String/pgy_array_pop_owned_String/' \
            "$WORK/borrowed-pop.$ext" >"$WORK/borrowed-pop-owned.$ext"
    else
        sed '/call void @pgy_array_pop_String/s/@pgy_array_pop_String/@pgy_array_pop_owned_String/' \
            "$WORK/borrowed-pop.$ext" >"$WORK/borrowed-pop-owned.$ext"
    fi
    if cmp -s "$WORK/borrowed-pop.$ext" "$WORK/borrowed-pop-owned.$ext"; then
        echo "[string-copy-release] $backend: no borrowed ArrayPop ABI to falsify" >&2
        failures=$((failures + 1))
    else
        "$CC" "${flags[@]}" -fsanitize=address,undefined \
            -fno-sanitize-recover=all "$WORK/borrowed-pop-owned.$ext" \
            "${runtime[@]}" -pthread -lm \
            -o "$WORK/borrowed-pop-owned-$backend"
        if ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
            "$WORK/borrowed-pop-owned-$backend" \
            >"$WORK/borrowed-pop-mutant.out" \
            2>"$WORK/borrowed-pop-mutant.err"; then
            echo "[string-copy-release] $backend: owned borrowed pop escaped invalid-free detection" >&2
            failures=$((failures + 1))
        elif grep -Eq '(AddressSanitizer|bad-free|invalid pointer|attempting free)' \
            "$WORK/borrowed-pop-mutant.err"; then
            echo "[string-copy-release] $backend: borrowed-pop mutation detected"
        else
            cat "$WORK/borrowed-pop-mutant.err" >&2
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
