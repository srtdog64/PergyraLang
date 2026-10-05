#!/usr/bin/env bash
# Current-source producer and projection owners, using native-built scaffolds.
# Successful inputs execute; rejected source/MIR inputs never execute.
# This is not installed-driver, bootstrap or public-CLI publication evidence.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here inout-array-storage-mir "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/inout-array-storage-mir.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[inout-array-storage-mir] $*; evidence=$REL" >&2; exit 1; }
for probe in source_mir mir_projection; do
    "$PGY" --native-pipeline --opt=dev --backend=c \
        "tests/self_hosted/fixtures/inout_array_storage_${probe}_probe.pgy" \
        -o "$WORK/$probe.exe" >"$WORK/$probe.compile.log" 2>&1 || fail "$probe scaffold did not compile"
done
"$PGY" --native-pipeline --machine-manifest-json \
    >"$WORK/machine.json" 2>"$WORK/machine.err" || fail "machine declaration owner refused"
pgy_selfhost_select_linked_runtime_compile_profile || fail "runtime profile is invalid"
"${PGY_SELFHOST_CLANG:-clang}" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" \
    "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -c src/runtime/pgy_runtime_lib.c \
    -o "$WORK/runtime.o" >"$WORK/runtime.compile.log" 2>&1 || fail "runtime object did not compile"
for name in scalar own own_bool own_pair inout_write; do
    "$WORK/source_mir.exe" "tests/concept_semantics/hashmap/public_array_drop_$name.pgy" \
        "$REL/machine.json" >"$WORK/$name.mir.raw" 2>"$WORK/$name.produce.err" || fail "$name source producer refused"
    tr -d '\r' <"$WORK/$name.mir.raw" >"$WORK/$name.mir.json"
    expected=5
    [[ "$name" != scalar ]] || expected=$'7\ntrue'
    [[ "$name" != own_bool ]] || expected=true
    [[ "$name" != own_pair ]] || expected=$'3\n4'
    [[ "$name" != inout_write ]] || expected=7
    mir_hash="$(sha256sum "$WORK/$name.mir.json" | cut -d' ' -f1)"
    for backend in c llvm; do
        suffix=c
        [[ "$backend" != llvm ]] || suffix=ll
        artifact="$WORK/$name.$suffix"
        "$WORK/mir_projection.exe" "$REL/$name.mir.json" "$REL/machine.json" "$backend" \
            >"$artifact.raw" 2>"$WORK/$name-$backend.project.err" || fail "$name $backend projection refused"
        tr -d '\r' <"$artifact.raw" >"$artifact"
        compile=("${PGY_SELFHOST_CC:-gcc}" -x c -std=c11 -O0 -fwrapv -fno-strict-aliasing \
            "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread "$artifact" -lm)
        [[ "$backend" != llvm ]] || compile=("${PGY_SELFHOST_CLANG:-clang}" -x ir "$artifact" \
            -x none "$WORK/runtime.o" -pthread -lm)
        "${compile[@]}" -o "$WORK/$name-$backend.exe" >"$WORK/$name-$backend.compile.log" 2>&1 || fail "$name $backend artifact did not compile"
        "$WORK/$name-$backend.exe" >"$WORK/$name-$backend.run" || fail "$name $backend execution failed"
        [[ "$(tr -d '\r' <"$WORK/$name-$backend.run")" == "$expected" ]] || fail "$name $backend result differs"
        [[ "$(sha256sum "$WORK/$name.mir.json" | cut -d' ' -f1)" == "$mir_hash" ]] || fail "$name $backend mutated source MIR"
    done
done
for name in inout_alias_negative inout_nested_rebind_negative; do
    if "$WORK/source_mir.exe" "tests/concept_semantics/hashmap/public_array_drop_$name.pgy" \
        "$REL/machine.json" >"$WORK/$name.out" 2>"$WORK/$name.err"; then fail "source accepted $name"; fi
    grep -Eqi 'borrow_boundary_escape|BORROW_ESCAPE|ArrayDrop storage|ArrayDrop cannot release an aliased or escaped descriptor' \
        "$WORK/$name.out" "$WORK/$name.err" || fail "$name lost ownership diagnosis"
    ! grep -Eq '"schema"[[:space:]]*:[[:space:]]*"pgy\.mir\.' "$WORK/$name.out" || fail "$name published MIR before refusal"
done
"${PYTHON_BIN:-python3}" tests/self_hosted/parity/public_array_drop_mir_mutations.py \
    "$WORK/scalar.mir.json" "$WORK/own.mir.json" "$WORK/own_pair.mir.json" "$WORK"
for mutation in mir-double-drop mir-use-after-drop mir-borrowed-forward mir-use-after-transfer mir-duplicate-own-argument; do
    for backend in c llvm; do
        if "$WORK/mir_projection.exe" "$REL/$mutation.mir.json" "$REL/machine.json" "$backend" \
            >"$WORK/$mutation-$backend.out" 2>"$WORK/$mutation-$backend.err"; then fail "$backend accepted $mutation"; fi
        grep -Fq 'program_readiness=28' "$WORK/$mutation-$backend.out" "$WORK/$mutation-$backend.err" || fail "$mutation did not reach storage-lifetime refusal"
        ! grep -Eq '^#include|^define |^@\.pgy\.' "$WORK/$mutation-$backend.out" || fail "$mutation emitted before refusal"
    done
done
echo "[inout-array-storage-mir] PASS (current source-issued MIR: 5 executed positives per C/LLVM, 2 source refusals and 5 MIR refusals per backend; not installed-driver proof); evidence=$REL"
