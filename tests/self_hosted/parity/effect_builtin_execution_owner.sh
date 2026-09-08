#!/usr/bin/env bash
# Valid source -> one admitted MIR -> C/LLVM; no native compilation fallback.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here effect-builtin-execution "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/effect-builtin-execution.XXXXXX)"
echo "[effect-builtin-execution] evidence: $WORK"
sha256sum "$DRIVER" >"$WORK/inputs.sha256"
timeout 60 clang -std=c11 -DPGY_LLVM_ENABLED -Isrc -Isrc/runtime \
    -c src/runtime/pgy_runtime_lib.c -o "$WORK/runtime.o" >"$WORK/runtime.compile.log" 2>&1
checks=0
while IFS='|' read -r name expected; do
    source="tests/concept_semantics/authority_effect/$name.pgy"
    sha256sum "$source" >>"$WORK/inputs.sha256"
    printf '%s\n' "$expected" | tr ',' '\n' >"$WORK/$name.expected"
    timeout 30 "$DRIVER" --emit-mir-json-verified "$source" -o "$WORK/$name.json" >"$WORK/$name.producer.log" 2>&1
    for backend in c llvm; do
        stem="$WORK/$name.$backend"
        suffix=c; [[ "$backend" == llvm ]] && suffix=ll
        timeout 30 "$DRIVER" "--mir-json-backend=$backend" "$WORK/$name.json" \
            -o "$stem.$suffix" >"$stem.project.log" 2>&1
        if [[ "$backend" == c ]]; then
            timeout 30 gcc -x c -std=c11 -O0 -fwrapv -fno-strict-aliasing \
                -Isrc -Isrc/runtime "$stem.c" -pthread -lm -o "$stem.exe" >"$stem.compile.log" 2>&1
        else
            timeout 30 clang -x ir "$stem.ll" -x none "$WORK/runtime.o" \
                -pthread -lm -o "$stem.exe" >"$stem.compile.log" 2>&1
        fi
        timeout 10 "$stem.exe" >"$stem.raw" 2>"$stem.err"
        tr -d '\r' <"$stem.raw" >"$stem.actual"
        [[ ! -s "$stem.err" ]]
        cmp "$WORK/$name.expected" "$stem.actual"
        checks=$((checks + 1))
        echo "[effect-builtin-execution] PASS $name/$backend"
    done
done <<'CASES'
effect_scalar_composition_valid|4,7
effect_checked_arithmetic_valid|2147483647,2147395600,-40
effect_collection_local_valid|2,3
effect_array_transform_valid|1,3,3,1,1,0,9
CASES
echo "[effect-builtin-execution] $checks direct-MIR executions PASS"
