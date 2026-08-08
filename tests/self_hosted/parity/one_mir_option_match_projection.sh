#!/usr/bin/env bash
# One admitted Option<Int> match CFG drives direct C and textual LLVM.
# Registry forbidden-fallback inventory exercised below:
# raw_match_text_recovery, certificate_raw_json_reopen,
# raw_use_array_backend_read, backend_specific_option_certificate,
# unbound_abi_layout, successor_default, pattern_or_binding_fallback,
# post_issue_certificate_mutation, backend_mir_or_air_read,
# backend_specific_option_plan, unbound_target_fingerprint,
# unverified_abi_projection, both_backend_mappings_in_one_receipt,
# runtime_symbol_guess, native_codegen_fallback, post_issue_plan_mutation,
# backend_local_option_layout, layout_id_without_reconstructible_row,
# tag_or_offset_guess, duplicate_layout_parse,
# post_issue_layout_identity_mutation.
set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 ||
    ! command -v tr >/dev/null 2>&1 ||
    ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-option-match"
DRIVER_BIN="${PGY_SELFHOST_OPTION_MATCH_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
DRIVER_BIN="$(pgy_select_optional_exe_binary "$DRIVER_BIN")"
WORK_DIR="${PGY_SELFHOST_OPTION_MATCH_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_option_match}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/option_match.pgy"
MIR="$WORK_DIR/option_match.one.mir.json"
C_ARTIFACT="$WORK_DIR/option_match.one.c"
LLVM_ARTIFACT="$WORK_DIR/option_match.one.ll"
C_BIN="$WORK_DIR/option_match.c.exe"
LLVM_BIN="$WORK_DIR/option_match.llvm.exe"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_file() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR/"}"; }
root_relative() { pgy_selfhost_path_relative_to_root "$1"; }
hash_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        fail "no SHA-256 tool is available"
    fi
}

assert_owner_ratchet() {
    local owner term
    local owners=(
        "$ROOT_DIR/src/self_hosted/air/mir_option_match_cfg_certificate_fact_owner.pgy"
        "$ROOT_DIR/src/self_hosted/air/mir_option_match_graph_fact_owner.pgy"
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_abi_fact_owner.pgy"
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_abi_capture_owner.pgy"
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_abi_projection_owner.pgy"
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_cfg_plan_owner.pgy"
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_cfg_plan_fact_owner.pgy"
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_cfg_plan_mutation_owner.pgy"
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_cfg_emission_owner.pgy"
        "$ROOT_DIR/src/self_hosted/mir_lower/abi_layout_admission_fact_owner.pgy"
        "$ROOT_DIR/src/self_hosted/mir_lower/match_json_fact_owner.pgy"
        "$ROOT_DIR/src/self_hosted/mir_lower/routine_instruction_match_fact_owner.pgy"
    )
    for owner in "${owners[@]}"; do require_file "$owner"; done
    for term in JsonObject Substring BuildMirDocumentFactIndex \
        MirAbiLayoutRowCaptureWithin CompileMirJsonToCVerified \
        GenerateCFromVerifiedSemanticArtifact; do
        ! grep -Fq -- "$term" \
            "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_cfg_plan_owner.pgy" \
            "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_cfg_emission_owner.pgy" ||
            fail "Option plan/emitter reopened raw or whole compiler path: $term"
    done
    for term in JsonObjectFactTableFromBounds \
        MirMatchInstructionCaptureFromTable; do
        ! grep -Fq -- "$term" \
            "$ROOT_DIR/src/self_hosted/air/mir_option_match_cfg_certificate_fact_owner.pgy" ||
            fail "AIR certificate reopened raw match JSON: $term"
    done
    grep -Fq 'MirRoutineInstructionMatchAtGlobal' \
        "$ROOT_DIR/src/self_hosted/air/mir_option_match_cfg_certificate_fact_owner.pgy" ||
        fail "AIR certificate does not consume indexed typed match facts"
    grep -Fq 'MirCapturedRequiredAbiLayoutRowAdmission' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_cfg_plan_owner.pgy" ||
        fail "Option plan does not consume its bounded ABI admission owner"
    grep -Fq 'DirectMirOptionMatchCfgPlanFromAdmitted' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy" ||
        fail "direct backend does not consume the Option match plan owner"
    grep -Fq 'DirectMirOptionMatchCfgPlanMutationRejected' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_cfg_plan_owner.pgy" ||
        fail "Option plan does not execute repaired-digest negatives"
    grep -Fq 'CompilerTargetCapabilityFingerprint()' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_match_cfg_plan_fact_owner.pgy" ||
        fail "Option plan is not bound to the target capability owner"
}

project() {
    local target="$1" output="$2" stdout stderr
    stdout="$WORK_DIR/project-$target.out"
    stderr="$WORK_DIR/project-$target.err"
    rm -f "$output" "$stdout" "$stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$MIR")" -o "$(root_relative "$output")" \
        >"$stdout" 2>"$stderr") || {
        cat "$stdout" "$stderr" >&2 || true
        fail "$target rejected admitted Option match MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no artifact"
}

reject_mutation() {
    local name="$1" diagnostic="$2" target output stdout stderr
    for target in c llvm; do
        output="$WORK_DIR/$name.$target.artifact"
        stdout="$WORK_DIR/$name.$target.out"
        stderr="$WORK_DIR/$name.$target.err"
        rm -f "$output" "$stdout" "$stderr"
        if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
            "$(root_relative "$WORK_DIR/$name.json")" -o \
            "$(root_relative "$output")" >"$stdout" 2>"$stderr"); then
            fail "$target accepted Option match mutation: $name"
        fi
        [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
        grep -Fq "$diagnostic" "$stdout" "$stderr" || {
            cat "$stdout" "$stderr" >&2 || true
            fail "$target rejection did not distinguish $name"
        }
    done
}

require_file "$SOURCE"
require_file "$DRIVER_BIN"
pgy_require_runnable_binary_here "$LABEL" "$DRIVER_BIN" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM IR compiler: $CLANG"
[[ -n "$PYTHON_BIN" ]] || fail "python3/python is required for typed mutations"
assert_owner_ratchet
mkdir -p "$WORK_DIR"

rm -f "$MIR" "$C_ARTIFACT" "$LLVM_ARTIFACT"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
    "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") ||
    fail "source-to-MIR producer rejected option_match.pgy"
mir_digest="$(hash_file "$MIR")"
project c "$C_ARTIFACT"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project llvm "$LLVM_ARTIFACT"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
! grep -Fq '@pgy_' "$LLVM_ARTIFACT" || fail "direct LLVM reopened Pergyra runtime"

c_command=("$CC" -x c -std=c11 "$C_ARTIFACT")
if pgy_selfhost_emitted_c_uses_runtime_headers "$C_ARTIFACT"; then
    c_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
c_command+=(-o "$C_BIN")
"${c_command[@]}" >"$WORK_DIR/c.compile.log" 2>&1 || fail "C artifact did not compile"
"$CLANG" -x ir "$LLVM_ARTIFACT" -o "$LLVM_BIN" \
    >"$WORK_DIR/llvm.compile.log" 2>&1 || fail "LLVM artifact did not compile"
(cd "$ROOT_DIR" && "$C_BIN") | pgy_selfhost_normalize_text_artifact \
    >"$WORK_DIR/c.run"
(cd "$ROOT_DIR" && "$LLVM_BIN") | pgy_selfhost_normalize_text_artifact \
    >"$WORK_DIR/llvm.run"
printf '42\n42\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" || fail "C output drifted"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/llvm.run" || fail "LLVM output drifted"

"$PYTHON_BIN" - "$MIR" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
source, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
base = json.loads(source.read_text(encoding="utf-8"))
def emit(name, mutate):
    doc = copy.deepcopy(base); mutate(doc["routines"][0]["blocks"])
    (out / f"{name}.json").write_text(
        json.dumps(doc, separators=(",", ":")), encoding="utf-8")
emit("pattern", lambda b: b[2]["instructions"][0].__setitem__("match_patterns", ["Bogus(v)"]))
emit("binding-type", lambda b: b[2]["instructions"][0].__setitem__("match_binding_types", ["Bool"]))
emit("extra-use", lambda b: b[0]["instructions"][1].__setitem__("uses", ["val.1", "val.1"]))
emit("successor", lambda b: b[4].__setitem__("succ_true", 6))
emit("abi-tag", lambda b: b[0]["instructions"][0]["abi_layout"].__setitem__("primary_tag", 1))
emit("abi-offset", lambda b: b[0]["instructions"][0]["abi_layout"]["fields"][1].__setitem__("offset", 0))
emit("abi-layout-id", lambda b: b[0]["instructions"][0].__setitem__("abi_layout_id", 576045278))
PY

reject_mutation pattern "direct MIR Option match typed pattern facts are invalid"
reject_mutation binding-type "direct MIR Option match routine fact owner is invalid"
reject_mutation extra-use "direct MIR Option match residue or exact use is invalid"
reject_mutation successor "direct MIR Option match typed owner inventory is invalid"
reject_mutation abi-tag "direct MIR Option match ABI admission is invalid"
reject_mutation abi-offset "direct MIR Option match ABI admission is invalid"
reject_mutation abi-layout-id "direct MIR Option match ABI admission is invalid"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "negative gates mutated MIR"
echo "[$LABEL] Option<Int> match direct C/LLVM parity and seven mutations are fail-closed (sha256=$mir_digest)"
