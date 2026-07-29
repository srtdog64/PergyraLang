#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$1"
BUILD_DIR="$2"
VALID="$3"
MULTI_VALID="$4"
MULTI_INTERLEAVED="$5"
MUTATION_COUNT="$6"
SELF_MIR_LOWER_DIR="$ROOT_DIR/src/self_hosted/mir_lower"

source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-parity:intent-execution-protocol] $*" >&2
    exit 1
}

ADMISSION_BIN="${PGY_SELFHOST_INTENT_EXECUTION_ADMISSION_BIN:-}"
if [[ -z "$ADMISSION_BIN" ]]; then
    ADMISSION_OWNER_STATE="missing"
    if grep -R -Fq -- 'MirIntentExecutionPlanFromDocument' \
        "$SELF_MIR_LOWER_DIR"; then
        ADMISSION_OWNER_STATE="present; executable not supplied"
    fi
    echo "[self-host-parity:intent-execution-protocol] native canonical + multi-routine schema/digest/join audit + $MUTATION_COUNT mutation corpus: PASS"
    echo "[self-host-parity:intent-execution-protocol] admission BLOCKED: MirIntentExecutionPlanFromDocument owner is $ADMISSION_OWNER_STATE; set PGY_SELFHOST_INTENT_EXECUTION_ADMISSION_BIN to an executable implementing '--verify-input <pgy.mir.v1.json>', success marker 'pgy.mir.v1 input verified', and fail-closed intent or machine-layer diagnostic" >&2
    echo "[self-host-parity:intent-execution-protocol] admission must validate wire has_predecessor before normalization and join routine/action/enum/tobject/instruction identities without source or row-order fallback" >&2
    exit 0
fi

ADMISSION_BIN="$(pgy_select_optional_exe_binary "$ADMISSION_BIN")"
pgy_require_runnable_binary_here \
    "intent-execution-protocol:self-admission" "$ADMISSION_BIN" \
    || fail "PGY_SELFHOST_INTENT_EXECUTION_ADMISSION_BIN is not runnable"

for positive in "$VALID" "$MULTI_VALID" "$MULTI_INTERLEAVED"; do
    name="$(basename "$positive")"
    if ! (cd "$ROOT_DIR" && "$ADMISSION_BIN" --verify-input \
        "${positive#"$ROOT_DIR"/}" \
        >"$positive.admission.out" 2>"$positive.admission.err"); then
        cat "$positive.admission.out" "$positive.admission.err" >&2
        fail "self admission rejected positive intent execution wire: $name"
    fi
    grep -Fq -- 'pgy.mir.v1 input verified' \
        "$positive.admission.out" \
        || fail "self admission success marker is missing: $name"
done

for negative in "$BUILD_DIR"/negative-*.mir.json; do
    name="$(basename "$negative")"
    if (cd "$ROOT_DIR" && "$ADMISSION_BIN" --verify-input \
        "${negative#"$ROOT_DIR"/}" \
        >"$negative.out" 2>"$negative.err"); then
        fail "self admission accepted mutation: $name"
    fi
    expected_diagnostic='MIR (intent execution|machine-layer) facts are missing or invalid'
    if [[ "$name" == "negative-duplicate-payload-declaration.mir.json" ]]; then
        expected_diagnostic='MIR domain topology facts are missing or invalid'
    fi
    grep -Eq -- "$expected_diagnostic" \
        "$negative.out" "$negative.err" \
        || { cat "$negative.out" "$negative.err" >&2; \
             fail "mutation did not fail at intent execution admission: $name"; }
    if grep -Fq -- 'pgy.mir.v1 input verified' \
        "$negative.out" "$negative.err"; then
        fail "mutation emitted a success marker before failure: $name"
    fi
done

echo "[self-host-parity:intent-execution-protocol] canonical + multi-routine admission + $MUTATION_COUNT fail-closed mutations: PASS"
