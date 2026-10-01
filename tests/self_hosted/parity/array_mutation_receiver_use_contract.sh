#!/usr/bin/env bash
# One receiver identity and reachable index facts admit the general C/LLVM route.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-array-mutation-receiver-use-contract
DRIVER="$(pgy_select_optional_exe_binary "$(pgy_path_for_bash_tool "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
command -v timeout >/dev/null || fail "bounded execution tool is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "Python is required for producer-owned receiver falsifiers"
pgy_selfhost_select_emitted_c_compile_profile || fail "emitted-C compiler profile is invalid"

WORK_BASE="$ROOT_DIR/.tmp/self_hosted"
mkdir -p "$WORK_BASE"
WORK_BASE="$(cd "$WORK_BASE" && pwd -P)"
WORK_DIR="$(mktemp -d "$WORK_BASE/array-mutation-receiver-use.XXXXXX")"
WORK_DIR="$(cd "$WORK_DIR" && pwd -P)"
[[ "$WORK_DIR" == "$WORK_BASE"/array-mutation-receiver-use.* ]] || fail "evidence path escaped its owner"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
[[ "$WORK_REL" != "$WORK_DIR" ]] || fail "evidence path is not below the repository"
echo "[$LABEL] evidence=$WORK_DIR driver=$DRIVER"
sha256sum "$DRIVER" >"$WORK_DIR/driver.before.sha256"

project() {
    local input="$1" stem="$2" backend="$3" suffix="$4" input_hash status=0
    [[ -s "$WORK_DIR/$input.json" ]] || fail "$input MIR is absent before projection"
    input_hash="$(sha256sum "$WORK_DIR/$input.json" | cut -d' ' -f1)" || fail "$input MIR hash is unreadable"
    [[ "$input_hash" =~ ^[0-9a-f]{64}$ ]] || fail "$input MIR hash is invalid"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$backend.out" 2>"$WORK_DIR/$stem.$backend.err" || status=$?
    [[ "$(sha256sum "$WORK_DIR/$input.json" | cut -d' ' -f1)" == "$input_hash" ]] || \
        fail "$backend rewrote $input MIR"
    return "$status"
}

pgy_selfhost_select_linked_runtime_compile_profile || fail "runtime compiler profile is invalid"
"$CLANG" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
    >"$WORK_DIR/runtime.compile.log" 2>&1 || { cat "$WORK_DIR/runtime.compile.log" >&2; fail "runtime object did not compile; evidence=$WORK_REL"; }
printf 'case\tbackend\troute\tstdout\tproducer_unchanged\n' >"$WORK_DIR/positives.tsv"
positives=(direct_mir_array_mutation_receiver_use_contract array_index_induction_preincrement_read \
    array_index_induction_decrement_break array_index_induction_literal_negative_break \
    array_index_constant_short_circuit)
for name in "${positives[@]}"; do
    source_rel="tests/self_hosted/fixtures/$name.pgy"
    source_hash="$(sha256sum "$ROOT_DIR/$source_rel" | cut -d' ' -f1)"
    (cd "$ROOT_DIR" && timeout 60 "$DRIVER" --emit-mir-json-verified "$source_rel" \
        -o "$WORK_REL/$name.json") >"$WORK_DIR/$name.producer.out" \
        2>"$WORK_DIR/$name.producer.err" || fail "$name source producer refused"
    [[ -s "$WORK_DIR/$name.json" ]] || fail "$name producer did not publish MIR"
    input_hash="$(sha256sum "$WORK_DIR/$name.json" | cut -d' ' -f1)"
    sha256sum "$WORK_DIR/$name.json" >"$WORK_DIR/$name.producer.sha256"
    case "$name" in
        direct_mir_array_mutation_receiver_use_contract) expected='5\n4\n2\n7\n5\n' ;;
        array_index_induction_preincrement_read) expected='zero\none\n' ;;
        array_index_induction_decrement_break|array_index_induction_literal_negative_break) expected='zero\n' ;;
        array_index_constant_short_circuit) expected='false\ntrue\nfalse\ntrue\nshort-circuit-ready\n' ;;
        *) fail "positive fixture has no explicit expected output: $name" ;;
    esac
    printf '%b' "$expected" >"$WORK_DIR/$name.expected"
    for backend in c llvm; do
        suffix=c; [[ "$backend" != llvm ]] || suffix=ll
        project "$name" "$name" "$backend" "$suffix" || fail "$backend refused normal source $name"
        if [[ "$backend" == c ]]; then
            grep -Fq 'pgy_r0_block_' "$WORK_DIR/$name.c" || fail "$name selected a non-general C route"
            command=("$CC" -x c -std=c11 "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" "$WORK_DIR/$name.c")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK_DIR/$name.c"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
        else
            grep -Fq 'pgy.r0.block.' "$WORK_DIR/$name.ll" || fail "$name selected a non-general LLVM route"
            command=("$CLANG" -x ir "$WORK_DIR/$name.ll" -x none "$WORK_DIR/runtime.o" -pthread)
        fi
        command+=(-lm -o "$WORK_DIR/$name-$backend.exe")
        "${command[@]}" >"$WORK_DIR/$name-$backend.compile" 2>&1 || fail "$name $backend did not compile"
        # Only complete normal source programs run. Invalid short-circuit RHSs
        # are never isolated or executed as independent expressions.
        (cd "$ROOT_DIR" && timeout 30 "$WORK_DIR/$name-$backend.exe") | tr -d '\r' >"$WORK_DIR/$name-$backend.run"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$backend.run" || fail "$name $backend output drift"
        [[ "$(sha256sum "$WORK_DIR/$name.json" | cut -d' ' -f1)" == "$input_hash" ]] || fail "$name producer MIR changed"
        printf '%s\t%s\tgeneral\texact\tyes\n' "$name" "$backend" >>"$WORK_DIR/positives.tsv"
    done
    [[ "$(sha256sum "$ROOT_DIR/$source_rel" | cut -d' ' -f1)" == "$source_hash" ]] || fail "$name source changed during the gate"
    echo "[$LABEL] executed=$name C/LLVM general"
done

BASE=direct_mir_array_mutation_receiver_use_contract
BASE_HASH="$(sha256sum "$WORK_DIR/$BASE.json" | cut -d' ' -f1)"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/array_mutation_receiver_use_mutations.py" \
    "$WORK_DIR/$BASE.json" "$WORK_DIR" >"$WORK_DIR/mutations.log" 2>&1 || fail "receiver falsifier generation failed"
"$PYTHON_BIN" - "$WORK_DIR" "$BASE_HASH" <<'PY'
import hashlib
import json
import pathlib
import re
import sys

work = pathlib.Path(sys.argv[1]).resolve()
source = work / "direct_mir_array_mutation_receiver_use_contract.json"
source_bytes = source.read_bytes()
manifest = json.loads((work / "mutations.manifest.json").read_bytes())
cases = manifest.get("cases")
if (manifest.get("schema") != "pgy.array-mutation-receiver-use-falsifiers.v1"
        or manifest.get("source_schema") != "pgy.mir.v1"
        or manifest.get("source_sha256") != sys.argv[2]
        or hashlib.sha256(source_bytes).hexdigest() != sys.argv[2]
        or manifest.get("source_bytes") != len(source_bytes)):
    raise SystemExit("receiver manifest does not join the immutable producer input")
if (not isinstance(cases, list) or len(cases) != 19 or len(set(cases)) != 19
        or any(not isinstance(stem, str) or re.fullmatch(r"receiver-[a-z0-9-]+", stem) is None for stem in cases)
        or (work / "mutations.list").read_text(encoding="utf-8").splitlines() != cases):
    raise SystemExit("receiver manifest/list does not carry 19 unique safe case names")
for stem in cases:
    case = (work / f"{stem}.json").resolve()
    if case.parent != work or not case.is_file() or case.stat().st_size == 0:
        raise SystemExit(f"receiver input is absent before refusal: {stem}")
    if hashlib.sha256(case.read_bytes()).hexdigest() != manifest["case_sha256"].get(stem):
        raise SystemExit(f"receiver input hash disagrees with generator manifest: {stem}")
print("19 immutable receiver inputs join the producer manifest")
PY

printf 'case\tbackend\tstatus\tprior_output\tinput_unchanged\n' >"$WORK_DIR/refusals.tsv"
refusals=0
while IFS= read -r bad; do
    [[ "$bad" =~ ^receiver-[a-z0-9-]+$ ]] || fail "receiver case name is invalid"
    for backend in c llvm; do
        suffix=c; [[ "$backend" != llvm ]] || suffix=ll
        printf 'prior-output:%s:%s\n' "$bad" "$backend" >"$WORK_DIR/$bad.$suffix"
        cp "$WORK_DIR/$bad.$suffix" "$WORK_DIR/$bad.$backend.sentinel"
        status=0
        if project "$bad" "$bad" "$backend" "$suffix"; then
            fail "$backend accepted $bad"
        else
            status=$?
        fi
        cmp -s "$WORK_DIR/$bad.$suffix" "$WORK_DIR/$bad.$backend.sentinel" || fail "$backend replaced the prior output for $bad"
        grep -Eq '(direct MIR|MIR-LOWER ERROR)' "$WORK_DIR/$bad.$backend.out" \
            "$WORK_DIR/$bad.$backend.err" || fail "$backend lost the owned refusal diagnostic for $bad"
        ! grep -Fq 'direct MIR Option match' "$WORK_DIR/$bad.$backend.out" \
            "$WORK_DIR/$bad.$backend.err" || fail "$backend retried $bad as Option"
        printf '%s\t%s\t%s\tpreserved\tyes\n' "$bad" "$backend" "$status" >>"$WORK_DIR/refusals.tsv"
        ((refusals+=1))
    done
done <"$WORK_DIR/mutations.list"
[[ "$refusals" == 38 ]] || fail "receiver refusal matrix is incomplete"
[[ "$(sha256sum "$WORK_DIR/$BASE.json" | cut -d' ' -f1)" == "$BASE_HASH" ]] || fail "receiver producer MIR changed"
sha256sum "$DRIVER" >"$WORK_DIR/driver.after.sha256"
cmp -s "$WORK_DIR/driver.before.sha256" "$WORK_DIR/driver.after.sha256" || fail "driver changed during the gate"
echo "[$LABEL] 5 source-issued general C/LLVM positives; 38 preserved-output receiver refusals; immutable producer MIR"
