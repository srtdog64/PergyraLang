#!/usr/bin/env bash
# Valid source -> one admitted MIR -> C/LLVM; no native compilation fallback.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
pgy_prepend_windows_runtime_paths
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here effect-builtin-execution "$DRIVER"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/effect-builtin-execution.XXXXXX)"
echo "[effect-builtin-execution] evidence: $WORK"
sha256sum "$DRIVER" >"$WORK/inputs.sha256"
timeout 60 clang "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" -Isrc -Isrc/runtime \
    -c src/runtime/pgy_runtime_lib.c -o "$WORK/runtime.o" >"$WORK/runtime.compile.log" 2>&1
checks=0
while IFS='|' read -r name expected; do
    source="tests/concept_semantics/authority_effect/$name.pgy"
    sha256sum "$source" >>"$WORK/inputs.sha256"
    printf '%s\n' "$expected" | tr ',' '\n' >"$WORK/$name.expected"
    timeout 30 "$DRIVER" --emit-mir-json-verified "$source" -o "$WORK/$name.json" >"$WORK/$name.producer.log" 2>&1
    if [[ "$name" == effect_array_transform_valid ]]; then
        # Source graphs carry no selected target-library identity. Inject a
        # forged one separately for each new receiver type, then require both
        # consumers to refuse without publishing an artifact.
        python3 - "$WORK/$name.json" "$WORK" <<'PY'
import copy
import json
from pathlib import Path
import sys

original = json.loads(Path(sys.argv[1]).read_text())
for receiver in ('words', 'flags'):
    invalid = copy.deepcopy(original)
    changed = 0
    for routine in invalid['routines']:
        for block in routine['blocks']:
            for instruction in block['instructions']:
                for key in ('expr0_graph', 'expr1_graph'):
                    nodes = (instruction.get(key) or {}).get('nodes', [])
                    if not any(node.get('kind') == 'leaf' and node.get('text') == receiver for node in nodes):
                        continue
                    for node in nodes:
                        if node.get('kind') == 'call' and node.get('call_target_name') == 'ArrayReverse':
                            node['runtime_call_abi_id'] = 1
                            changed += 1
    assert changed > 0, receiver
    Path(sys.argv[2], receiver + '.forged.json').write_text(json.dumps(invalid))
PY
        for receiver in words flags; do
            for backend in c llvm; do
                rejected="$WORK/$receiver.forged.$backend"
                if timeout 30 "$DRIVER" "--mir-json-backend=$backend" "$WORK/$receiver.forged.json" \
                    -o "$rejected.artifact" >"$rejected.log" 2>&1; then
                    echo "[effect-builtin-execution] forged $receiver/$backend identity was admitted" >&2
                    exit 1
                fi
                [[ ! -e "$rejected.artifact" ]]
                grep -Fq 'expression admission is invalid: stage=builtin-call' "$rejected.log"
                echo "[effect-builtin-execution] PASS refused forged $receiver/$backend identity"
            done
        done
    fi
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
effect_array_transform_valid|1,3,3,1,1,0,9,4,3,2,1,1,3,2,1,0,6,false,false,false,true,false,false,true,0,true,4,3,2,3
effect_array_transform_lifetime_valid|3,3,false,true,3,1,9,true,0
CASES
retained=direct_mir_array_transform_retained_int_bool
retained_source="tests/self_hosted/fixtures/$retained.pgy"
sha256sum "$retained_source" >>"$WORK/inputs.sha256"
printf '3\nfalse\n' >"$WORK/$retained.expected"
timeout 30 "$DRIVER" --emit-mir-json-verified "$retained_source" \
    -o "$WORK/$retained.json" >"$WORK/$retained.producer.log" 2>&1
for backend in c llvm; do
    suffix=c; [[ "$backend" == llvm ]] && suffix=ll
    timeout 30 "$DRIVER" "--mir-json-backend=$backend" "$WORK/$retained.json" \
        -o "$WORK/$retained.$backend.$suffix" >"$WORK/$retained.$backend.project.log" 2>&1
done
if [[ "$(uname -s)" == Linux ]]; then
    python3 tests/self_hosted/parity/domain/effect_array_transform_heap_owner.py \
        "$ROOT_DIR" "$WORK" effect_array_transform_lifetime_valid "$retained"
fi
cat >"$WORK/reverse_binding.pgy" <<'SOURCE'
func Main() -> Void {
    var values: Array<String> = ["aa", "b"];
    let alias: Array<String> = ArrayReverse(values);
    Log(StringLength(values[0]) + StringLength(alias[1]));
}
SOURCE
cat >"$WORK/reverse_constructor.pgy" <<'SOURCE'
struct Box { items: Array<String>; }
func Main() -> Void {
    var values: Array<String> = ["aa", "b"];
    let box: Box = Box(ArrayReverse(values));
    Log(StringLength(values[0]) + StringLength(box.items[1]));
}
SOURCE
cat >"$WORK/reverse_call.pgy" <<'SOURCE'
func Keep(values: Array<String>) -> Array<String> { return values; }
func Main() -> Void {
    var values: Array<String> = ["aa", "b"];
    let alias: Array<String> = Keep(ArrayReverse(values));
    Log(StringLength(values[0]) + StringLength(alias[1]));
}
SOURCE
cat >"$WORK/reverse_return.pgy" <<'SOURCE'
func Reversed() -> Array<String> {
    var values: Array<String> = [];
    ArrayPushOwnedString(values, "aa"); ArrayPushOwnedString(values, "b");
    return ArrayReverse(values);
}
func Main() -> Void {
    let values: Array<String> = Reversed(); Log(StringLength(values[0]));
}
SOURCE
for name in binding constructor call return; do
    stem="$WORK/reverse_$name"
    timeout 30 "$DRIVER" --emit-mir-json-verified "$stem.pgy" -o "$stem.json" >"$stem.producer.log" 2>&1
    for backend in c llvm; do
        if timeout 30 "$DRIVER" "--mir-json-backend=$backend" "$stem.json" \
            -o "$stem.$backend.artifact" >"$stem.$backend.log" 2>&1; then
            echo "[effect-builtin-execution] unproved retaining $name/$backend alias was admitted" >&2
            exit 1
        fi
        [[ ! -e "$stem.$backend.artifact" ]]
        grep -Fq 'CODEGEN ERROR: direct MIR Array<String>' "$stem.$backend.log"
        echo "[effect-builtin-execution] PASS refused retaining $name/$backend alias"
    done
done
echo "[effect-builtin-execution] $checks direct-MIR executions PASS"
